/*
 * Copyright 2012 Henri Verbeet for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);

enum wined3d_cs_op
{
    WINED3D_CS_OP_STOP,
};

struct wined3d_cs_stop
{
    enum wined3d_cs_op opcode;
};

static CRITICAL_SECTION wined3d_cs_list_mutex;
static CRITICAL_SECTION_DEBUG wined3d_cs_list_mutex_debug =
{
    0, 0, &wined3d_cs_list_mutex,
    {&wined3d_cs_list_mutex_debug.ProcessLocksList,
    &wined3d_cs_list_mutex_debug.ProcessLocksList},
    0, 0, {(DWORD_PTR)(__FILE__ ": wined3d_cs_list_mutex")}
};
static CRITICAL_SECTION wined3d_cs_list_mutex = {&wined3d_cs_list_mutex_debug, -1, 0, 0, 0, 0};

/* FIXME: The list synchronization probably isn't particularly fast. */
static void wined3d_cs_list_enqueue(struct wined3d_cs_list *list, struct wined3d_cs_block *block)
{
    EnterCriticalSection(&wined3d_cs_list_mutex);
    list_add_tail(&list->blocks, &block->entry);
    LeaveCriticalSection(&wined3d_cs_list_mutex);
}

static struct wined3d_cs_block *wined3d_cs_list_dequeue(struct wined3d_cs_list *list)
{
    struct list *head;

    EnterCriticalSection(&wined3d_cs_list_mutex);
    if (!(head = list_head(&list->blocks)))
    {
        LeaveCriticalSection(&wined3d_cs_list_mutex);
        return NULL;
    }
    list_remove(head);
    LeaveCriticalSection(&wined3d_cs_list_mutex);

    return LIST_ENTRY(head, struct wined3d_cs_block, entry);
}

static struct wined3d_cs_block *wined3d_cs_list_dequeue_blocking(struct wined3d_cs_list *list)
{
    struct wined3d_cs_block *block;

    /* FIXME: Use an event to wait after a couple of spins. */
    for (;;)
    {
        if ((block = wined3d_cs_list_dequeue(list)))
            return block;
    }
}

static void wined3d_cs_list_init(struct wined3d_cs_list *list)
{
    list_init(&list->blocks);
}

static struct wined3d_cs_block *wined3d_cs_get_thread_block(const struct wined3d_cs *cs)
{
    return TlsGetValue(cs->tls_idx);
}

static void wined3d_cs_set_thread_block(const struct wined3d_cs *cs, struct wined3d_cs_block *block)
{
    if (!TlsSetValue(cs->tls_idx, block))
        ERR("Failed to set thread block.\n");
}

static void wined3d_cs_flush(struct wined3d_cs *cs)
{
    wined3d_cs_list_enqueue(&cs->exec_list, wined3d_cs_get_thread_block(cs));
    wined3d_cs_set_thread_block(cs, NULL);
}

static struct wined3d_cs_block *wined3d_cs_get_block(struct wined3d_cs *cs)
{
    struct wined3d_cs_block *block;

    if (!(block = wined3d_cs_list_dequeue(&cs->free_list)))
    {
        if (!(block = HeapAlloc(GetProcessHeap(), 0, sizeof(*block))))
        {
            ERR("Failed to get new block.\n");
            return NULL;
        }
    }

    block->pos = 0;

    return block;
}

static void *wined3d_cs_mt_require_space(struct wined3d_cs *cs, size_t size)
{
    struct wined3d_cs_block *block = wined3d_cs_get_thread_block(cs);
    void *data;

    if (!block || block->pos + size > sizeof(block->data))
    {
        if (block)
            wined3d_cs_flush(cs);
        block = wined3d_cs_get_block(cs);
        wined3d_cs_set_thread_block(cs, block);
    }

    data = &block->data[block->pos];
    block->pos += size;

    return data;
}

/* FIXME: wined3d_device_uninit_3d() should either flush and wait, or be an
 * OP itself. */
static void wined3d_cs_emit_stop(struct wined3d_cs *cs)
{
    struct wined3d_cs_stop *op;

    op = wined3d_cs_mt_require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_STOP;

    wined3d_cs_flush(cs);
}

static DWORD WINAPI wined3d_cs_run(void *thread_param)
{
    struct wined3d_cs *cs = thread_param;

    TRACE("Started.\n");

    for (;;)
    {
        struct wined3d_cs_block *block;
        enum wined3d_cs_op opcode;

        block = wined3d_cs_list_dequeue_blocking(&cs->exec_list);
        opcode = *(const enum wined3d_cs_op *)block->data;

        if (opcode > WINED3D_CS_OP_STOP)
            ERR("Invalid opcode %#x.\n", opcode);

        wined3d_cs_list_enqueue(&cs->free_list, block);

        goto done;
    }

done:
    TRACE("Stopped.\n");
    return 0;
}

/* We could also create a single thread for all of wined3d, instead of one for
 * each device, at the cost of some extra overhead for each block. I'm not
 * sure that we'd gain anything from that though. */
struct wined3d_cs *wined3d_cs_create(void)
{
    struct wined3d_cs *cs;
    DWORD ret;

    if (!(cs = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cs))))
    {
        ERR("Failed to allocate wined3d command stream memory.\n");
        return NULL;
    }

    if ((cs->tls_idx = TlsAlloc()) == TLS_OUT_OF_INDEXES)
    {
        ERR("Failed to allocate cs TLS index, err %#x.\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, cs);
        return NULL;
    }

    if (wined3d_settings.cs_multithreaded)
    {
        wined3d_cs_list_init(&cs->free_list);
        wined3d_cs_list_init(&cs->exec_list);

        if (!(cs->thread = CreateThread(NULL, 0, wined3d_cs_run, cs, 0, &ret)))
        {
            ERR("Failed to create wined3d command stream thread.\n");
            if (!TlsFree(cs->tls_idx))
                ERR("Failed to free cs TLS index, err %#x.\n", GetLastError());
            HeapFree(GetProcessHeap(), 0, cs);
            return NULL;
        }
    }

    return cs;
}

void wined3d_cs_destroy(struct wined3d_cs *cs)
{
    DWORD ret;

    if (wined3d_settings.cs_multithreaded)
    {
        wined3d_cs_emit_stop(cs);

        ret = WaitForSingleObject(cs->thread, INFINITE);
        CloseHandle(cs->thread);
        if (ret != WAIT_OBJECT_0)
            ERR("Wait failed (%#x).\n", ret);

        /* FIXME: Cleanup the block lists on thread exit. */
#if 0
        wined3d_cs_list_cleanup(&cs->exec_list);
        wined3d_cs_list_cleanup(&cs->free_list);
#endif
    }

    if (!TlsFree(cs->tls_idx))
        ERR("Failed to free cs TLS index, err %#x.\n", GetLastError());

    HeapFree(GetProcessHeap(), 0, cs);
}
