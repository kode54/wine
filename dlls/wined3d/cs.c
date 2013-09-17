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
    WINED3D_CS_OP_NOP,
    WINED3D_CS_OP_SKIP,
    WINED3D_CS_OP_FENCE,
    WINED3D_CS_OP_PRESENT,
    WINED3D_CS_OP_CLEAR,
    WINED3D_CS_OP_DRAW,
    WINED3D_CS_OP_SET_RENDER_TARGET,
    WINED3D_CS_OP_SET_VS_CONSTS_F,
    WINED3D_CS_OP_SET_VS_CONSTS_B,
    WINED3D_CS_OP_SET_VS_CONSTS_I,
    WINED3D_CS_OP_SET_PS_CONSTS_F,
    WINED3D_CS_OP_SET_PS_CONSTS_B,
    WINED3D_CS_OP_SET_PS_CONSTS_I,
    WINED3D_CS_OP_RESET_STATE,
    WINED3D_CS_OP_GLFINISH,
    WINED3D_CS_OP_SET_VIEWPORT,
    WINED3D_CS_OP_SET_SCISSOR_RECT,
    WINED3D_CS_OP_SET_DEPTH_STENCIL,
    WINED3D_CS_OP_SET_VERTEX_DECLARATION,
    WINED3D_CS_OP_SET_STREAM_SOURCE,
    WINED3D_CS_OP_SET_STREAM_SOURCE_FREQ,
    WINED3D_CS_OP_SET_INDEX_BUFFER,
    WINED3D_CS_OP_SET_TEXTURE,
    WINED3D_CS_OP_SET_VERTEX_SHADER,
    WINED3D_CS_OP_SET_PIXEL_SHADER,
    WINED3D_CS_OP_SET_GEOMETRY_SHADER,
    WINED3D_CS_OP_SET_RENDER_STATE,
    WINED3D_CS_OP_SET_TEXTURE_STATE,
    WINED3D_CS_OP_SET_SAMPLER_STATE,
    WINED3D_CS_OP_SET_TRANSFORM,
    WINED3D_CS_OP_SET_CLIP_PLANE,
    WINED3D_CS_OP_SET_MATERIAL,
    WINED3D_CS_OP_SET_BASE_VERTEX_INDEX,
    WINED3D_CS_OP_SET_PRIMITIVE_TYPE,
    WINED3D_CS_OP_SET_VS_CB,
    WINED3D_CS_OP_SET_PS_CB,
    WINED3D_CS_OP_SET_GS_CB,
    WINED3D_CS_OP_SET_VS_SAMPLER,
    WINED3D_CS_OP_SET_PS_SAMPLER,
    WINED3D_CS_OP_SET_GS_SAMPLER,
    WINED3D_CS_OP_SET_STREAM_OUTPUT,
    WINED3D_CS_OP_SET_LIGHT,
    WINED3D_CS_OP_SET_LIGHT_ENABLE,
    WINED3D_CS_OP_BLT,
    WINED3D_CS_OP_COLOR_FILL,
    WINED3D_CS_OP_SURFACE_MAP,
    WINED3D_CS_OP_SURFACE_UNMAP,
    WINED3D_CS_OP_SWAP_MEM,
    WINED3D_CS_OP_BUFFER_INVALIDATE_RANGE,
    WINED3D_CS_OP_QUERY_ISSUE,
    WINED3D_CS_OP_QUERY_GET_DATA,
    WINED3D_CS_OP_STOP,
};

struct wined3d_cs_stop
{
    enum wined3d_cs_op opcode;
};

struct wined3d_cs_fence
{
    enum wined3d_cs_op opcode;
    BOOL *signalled;
};

#define CS_PRESENT_SRC_RECT 1
#define CS_PRESENT_DST_RECT 2
#define CS_PRESENT_DIRTY_RGN 4
struct wined3d_cs_present
{
    enum wined3d_cs_op opcode;
    HWND dst_window_override;
    struct wined3d_swapchain *swapchain;
    RECT src_rect;
    RECT dst_rect;
    RGNDATA dirty_region;
    DWORD flags;
    DWORD set_data;
};

struct wined3d_cs_clear
{
    enum wined3d_cs_op opcode;
    DWORD rect_count;
    DWORD flags;
    struct wined3d_color color;
    float depth;
    DWORD stencil;
    RECT rects[1];
};

struct wined3d_cs_draw
{
    enum wined3d_cs_op opcode;
    UINT start_idx;
    UINT index_count;
    UINT start_instance;
    UINT instance_count;
    BOOL indexed;
};

struct wined3d_cs_set_render_target
{
    enum wined3d_cs_op opcode;
    UINT render_target_idx;
    struct wined3d_surface *render_target;
};

struct wined3d_cs_set_consts_f
{
    enum wined3d_cs_op opcode;
    UINT start_register, vector4f_count;
    float constants[4];
};

struct wined3d_cs_set_consts_b
{
    enum wined3d_cs_op opcode;
    UINT start_register, bool_count;
    BOOL constants[1];
};

struct wined3d_cs_set_consts_i
{
    enum wined3d_cs_op opcode;
    UINT start_register, vector4i_count;
    int constants[4];
};

struct wined3d_cs_reset_state
{
    enum wined3d_cs_op opcode;
};

struct wined3d_cs_finish
{
    enum wined3d_cs_op opcode;
};

struct wined3d_cs_set_viewport
{
    enum wined3d_cs_op opcode;
    struct wined3d_viewport viewport;
};

struct wined3d_cs_set_scissor_rect
{
    enum wined3d_cs_op opcode;
    RECT rect;
};

struct wined3d_cs_set_depth_stencil
{
    enum wined3d_cs_op opcode;
    struct wined3d_surface *depth_stencil;
};

struct wined3d_cs_set_vertex_declaration
{
    enum wined3d_cs_op opcode;
    struct wined3d_vertex_declaration *declaration;
};

struct wined3d_cs_set_stream_source
{
    enum wined3d_cs_op opcode;
    UINT stream_idx;
    struct wined3d_buffer *buffer;
    UINT offset;
    UINT stride;
};

struct wined3d_cs_set_stream_source_freq
{
    enum wined3d_cs_op opcode;
    UINT stream_idx;
    UINT frequency;
    UINT flags;
};

struct wined3d_cs_set_index_buffer
{
    enum wined3d_cs_op opcode;
    struct wined3d_buffer *buffer;
    enum wined3d_format_id format_id;
};

struct wined3d_cs_set_texture
{
    enum wined3d_cs_op opcode;
    UINT stage;
    struct wined3d_texture *texture;
};

struct wined3d_cs_set_shader
{
    enum wined3d_cs_op opcode;
    struct wined3d_shader *shader;
};

struct wined3d_cs_set_render_state
{
    enum wined3d_cs_op opcode;
    enum wined3d_render_state state;
    DWORD value;
};

struct wined3d_cs_set_texture_state
{
    enum wined3d_cs_op opcode;
    UINT stage;
    enum wined3d_texture_stage_state state;
    DWORD value;
};

struct wined3d_cs_set_sampler_state
{
    enum wined3d_cs_op opcode;
    UINT sampler_idx;
    enum wined3d_sampler_state state;
    DWORD value;
};

struct wined3d_cs_set_transform
{
    enum wined3d_cs_op opcode;
    enum wined3d_transform_state state;
    struct wined3d_matrix matrix;
};

struct wined3d_cs_set_clip_plane
{
    enum wined3d_cs_op opcode;
    UINT plane_idx;
    struct wined3d_vec4 plane;
};

struct wined3d_cs_set_material
{
    enum wined3d_cs_op opcode;
    struct wined3d_material material;
};

struct wined3d_cs_set_base_vertex_index
{
    enum wined3d_cs_op opcode;
    UINT base_vertex_index;
};

struct wined3d_cs_set_primitive_type
{
    enum wined3d_cs_op opcode;
    GLenum gl_primitive_type;
};

struct wined3d_cs_set_cb
{
    enum wined3d_cs_op opcode;
    UINT idx;
    struct wined3d_buffer *buffer;
};

struct wined3d_cs_set_sampler
{
    enum wined3d_cs_op opcode;
    UINT idx;
    struct wined3d_sampler *sampler;
};

struct wined3d_cs_set_stream_output
{
    enum wined3d_cs_op opcode;
    UINT idx, offset;
    struct wined3d_buffer *buffer;
};

struct wined3d_cs_set_light
{
    enum wined3d_cs_op opcode;
    struct wined3d_light_info light;
};

struct wined3d_cs_set_light_enable
{
    enum wined3d_cs_op opcode;
    UINT idx;
    BOOL enable;
};

struct wined3d_cs_blt
{
    enum wined3d_cs_op opcode;
    struct wined3d_surface *dst_surface;
    RECT dst_rect;
    struct wined3d_surface *src_surface;
    RECT src_rect;
    DWORD flags;
    WINEDDBLTFX fx;
    enum wined3d_texture_filter_type filter;
};

struct wined3d_cs_color_fill
{
    enum wined3d_cs_op opcode;
    struct wined3d_surface *surface;
    RECT rect;
    struct wined3d_color color;
};

struct wined3d_cs_surface_map
{
    enum wined3d_cs_op opcode;
    struct wined3d_surface *surface;
    RECT rect;
    DWORD flags;
    BOOL has_rect;
};

struct wined3d_cs_surface_unmap
{
    enum wined3d_cs_op opcode;
    struct wined3d_surface *surface;
};

struct wined3d_cs_skip
{
    enum wined3d_cs_op opcode;
    DWORD size;
};

struct wined3d_cs_swap_mem
{
    enum wined3d_cs_op opcode;
    struct wined3d_buffer *buffer;
    BYTE *mem;
};

struct wined3d_cs_buffer_invalidate_bo_range
{
    enum wined3d_cs_op opcode;
    struct wined3d_buffer *buffer;
    UINT offset, size;
};

struct wined3d_cs_query_issue
{
    enum wined3d_cs_op opcode;
    struct wined3d_query *query;
    DWORD flags;
};

struct wined3d_cs_query_get_data
{
    enum wined3d_cs_op opcode;
    struct wined3d_query *query;
    void *data;
    UINT data_size;
    DWORD flags;
    HRESULT *ret;
};

static void wined3d_cs_mt_submit(struct wined3d_cs *cs, size_t size)
{
    LONG new_val = (cs->queue.head + size) & (WINED3D_CS_QUEUE_SIZE - 1);
    /* There is only one thread writing to queue.head, InterlockedExchange
     * is used for the memory barrier. */
    InterlockedExchange(&cs->queue.head, new_val);
}

static void wined3d_cs_mt_submit_prio(struct wined3d_cs *cs, size_t size)
{
    LONG new_val = (cs->prio_queue.head + size) & (WINED3D_CS_QUEUE_SIZE - 1);
    /* There is only one thread writing to queue.head, InterlockedExchange
     * is used for the memory barrier. */
    InterlockedExchange(&cs->prio_queue.head, new_val);
}

static UINT wined3d_cs_exec_nop(struct wined3d_cs *cs, const void *data)
{
    return sizeof(enum wined3d_cs_op);
}

static UINT wined3d_cs_exec_skip(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_skip *op = data;

    return op->size;
}

static UINT wined3d_cs_exec_fence(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_fence *op = data;

    InterlockedExchange(op->signalled, TRUE);

    return sizeof(*op);
}

static void wined3d_cs_emit_fence(struct wined3d_cs *cs, BOOL *signalled)
{
    struct wined3d_cs_fence *op;

    *signalled = FALSE;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_FENCE;
    op->signalled = signalled;
    cs->ops->submit(cs, sizeof(*op));
}

static void wined3d_cs_emit_fence_prio(struct wined3d_cs *cs, BOOL *signalled)
{
    struct wined3d_cs_fence *op;

    *signalled = FALSE;

    op = cs->ops->require_space_prio(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_FENCE;
    op->signalled = signalled;
    cs->ops->submit_prio(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_present(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_present *op = data;
    struct wined3d_swapchain *swapchain;
    const RECT *src_rect = op->set_data & CS_PRESENT_SRC_RECT ? &op->src_rect : NULL;
    const RECT *dst_rect = op->set_data & CS_PRESENT_DST_RECT ? &op->dst_rect : NULL;
    const RGNDATA *dirty_region = op->set_data & CS_PRESENT_DIRTY_RGN ? &op->dirty_region : NULL;

    swapchain = op->swapchain;
    wined3d_swapchain_set_window(swapchain, op->dst_window_override);

    swapchain->swapchain_ops->swapchain_present(swapchain,
            src_rect, dst_rect, dirty_region, op->flags,
            cs->state.fb.depth_stencil);

    InterlockedDecrement(&cs->pending_presents);

    return sizeof(*op);
}

void wined3d_cs_emit_present(struct wined3d_cs *cs, struct wined3d_swapchain *swapchain,
        const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override,
        const RGNDATA *dirty_region, DWORD flags)
{
    struct wined3d_cs_present *op;
    LONG pending;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_PRESENT;
    op->dst_window_override = dst_window_override;
    op->swapchain = swapchain;
    op->set_data = 0;
    if (src_rect)
    {
        op->src_rect = *src_rect;
        op->set_data |= CS_PRESENT_SRC_RECT;
    }
    if (dst_rect)
    {
        op->dst_rect = *dst_rect;
        op->set_data |= CS_PRESENT_DST_RECT;
    }
    if (dirty_region)
    {
        op->dirty_region = *dirty_region;
        op->set_data = CS_PRESENT_DIRTY_RGN;
    }
    op->flags = flags;

    pending = InterlockedIncrement(&cs->pending_presents);

    cs->ops->submit(cs, sizeof(*op));

    while (pending > 1)
        pending = InterlockedCompareExchange(&cs->pending_presents, 0, 0);
}

static UINT wined3d_cs_exec_clear(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_clear *op = data;
    struct wined3d_device *device = cs->device;
    const struct wined3d_fb_state *fb = &cs->state.fb;
    RECT draw_rect;
    unsigned int extra_rects = op->rect_count ? op->rect_count - 1 : 0;

    wined3d_get_draw_rect(&cs->state, &draw_rect);
    device_clear_render_targets(device, device->adapter->gl_info.limits.buffers,
            fb, op->rect_count, op->rect_count ? op->rects : NULL, &draw_rect, op->flags,
            &op->color, op->depth, op->stencil);

    return sizeof(*op) + sizeof(*op->rects) * extra_rects;
}

void wined3d_cs_emit_clear(struct wined3d_cs *cs, DWORD rect_count, const RECT *rects,
        DWORD flags, const struct wined3d_color *color, float depth, DWORD stencil)
{
    struct wined3d_cs_clear *op;
    unsigned int extra_rects = rect_count ? rect_count - 1 : 0;
    size_t size = sizeof(*op) + sizeof(*op->rects) * extra_rects;

    op = cs->ops->require_space(cs, size);
    op->opcode = WINED3D_CS_OP_CLEAR;
    op->rect_count = rect_count;
    if (rect_count)
        memcpy(op->rects, rects, rect_count * sizeof(*rects));
    op->flags = flags;
    op->color = *color;
    op->depth = depth;
    op->stencil = stencil;

    cs->ops->submit(cs, size);
}

static UINT wined3d_cs_exec_draw(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_draw *op = data;
    const struct wined3d_gl_info *gl_info = &cs->device->adapter->gl_info;

    if (op->indexed && !gl_info->supported[ARB_DRAW_ELEMENTS_BASE_VERTEX])
    {
        if (cs->state.load_base_vertex_index != cs->state.base_vertex_index)
        {
            cs->state.load_base_vertex_index = cs->state.base_vertex_index;
            device_invalidate_state(cs->device, STATE_BASEVERTEXINDEX);
        }
    }
    else if (cs->state.load_base_vertex_index)
    {
        cs->state.load_base_vertex_index = 0;
        device_invalidate_state(cs->device, STATE_BASEVERTEXINDEX);
    }

    draw_primitive(cs->device, &cs->state, op->start_idx, op->index_count,
            op->start_instance, op->instance_count, op->indexed);

    return sizeof(*op);
}

void wined3d_cs_emit_draw(struct wined3d_cs *cs, UINT start_idx, UINT index_count,
        UINT start_instance, UINT instance_count, BOOL indexed)
{
    struct wined3d_cs_draw *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_DRAW;
    op->start_idx = start_idx;
    op->index_count = index_count;
    op->start_instance = start_instance;
    op->instance_count = instance_count;
    op->indexed = indexed;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_render_target(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_render_target *op = data;

    cs->state.fb.render_targets[op->render_target_idx] = op->render_target;

    device_invalidate_state(cs->device, STATE_FRAMEBUFFER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_render_target(struct wined3d_cs *cs, UINT render_target_idx,
        struct wined3d_surface *render_target)
{
    struct wined3d_cs_set_render_target *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_RENDER_TARGET;
    op->render_target_idx = render_target_idx;
    op->render_target = render_target;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_vs_consts_f(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_f *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(cs->state.vs_consts_f + op->start_register * 4, op->constants,
            sizeof(*cs->state.vs_consts_f) * 4 * op->vector4f_count);

    device->shader_backend->shader_update_float_vertex_constants(device,
            op->start_register, op->vector4f_count);

    return sizeof(*op) + sizeof(op->constants) * (op->vector4f_count - 1);
}

static UINT wined3d_cs_exec_set_ps_consts_f(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_f *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(cs->state.ps_consts_f + op->start_register * 4, op->constants,
            sizeof(*cs->state.ps_consts_f) * 4 * op->vector4f_count);

    device->shader_backend->shader_update_float_pixel_constants(device,
            op->start_register, op->vector4f_count);

    return sizeof(*op) + sizeof(op->constants) * (op->vector4f_count - 1);
}

void wined3d_cs_emit_set_consts_f(struct wined3d_cs *cs, UINT start_register,
        const float *constants, UINT vector4f_count, enum wined3d_shader_type type)
{
    struct wined3d_cs_set_consts_f *op;
    UINT extra_space = vector4f_count - 1;
    size_t size = sizeof(*op) + sizeof(op->constants) * extra_space;

    op = cs->ops->require_space(cs, size);
    switch (type)
    {
        case WINED3D_SHADER_TYPE_PIXEL:
            op->opcode = WINED3D_CS_OP_SET_PS_CONSTS_F;
            break;

        case WINED3D_SHADER_TYPE_VERTEX:
            op->opcode = WINED3D_CS_OP_SET_VS_CONSTS_F;
            break;

        case WINED3D_SHADER_TYPE_GEOMETRY:
            ERR("Invalid for geometry shaders\n");
            return;
    }
    op->start_register = start_register;
    op->vector4f_count = vector4f_count;
    memcpy(op->constants, constants, sizeof(*constants) * 4 * vector4f_count);

    cs->ops->submit(cs, size);
}

static UINT wined3d_cs_exec_set_vs_consts_b(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_b *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(&cs->state.vs_consts_b[op->start_register], op->constants,
            sizeof(*cs->state.vs_consts_b) * op->bool_count);

    device_invalidate_shader_constants(device, WINED3D_SHADER_CONST_VS_B);

    return sizeof(*op) + sizeof(op->constants) * (op->bool_count - 1);
}

static UINT wined3d_cs_exec_set_ps_consts_b(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_b *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(&cs->state.ps_consts_b[op->start_register], op->constants,
            sizeof(*cs->state.ps_consts_b) * op->bool_count);

    device_invalidate_shader_constants(device, WINED3D_SHADER_CONST_PS_B);

    return sizeof(*op) + sizeof(op->constants) * (op->bool_count - 1);
}

void wined3d_cs_emit_set_consts_b(struct wined3d_cs *cs, UINT start_register,
        const BOOL *constants, UINT bool_count, enum wined3d_shader_type type)
{
    struct wined3d_cs_set_consts_b *op;
    UINT extra_space = bool_count - 1;
    size_t size = sizeof(*op) + sizeof(op->constants) * extra_space;

    op = cs->ops->require_space(cs, size);
    switch (type)
    {
        case WINED3D_SHADER_TYPE_PIXEL:
            op->opcode = WINED3D_CS_OP_SET_PS_CONSTS_B;
            break;

        case WINED3D_SHADER_TYPE_VERTEX:
            op->opcode = WINED3D_CS_OP_SET_VS_CONSTS_B;
            break;

        case WINED3D_SHADER_TYPE_GEOMETRY:
            ERR("Invalid for geometry shaders\n");
            return;
    }
    op->start_register = start_register;
    op->bool_count = bool_count;
    memcpy(op->constants, constants, sizeof(op->constants) * bool_count);

    cs->ops->submit(cs, size);
}

static UINT wined3d_cs_exec_set_vs_consts_i(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_i *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(&cs->state.vs_consts_i[op->start_register], op->constants,
            sizeof(*cs->state.vs_consts_i) * 4 * op->vector4i_count);

    device_invalidate_shader_constants(device, WINED3D_SHADER_CONST_VS_I);

    return sizeof(*op) + sizeof(op->constants) * (op->vector4i_count - 1);
}

static UINT wined3d_cs_exec_set_ps_consts_i(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_i *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(&cs->state.ps_consts_i[op->start_register], op->constants,
            sizeof(*cs->state.ps_consts_i) * 4 * op->vector4i_count);

    device_invalidate_shader_constants(device, WINED3D_SHADER_CONST_PS_I);

    return sizeof(*op) + sizeof(op->constants) * (op->vector4i_count - 1);
}

void wined3d_cs_emit_set_consts_i(struct wined3d_cs *cs, UINT start_register,
        const int *constants, UINT vector4i_count, enum wined3d_shader_type type)
{
    struct wined3d_cs_set_consts_i *op;
    UINT extra_space = vector4i_count - 1;
    size_t size = sizeof(*op) + sizeof(op->constants) * extra_space;

    op = cs->ops->require_space(cs, size);
    switch (type)
    {
        case WINED3D_SHADER_TYPE_PIXEL:
            op->opcode = WINED3D_CS_OP_SET_PS_CONSTS_I;
            break;

        case WINED3D_SHADER_TYPE_VERTEX:
            op->opcode = WINED3D_CS_OP_SET_VS_CONSTS_I;
            break;

        case WINED3D_SHADER_TYPE_GEOMETRY:
            FIXME("Invalid for geometry shaders\n");
            return;
    }
    op->start_register = start_register;
    op->vector4i_count = vector4i_count;
    memcpy(op->constants, constants, sizeof(op->constants) * vector4i_count);

    cs->ops->submit(cs, size);
}

static UINT wined3d_cs_exec_reset_state(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_reset_state *op = data;

    state_init_default(&cs->state, cs->device);

    return sizeof(*op);
}

void wined3d_cs_emit_reset_state(struct wined3d_cs *cs)
{
    struct wined3d_cs_reset_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_RESET_STATE;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_glfinish(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_reset_state *op = data;
    struct wined3d_device *device = cs->device;
    struct wined3d_context *context;

    if (!device->d3d_initialized)
        return sizeof(*op);

    context = context_acquire(device, NULL);
    context->gl_info->gl_ops.gl.p_glFinish();
    context_release(context);

    return sizeof(*op);
}

void wined3d_cs_emit_glfinish(struct wined3d_cs *cs)
{
    struct wined3d_cs_reset_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_GLFINISH;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_viewport(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_viewport *op = data;
    struct wined3d_device *device = cs->device;

    cs->state.viewport = op->viewport;
    device_invalidate_state(device, STATE_VIEWPORT);

    return sizeof(*op);
}

void wined3d_cs_emit_set_viewport(struct wined3d_cs *cs, const struct wined3d_viewport *vp)
{
    struct wined3d_cs_set_viewport *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_VIEWPORT;
    op->viewport = *vp;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_scissor_rect(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_scissor_rect *op = data;
    struct wined3d_device *device = cs->device;

    cs->state.scissor_rect = op->rect;
    device_invalidate_state(device, STATE_SCISSORRECT);

    return sizeof(*op);
}

void wined3d_cs_emit_set_scissor_rect(struct wined3d_cs *cs, const RECT *rect)
{
    struct wined3d_cs_set_scissor_rect *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_SCISSOR_RECT;
    op->rect = *rect;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_depth_stencil(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_depth_stencil *op = data;
    struct wined3d_surface *prev = cs->state.fb.depth_stencil;
    struct wined3d_device *device = cs->device;

    if (prev)
    {
        if (device->swapchains[0]->desc.flags & WINED3DPRESENTFLAG_DISCARD_DEPTHSTENCIL
                || prev->flags & SFLAG_DISCARD)
        {
            surface_modify_ds_location(prev, SFLAG_DISCARDED,
                    prev->resource.width, prev->resource.height);
            if (prev == cs->onscreen_depth_stencil)
            {
                wined3d_surface_decref(cs->onscreen_depth_stencil);
                cs->onscreen_depth_stencil = NULL;
            }
        }
    }

    cs->state.fb.depth_stencil = op->depth_stencil;

    if (!prev != !op->depth_stencil)
    {
        /* Swapping NULL / non NULL depth stencil affects the depth and tests */
        device_invalidate_state(device, STATE_RENDER(WINED3D_RS_ZENABLE));
        device_invalidate_state(device, STATE_RENDER(WINED3D_RS_STENCILENABLE));
        device_invalidate_state(device, STATE_RENDER(WINED3D_RS_STENCILWRITEMASK));
        device_invalidate_state(device, STATE_RENDER(WINED3D_RS_DEPTHBIAS));
    }
    else if (prev && prev->resource.format->depth_size != op->depth_stencil->resource.format->depth_size)
    {
        device_invalidate_state(device, STATE_RENDER(WINED3D_RS_DEPTHBIAS));
    }

    device_invalidate_state(device, STATE_FRAMEBUFFER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_depth_stencil(struct wined3d_cs *cs, struct wined3d_surface *depth_stencil)
{
    struct wined3d_cs_set_depth_stencil *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_DEPTH_STENCIL;
    op->depth_stencil = depth_stencil;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_vertex_declaration(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_vertex_declaration *op = data;

    cs->state.vertex_declaration = op->declaration;
    device_invalidate_state(cs->device, STATE_VDECL);

    return sizeof(*op);
}

void wined3d_cs_emit_set_vertex_declaration(struct wined3d_cs *cs,
        struct wined3d_vertex_declaration *declaration)
{
    struct wined3d_cs_set_vertex_declaration *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_VERTEX_DECLARATION;
    op->declaration = declaration;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_stream_source(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_stream_source *op = data;
    struct wined3d_stream_state *stream;
    struct wined3d_buffer *prev;

    stream = &cs->state.streams[op->stream_idx];
    prev = stream->buffer;
    stream->buffer = op->buffer;
    stream->offset = op->offset;
    stream->stride = op->stride;

    if (op->buffer)
        InterlockedIncrement(&op->buffer->resource.bind_count);

    if (prev)
        InterlockedDecrement(&prev->resource.bind_count);

    device_invalidate_state(cs->device, STATE_STREAMSRC);

    return sizeof(*op);
}

void wined3d_cs_emit_set_stream_source(struct wined3d_cs *cs, UINT stream_idx,
        struct wined3d_buffer *buffer, UINT offset, UINT stride)
{
    struct wined3d_cs_set_stream_source *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_STREAM_SOURCE;
    op->stream_idx = stream_idx;
    op->buffer = buffer;
    op->offset = offset;
    op->stride = stride;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_stream_source_freq(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_stream_source_freq *op = data;
    struct wined3d_stream_state *stream;

    stream = &cs->state.streams[op->stream_idx];
    stream->frequency = op->frequency;
    stream->flags = op->flags;

    device_invalidate_state(cs->device, STATE_STREAMSRC);

    return sizeof(*op);
}

void wined3d_cs_emit_set_stream_source_freq(struct wined3d_cs *cs, UINT stream_idx, UINT frequency, UINT flags)
{
    struct wined3d_cs_set_stream_source_freq *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_STREAM_SOURCE_FREQ;
    op->stream_idx = stream_idx;
    op->frequency = frequency;
    op->flags = flags;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_index_buffer(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_index_buffer *op = data;
    struct wined3d_buffer *prev;

    prev = cs->state.index_buffer;
    cs->state.index_buffer = op->buffer;
    cs->state.index_format = op->format_id;

    if (op->buffer)
        InterlockedIncrement(&op->buffer->resource.bind_count);

    if (prev)
        InterlockedDecrement(&prev->resource.bind_count);

    device_invalidate_state(cs->device, STATE_INDEXBUFFER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_index_buffer(struct wined3d_cs *cs, struct wined3d_buffer *buffer,
        enum wined3d_format_id format_id)
{
    struct wined3d_cs_set_index_buffer *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_INDEX_BUFFER;
    op->buffer = buffer;
    op->format_id = format_id;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_texture(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_d3d_info *d3d_info = &cs->device->adapter->d3d_info;
    const struct wined3d_cs_set_texture *op = data;
    struct wined3d_texture *prev;

    prev = cs->state.textures[op->stage];
    cs->state.textures[op->stage] = op->texture;

    if (op->texture)
    {
        if (InterlockedIncrement(&op->texture->resource.bind_count) == 1)
            op->texture->sampler = op->stage;

        if (!prev || op->texture->target != prev->target)
            device_invalidate_state(cs->device, STATE_PIXELSHADER);

        if (!prev && op->stage < d3d_info->limits.ffp_blend_stages)
        {
            /* The source arguments for color and alpha ops have different
             * meanings when a NULL texture is bound, so the COLOR_OP and
             * ALPHA_OP have to be dirtified. */
            device_invalidate_state(cs->device, STATE_TEXTURESTAGE(op->stage, WINED3D_TSS_COLOR_OP));
            device_invalidate_state(cs->device, STATE_TEXTURESTAGE(op->stage, WINED3D_TSS_ALPHA_OP));
        }
    }

    if (prev)
    {
        if (InterlockedDecrement(&prev->resource.bind_count) && prev->sampler == op->stage)
        {
            unsigned int i;

            /* Search for other stages the texture is bound to. Shouldn't
             * happen if applications bind textures to a single stage only. */
            TRACE("Searching for other stages the texture is bound to.\n");
            for (i = 0; i < MAX_COMBINED_SAMPLERS; ++i)
            {
                if (cs->state.textures[i] == prev)
                {
                    TRACE("Texture is also bound to stage %u.\n", i);
                    prev->sampler = i;
                    break;
                }
            }
        }

        if (!op->texture && op->stage < d3d_info->limits.ffp_blend_stages)
        {
            device_invalidate_state(cs->device, STATE_TEXTURESTAGE(op->stage, WINED3D_TSS_COLOR_OP));
            device_invalidate_state(cs->device, STATE_TEXTURESTAGE(op->stage, WINED3D_TSS_ALPHA_OP));
        }
    }

    device_invalidate_state(cs->device, STATE_SAMPLER(op->stage));

    return sizeof(*op);
}

void wined3d_cs_emit_set_texture(struct wined3d_cs *cs, UINT stage, struct wined3d_texture *texture)
{
    struct wined3d_cs_set_texture *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_TEXTURE;
    op->stage = stage;
    op->texture = texture;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_vertex_shader(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_shader *op = data;

    cs->state.vertex_shader = op->shader;
    device_invalidate_state(cs->device, STATE_VSHADER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_vertex_shader(struct wined3d_cs *cs, struct wined3d_shader *shader)
{
    struct wined3d_cs_set_shader *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_VERTEX_SHADER;
    op->shader = shader;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_pixel_shader(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_shader *op = data;

    cs->state.pixel_shader = op->shader;
    device_invalidate_state(cs->device, STATE_PIXELSHADER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_pixel_shader(struct wined3d_cs *cs, struct wined3d_shader *shader)
{
    struct wined3d_cs_set_shader *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_PIXEL_SHADER;
    op->shader = shader;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_geometry_shader(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_shader *op = data;

    cs->state.geometry_shader = op->shader;
    device_invalidate_state(cs->device, STATE_GEOMETRY_SHADER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_geometry_shader(struct wined3d_cs *cs, struct wined3d_shader *shader)
{
    struct wined3d_cs_set_shader *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_GEOMETRY_SHADER;
    op->shader = shader;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_render_state(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_render_state *op = data;

    cs->state.render_states[op->state] = op->value;
    device_invalidate_state(cs->device, STATE_RENDER(op->state));

    return sizeof(*op);
}

void wined3d_cs_emit_set_render_state(struct wined3d_cs *cs, enum wined3d_render_state state, DWORD value)
{
    struct wined3d_cs_set_render_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_RENDER_STATE;
    op->state = state;
    op->value = value;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_texture_state(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_texture_state *op = data;
    struct wined3d_device *device = cs->device;
    UINT stage = op->stage;
    enum wined3d_texture_stage_state state = op->state;
    DWORD value = op->value;
    DWORD old_value;
    const struct wined3d_d3d_info *d3d_info = &device->adapter->d3d_info;

    old_value = cs->state.texture_states[stage][state];
    cs->state.texture_states[stage][state] = value;

    /* Colorop change above lowest disabled stage? That won't change
     * anything in the GL setup. Changes in other states are important on
     * disabled stages too. */
    if (stage > cs->state.lowest_disabled_stage
            && device->StateTable[STATE_TEXTURESTAGE(0, state)].representative
            == STATE_TEXTURESTAGE(0, WINED3D_TSS_COLOR_OP))
        goto done;

    if (state == WINED3D_TSS_COLOR_OP)
    {
        unsigned int i;

        if (value == WINED3D_TOP_DISABLE && old_value != WINED3D_TOP_DISABLE)
        {
            /* Previously enabled stage disabled now. Make sure to dirtify
             * all enabled stages above stage, they have to be disabled.
             *
             * The current stage is dirtified below. */
            for (i = stage + 1; i < cs->state.lowest_disabled_stage; ++i)
            {
                TRACE("Additionally dirtifying stage %u.\n", i);
                device_invalidate_state(device, STATE_TEXTURESTAGE(i, WINED3D_TSS_COLOR_OP));
            }
            cs->state.lowest_disabled_stage = stage;
            TRACE("New lowest disabled: %u.\n", stage);
        }
        else if (value != WINED3D_TOP_DISABLE && old_value == WINED3D_TOP_DISABLE)
        {
            /* Previously disabled stage enabled. Stages above it may need
             * enabling. Stage must be lowest_disabled_stage here, if it's
             * bigger success is returned above, and stages below the lowest
             * disabled stage can't be enabled (because they are enabled
             * already).
             *
             * Again stage stage doesn't need to be dirtified here, it is
             * handled below. */
            for (i = stage + 1; i < d3d_info->limits.ffp_blend_stages; ++i)
            {
                if (cs->state.texture_states[i][WINED3D_TSS_COLOR_OP] == WINED3D_TOP_DISABLE)
                    break;
                TRACE("Additionally dirtifying stage %u due to enable.\n", i);
                device_invalidate_state(device, STATE_TEXTURESTAGE(i, WINED3D_TSS_COLOR_OP));
            }
            cs->state.lowest_disabled_stage = i;
            TRACE("New lowest disabled: %u.\n", i);
        }
    }

    device_invalidate_state(device, STATE_TEXTURESTAGE(stage, state));

done:
    return sizeof(*op);
}

void wined3d_cs_emit_set_texture_state(struct wined3d_cs *cs, UINT stage,
        enum wined3d_texture_stage_state state, DWORD value)
{
    struct wined3d_cs_set_texture_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_TEXTURE_STATE;
    op->stage = stage;
    op->state = state;
    op->value = value;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_sampler_state(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_sampler_state *op = data;

    cs->state.sampler_states[op->sampler_idx][op->state] = op->value;
    device_invalidate_state(cs->device, STATE_SAMPLER(op->sampler_idx));

    return sizeof(*op);
}

void wined3d_cs_emit_set_sampler_state(struct wined3d_cs *cs, UINT sampler_idx,
        enum wined3d_sampler_state state, DWORD value)
{
    struct wined3d_cs_set_sampler_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_SAMPLER_STATE;
    op->sampler_idx = sampler_idx;
    op->state = state;
    op->value = value;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_transform(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_transform *op = data;

    cs->state.transforms[op->state] = op->matrix;
    if (op->state < WINED3D_TS_WORLD_MATRIX(cs->device->adapter->gl_info.limits.blends))
        device_invalidate_state(cs->device, STATE_TRANSFORM(op->state));

    return sizeof(*op);
}

void wined3d_cs_emit_set_transform(struct wined3d_cs *cs, enum wined3d_transform_state state,
        const struct wined3d_matrix *matrix)
{
    struct wined3d_cs_set_transform *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_TRANSFORM;
    op->state = state;
    op->matrix = *matrix;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_clip_plane(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_clip_plane *op = data;

    cs->state.clip_planes[op->plane_idx] = op->plane;
    device_invalidate_state(cs->device, STATE_CLIPPLANE(op->plane_idx));

    return sizeof(*op);
}

void wined3d_cs_emit_set_clip_plane(struct wined3d_cs *cs, UINT plane_idx, const struct wined3d_vec4 *plane)
{
    struct wined3d_cs_set_clip_plane *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_CLIP_PLANE;
    op->plane_idx = plane_idx;
    op->plane = *plane;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_material(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_material *op = data;

    cs->state.material = op->material;
    device_invalidate_state(cs->device, STATE_MATERIAL);

    return sizeof(*op);
}

void wined3d_cs_emit_set_material(struct wined3d_cs *cs, const struct wined3d_material *material)
{
    struct wined3d_cs_set_material *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_MATERIAL;
    op->material = *material;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_base_vertex_index(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_base_vertex_index *op = data;

    cs->state.base_vertex_index = op->base_vertex_index;
    device_invalidate_state(cs->device, STATE_BASEVERTEXINDEX);

    return sizeof(*op);
}

void wined3d_cs_emit_set_base_vertex_index(struct wined3d_cs *cs,
        UINT base_vertex_index)
{
    struct wined3d_cs_set_base_vertex_index *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_BASE_VERTEX_INDEX;
    op->base_vertex_index = base_vertex_index;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_primitive_type(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_primitive_type *op = data;
    GLenum prev;

    prev = cs->state.gl_primitive_type;

    if (op->gl_primitive_type == GL_POINTS || prev == GL_POINTS)
        device_invalidate_state(cs->device, STATE_POINT_SIZE_ENABLE);

    cs->state.gl_primitive_type = op->gl_primitive_type;

    return sizeof(*op);
}

void wined3d_cs_emit_set_primitive_type(struct wined3d_cs *cs, GLenum primitive_type)
{
    struct wined3d_cs_set_primitive_type *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_PRIMITIVE_TYPE;
    op->gl_primitive_type = primitive_type;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_vs_cb(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_cb *op = data;
    struct wined3d_buffer *prev = cs->state.vs_cb[op->idx];

    if (op->buffer)
        InterlockedIncrement(&op->buffer->resource.bind_count);

    cs->state.vs_cb[op->idx] = op->buffer;

    if (prev)
        InterlockedDecrement(&prev->resource.bind_count);

    return sizeof(*op);
}

static UINT wined3d_cs_exec_set_ps_cb(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_cb *op = data;
    struct wined3d_buffer *prev = cs->state.ps_cb[op->idx];

    if (op->buffer)
        InterlockedIncrement(&op->buffer->resource.bind_count);

    cs->state.ps_cb[op->idx] = op->buffer;

    if (prev)
        InterlockedDecrement(&prev->resource.bind_count);


    return sizeof(*op);
}

static UINT wined3d_cs_exec_set_gs_cb(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_cb *op = data;
    struct wined3d_buffer *prev = cs->state.gs_cb[op->idx];

    if (op->buffer)
        InterlockedIncrement(&op->buffer->resource.bind_count);

    cs->state.gs_cb[op->idx] = op->buffer;

    if (prev)
        InterlockedDecrement(&prev->resource.bind_count);

    return sizeof(*op);
}

void wined3d_cs_emit_set_cb(struct wined3d_cs *cs, UINT idx, struct wined3d_buffer *buffer,
        enum wined3d_shader_type type)
{
    struct wined3d_cs_set_cb *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    switch (type)
    {
        case WINED3D_SHADER_TYPE_VERTEX:
            op->opcode = WINED3D_CS_OP_SET_VS_CB;
            break;
        case WINED3D_SHADER_TYPE_PIXEL:
            op->opcode = WINED3D_CS_OP_SET_PS_CB;
            break;
        case WINED3D_SHADER_TYPE_GEOMETRY:
            op->opcode = WINED3D_CS_OP_SET_GS_CB;
            break;
    }
    op->idx = idx;
    op->buffer = buffer;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_vs_sampler(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_sampler *op = data;

    cs->state.vs_sampler[op->idx] = op->sampler;

    return sizeof(*op);
}

static UINT wined3d_cs_exec_set_ps_sampler(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_sampler *op = data;

    cs->state.ps_sampler[op->idx] = op->sampler;

    return sizeof(*op);
}

static UINT wined3d_cs_exec_set_gs_sampler(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_sampler *op = data;

    cs->state.gs_sampler[op->idx] = op->sampler;

    return sizeof(*op);
}

void wined3d_cs_emit_set_sampler(struct wined3d_cs *cs, UINT idx, struct wined3d_sampler *sampler,
        enum wined3d_shader_type type)
{
    struct wined3d_cs_set_sampler *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    switch (type)
    {
        case WINED3D_SHADER_TYPE_VERTEX:
            op->opcode = WINED3D_CS_OP_SET_VS_SAMPLER;
            break;
        case WINED3D_SHADER_TYPE_PIXEL:
            op->opcode = WINED3D_CS_OP_SET_PS_SAMPLER;
            break;
        case WINED3D_SHADER_TYPE_GEOMETRY:
            op->opcode = WINED3D_CS_OP_SET_GS_SAMPLER;
            break;
    }
    op->idx = idx;
    op->sampler = sampler;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_stream_output(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_stream_output *op = data;
    struct wined3d_buffer *prev = cs->state.stream_output[op->idx].buffer;

    if (op->buffer)
        InterlockedIncrement(&op->buffer->resource.bind_count);

    cs->state.stream_output[op->idx].buffer = op->buffer;
    cs->state.stream_output[op->idx].offset = op->offset;

    if (op->buffer)
        InterlockedDecrement(&prev->resource.bind_count);

    return sizeof(*op);
}

void wined3d_cs_emit_set_stream_output(struct wined3d_cs *cs, UINT idx,
        struct wined3d_buffer *buffer, UINT offset)
{
    struct wined3d_cs_set_stream_output *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_STREAM_OUTPUT;
    op->idx = idx;
    op->buffer = buffer;
    op->offset = offset;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_light(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_light *op = data;

    UINT light_idx = op->light.OriginalIndex;
    UINT hash_idx = LIGHTMAP_HASHFUNC(op->light.OriginalIndex);
    struct wined3d_light_info *object = NULL;
    struct list *e;

    LIST_FOR_EACH(e, &cs->state.light_map[hash_idx])
    {
        object = LIST_ENTRY(e, struct wined3d_light_info, entry);
        if (object->OriginalIndex == light_idx)
            break;
        object = NULL;
    }

    if (!object)
    {
        TRACE("Adding new light\n");
        object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
        if (!object)
            return E_OUTOFMEMORY;

        list_add_head(&cs->state.light_map[hash_idx], &object->entry);
        object->glIndex = -1;
        object->OriginalIndex = light_idx;
    }

    object->OriginalParms = op->light.OriginalParms;
    memcpy(object->lightPosn, op->light.lightPosn, sizeof(object->lightPosn));
    memcpy(object->lightDirn, op->light.lightDirn, sizeof(object->lightDirn));
    object->exponent = op->light.exponent;
    object->cutoff = op->light.cutoff;

    /* Update the live definitions if the light is currently assigned a glIndex. */
    if (object->glIndex != -1)
        device_invalidate_state(cs->device, STATE_ACTIVELIGHT(object->glIndex));

    return sizeof(*op);
}

void wined3d_cs_emit_set_light(struct wined3d_cs *cs, const struct wined3d_light_info *light)
{
    struct wined3d_cs_set_light *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_LIGHT;
    op->light = *light;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_set_light_enable(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_light_enable *op = data;
    UINT hash_idx = LIGHTMAP_HASHFUNC(op->idx);
    struct wined3d_light_info *light_info = NULL;
    struct list *e;
    struct wined3d_device *device = cs->device;

    LIST_FOR_EACH(e, &cs->state.light_map[hash_idx])
    {
        light_info = LIST_ENTRY(e, struct wined3d_light_info, entry);
        if (light_info->OriginalIndex == op->idx)
            break;
        light_info = NULL;
    }
    TRACE("Found light %p.\n", light_info);

    /* Should be handled by the device by emitting a set_light op */
    if (!light_info)
    {
        ERR("Light enabled requested but light not defined in cs state!\n");
        return sizeof(*op);
    }

    if (!op->enable)
    {
        if (light_info->glIndex != -1)
        {
            device_invalidate_state(device, STATE_LIGHT_TYPE);
            device_invalidate_state(device, STATE_ACTIVELIGHT(light_info->glIndex));
            cs->state.lights[light_info->glIndex] = NULL;
            light_info->glIndex = -1;
        }
        else
        {
            TRACE("Light already disabled, nothing to do\n");
        }
        light_info->enabled = FALSE;
    }
    else
    {
        light_info->enabled = TRUE;
        if (light_info->glIndex != -1)
        {
            TRACE("Nothing to do as light was enabled\n");
        }
        else
        {
            unsigned int i;
            const struct wined3d_gl_info *gl_info = &device->adapter->gl_info;
            /* Find a free GL light. */
            for (i = 0; i < gl_info->limits.lights; ++i)
            {
                if (!cs->state.lights[i])
                {
                    cs->state.lights[i] = light_info;
                    light_info->glIndex = i;
                    break;
                }
            }
            if (light_info->glIndex == -1)
            {
                /* Should be caught by the device before emitting
                 * the light_enable op */
                ERR("Too many concurrently active lights in cs\n");
                return sizeof(*op);
            }

            /* i == light_info->glIndex */
            device_invalidate_state(device, STATE_LIGHT_TYPE);
            device_invalidate_state(device, STATE_ACTIVELIGHT(i));
        }
    }

    return sizeof(*op);
}

void wined3d_cs_emit_set_light_enable(struct wined3d_cs *cs, UINT idx, BOOL enable)
{
    struct wined3d_cs_set_light_enable *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_LIGHT_ENABLE;
    op->idx = idx;
    op->enable = enable;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_blt(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_blt *op = data;

    surface_blt_ugly(op->dst_surface, &op->dst_rect,
            op->src_surface, &op->src_rect,
            op->flags, &op->fx, op->filter);

    return sizeof(*op);
}

void wined3d_cs_emit_blt(struct wined3d_cs *cs, struct wined3d_surface *dst_surface,
        const RECT *dst_rect, struct wined3d_surface *src_surface,
        const RECT *src_rect, DWORD flags, const WINEDDBLTFX *fx,
        enum wined3d_texture_filter_type filter)
{
    struct wined3d_cs_blt *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_BLT;
    op->dst_surface = dst_surface;
    op->dst_rect = *dst_rect;
    op->src_surface = src_surface;
    op->src_rect = *src_rect;
    op->flags = flags;
    op->filter = filter;
    if (fx)
        op->fx = *fx;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_color_fill(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_color_fill *op = data;

    surface_color_fill(op->surface, &op->rect, &op->color);

    return sizeof(*op);
}

void wined3d_cs_emit_color_fill(struct wined3d_cs *cs, struct wined3d_surface *surface,
        const RECT *rect, const struct wined3d_color *color)
{
    struct wined3d_cs_color_fill *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_COLOR_FILL;
    op->surface = surface;
    op->rect = *rect;
    op->color = *color;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_surface_map(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_surface_map *op = data;
    struct wined3d_surface *surface = op->surface;
    const RECT *r = op->has_rect ? &op->rect : NULL;

    surface->surface_ops->surface_map(surface, r, op->flags);

    return sizeof(*op);
}

void wined3d_cs_emit_surface_map(struct wined3d_cs *cs, struct wined3d_surface *surface,
        const RECT *rect, DWORD flags)
{
    struct wined3d_cs_surface_map *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SURFACE_MAP;
    op->surface = surface;
    if (rect)
    {
        op->rect = *rect;
        op->has_rect = TRUE;
    }
    else
    {
        op->has_rect = FALSE;
    }
    op->flags = flags;

    cs->ops->submit(cs, sizeof(*op));

    if (flags & (WINED3D_MAP_NOOVERWRITE | WINED3D_MAP_DISCARD))
    {
        FIXME("Dynamic surface map is inefficient\n");
    }

    cs->ops->finish(cs);
}

static UINT wined3d_cs_exec_surface_unmap(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_surface_unmap *op = data;
    struct wined3d_surface *surface = op->surface;

    surface->surface_ops->surface_unmap(surface);

    return sizeof(*op);
}

void wined3d_cs_emit_surface_unmap(struct wined3d_cs *cs, struct wined3d_surface *surface)
{
    struct wined3d_cs_surface_unmap *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SURFACE_UNMAP;
    op->surface = surface;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_swap_mem(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_swap_mem *op = data;
    struct wined3d_buffer *buffer = op->buffer;

    wined3d_resource_free_sysmem(buffer->resource.heap_memory);
    buffer->resource.allocatedMemory = op->mem;
    buffer->resource.heap_memory = op->mem;

    if (!buffer->buffer_object && buffer->resource.bind_count)
    {
        device_invalidate_state(cs->device, STATE_STREAMSRC);
        device_invalidate_state(cs->device, STATE_INDEXBUFFER);
    }
    return sizeof(*op);
}

void wined3d_cs_emit_swap_mem(struct wined3d_cs *cs, struct wined3d_buffer *buffer, BYTE *mem)
{
    struct wined3d_cs_swap_mem *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SWAP_MEM;
    op->buffer = buffer;
    op->mem = mem;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_buffer_invalidate_bo_range(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_buffer_invalidate_bo_range *op = data;

    buffer_invalidate_bo_range(op->buffer, op->offset, op->size);
    return sizeof(*op);
}

void wined3d_cs_emit_buffer_invalidate_bo_range(struct wined3d_cs *cs,
        struct wined3d_buffer *buffer, UINT offset, UINT size)
{
    struct wined3d_cs_buffer_invalidate_bo_range *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_BUFFER_INVALIDATE_RANGE;
    op->buffer = buffer;
    op->offset = offset;
    op->size = size;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_query_issue(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_query_issue *op = data;
    struct wined3d_query *query = op->query;

    query->query_ops->query_issue(query, op->flags);

    if (op->flags & WINED3DISSUE_END)
        InterlockedIncrement(&query->counter_worker);

    return sizeof(*op);
}

void wined3d_cs_emit_query_issue(struct wined3d_cs *cs, struct wined3d_query *query, DWORD flags)
{
    struct wined3d_cs_query_issue *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_QUERY_ISSUE;
    op->query = query;
    op->flags = flags;

    cs->ops->submit(cs, sizeof(*op));
}

static UINT wined3d_cs_exec_query_get_data(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_query_get_data *op = data;
    struct wined3d_query *query = op->query;

    *op->ret = query->query_ops->query_get_data(query, op->data, op->data_size, op->flags);

    return sizeof(*op);
}

void wined3d_cs_emit_query_get_data(struct wined3d_cs *cs, struct wined3d_query *query, void *data,
        UINT data_size, DWORD flags, HRESULT *ret)
{
    struct wined3d_cs_query_get_data *op;

    op = cs->ops->require_space_prio(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_QUERY_GET_DATA;
    op->query = query;
    op->data = data;
    op->data_size = data_size;
    op->flags = flags;
    op->ret = ret;

    cs->ops->submit_prio(cs, sizeof(*op));
    cs->ops->finish_prio(cs);
}

static UINT (* const wined3d_cs_op_handlers[])(struct wined3d_cs *cs, const void *data) =
{
    /* WINED3D_CS_OP_NOP                    */ wined3d_cs_exec_nop,
    /* WINED3D_CS_OP_SKIP                   */ wined3d_cs_exec_skip,
    /* WINED3D_CS_OP_FENCE                  */ wined3d_cs_exec_fence,
    /* WINED3D_CS_OP_PRESENT                */ wined3d_cs_exec_present,
    /* WINED3D_CS_OP_CLEAR                  */ wined3d_cs_exec_clear,
    /* WINED3D_CS_OP_DRAW                   */ wined3d_cs_exec_draw,
    /* WINED3D_CS_OP_SET_RENDER_TARGET      */ wined3d_cs_exec_set_render_target,
    /* WINED3D_CS_OP_SET_VS_CONSTS_F        */ wined3d_cs_exec_set_vs_consts_f,
    /* WINED3D_CS_OP_SET_VS_CONSTS_B        */ wined3d_cs_exec_set_vs_consts_b,
    /* WINED3D_CS_OP_SET_VS_CONSTS_I        */ wined3d_cs_exec_set_vs_consts_i,
    /* WINED3D_CS_OP_SET_PS_CONSTS_F        */ wined3d_cs_exec_set_ps_consts_f,
    /* WINED3D_CS_OP_SET_PS_CONSTS_B        */ wined3d_cs_exec_set_ps_consts_b,
    /* WINED3D_CS_OP_SET_PS_CONSTS_I        */ wined3d_cs_exec_set_ps_consts_i,
    /* WINED3D_CS_OP_RESET_STATE            */ wined3d_cs_exec_reset_state,
    /* WINED3D_CS_OP_GLFINISH               */ wined3d_cs_exec_glfinish,
    /* WINED3D_CS_OP_SET_VIEWPORT           */ wined3d_cs_exec_set_viewport,
    /* WINED3D_CS_OP_SET_SCISSOR_RECT       */ wined3d_cs_exec_set_scissor_rect,
    /* WINED3D_CS_OP_SET_DEPTH_STENCIL      */ wined3d_cs_exec_set_depth_stencil,
    /* WINED3D_CS_OP_SET_VERTEX_DECLARATION */ wined3d_cs_exec_set_vertex_declaration,
    /* WINED3D_CS_OP_SET_STREAM_SOURCE      */ wined3d_cs_exec_set_stream_source,
    /* WINED3D_CS_OP_SET_STREAM_SOURCE_FREQ */ wined3d_cs_exec_set_stream_source_freq,
    /* WINED3D_CS_OP_SET_INDEX_BUFFER       */ wined3d_cs_exec_set_index_buffer,
    /* WINED3D_CS_OP_SET_TEXTURE            */ wined3d_cs_exec_set_texture,
    /* WINED3D_CS_OP_SET_VERTEX_SHADER      */ wined3d_cs_exec_set_vertex_shader,
    /* WINED3D_CS_OP_SET_PIXEL_SHADER       */ wined3d_cs_exec_set_pixel_shader,
    /* WINED3D_CS_OP_SET_GEOMETRY_SHADER    */ wined3d_cs_exec_set_geometry_shader,
    /* WINED3D_CS_OP_SET_RENDER_STATE       */ wined3d_cs_exec_set_render_state,
    /* WINED3D_CS_OP_SET_TEXTURE_STATE      */ wined3d_cs_exec_set_texture_state,
    /* WINED3D_CS_OP_SET_SAMPLER_STATE      */ wined3d_cs_exec_set_sampler_state,
    /* WINED3D_CS_OP_SET_TRANSFORM          */ wined3d_cs_exec_set_transform,
    /* WINED3D_CS_OP_SET_CLIP_PLANE         */ wined3d_cs_exec_set_clip_plane,
    /* WINED3D_CS_OP_SET_MATERIAL           */ wined3d_cs_exec_set_material,
    /* WINED3D_CS_OP_SET_BASE_VERTEX_INDEX  */ wined3d_cs_exec_set_base_vertex_index,
    /* WINED3D_CS_OP_SET_PRIMITIVE_TYPE     */ wined3d_cs_exec_set_primitive_type,
    /* WINED3D_CS_OP_SET_VS_CB              */ wined3d_cs_exec_set_vs_cb,
    /* WINED3D_CS_OP_SET_PS_CB              */ wined3d_cs_exec_set_ps_cb,
    /* WINED3D_CS_OP_SET_GS_CB              */ wined3d_cs_exec_set_gs_cb,
    /* WINED3D_CS_OP_SET_VS_SAMPLER         */ wined3d_cs_exec_set_vs_sampler,
    /* WINED3D_CS_OP_SET_PS_SAMPLER         */ wined3d_cs_exec_set_ps_sampler,
    /* WINED3D_CS_OP_SET_GS_SAMPLER         */ wined3d_cs_exec_set_gs_sampler,
    /* WINED3D_CS_OP_SET_STREAM_OUTPUT      */ wined3d_cs_exec_set_stream_output,
    /* WINED3D_CS_OP_SET_LIGHT              */ wined3d_cs_exec_set_light,
    /* WINED3D_CS_OP_SET_LIGHT_ENABLE       */ wined3d_cs_exec_set_light_enable,
    /* WINED3D_CS_OP_BLT                    */ wined3d_cs_exec_blt,
    /* WINED3D_CS_OP_COLOR_FILL             */ wined3d_cs_exec_color_fill,
    /* WINED3D_CS_OP_SURFACE_MAP            */ wined3d_cs_exec_surface_map,
    /* WINED3D_CS_OP_SURFACE_UNMAP          */ wined3d_cs_exec_surface_unmap,
    /* WINED3D_CS_OP_SWAP_MEM               */ wined3d_cs_exec_swap_mem,
    /* WINED3D_CS_OP_BUFFER_INVALIDATE_RANGE*/ wined3d_cs_exec_buffer_invalidate_bo_range,
    /* WINED3D_CS_OP_QUERY_ISSUE            */ wined3d_cs_exec_query_issue,
    /* WINED3D_CS_OP_QUERY_GET_DATA         */ wined3d_cs_exec_query_get_data,
};

static inline void *_wined3d_cs_mt_require_space(struct wined3d_cs *cs, size_t size, BOOL prio)
{
    struct wined3d_cs_queue *queue = prio ? &cs->prio_queue : &cs->queue;
    size_t queue_size = sizeof(queue->data) / sizeof(*queue->data);

    if (queue_size - size < queue->head)
    {
        struct wined3d_cs_skip *skip;
        size_t nop_size = queue_size - queue->head;

        skip = _wined3d_cs_mt_require_space(cs, nop_size, prio);
        if (nop_size < sizeof(*skip))
        {
            skip->opcode = WINED3D_CS_OP_NOP;
        }
        else
        {
            skip->opcode = WINED3D_CS_OP_SKIP;
            skip->size = nop_size;
        }

        if (prio)
            cs->ops->submit_prio(cs, nop_size);
        else
            cs->ops->submit(cs, nop_size);

        assert(!queue->head);
    }

    while(1)
    {
        LONG head = queue->head;
        LONG tail = *((volatile LONG *)&queue->tail);
        LONG new_pos;
        /* Empty */
        if (head == tail)
            break;
        /* Head ahead of tail, take care of wrap-around */
        new_pos = (head + size) & (WINED3D_CS_QUEUE_SIZE - 1);
        if (head > tail && (new_pos || tail))
            break;
        /* Tail ahead of head, but still enough space */
        if (new_pos < tail && new_pos)
            break;

        TRACE("Waiting for free space. Head %u, tail %u, want %u\n", head, tail,
                (unsigned int) size);
    }

    return &queue->data[queue->head];
}

static inline void *wined3d_cs_mt_require_space(struct wined3d_cs *cs, size_t size)
{
    return _wined3d_cs_mt_require_space(cs, size, FALSE);
}

static inline void *wined3d_cs_mt_require_space_prio(struct wined3d_cs *cs, size_t size)
{
    return _wined3d_cs_mt_require_space(cs, size, TRUE);
}

/* FIXME: wined3d_device_uninit_3d() should either flush and wait, or be an
 * OP itself. */
static void wined3d_cs_emit_stop(struct wined3d_cs *cs)
{
    struct wined3d_cs_stop *op;

    op = wined3d_cs_mt_require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_STOP;

    wined3d_cs_mt_submit(cs, sizeof(*op));
}

static void wined3d_cs_mt_finish(struct wined3d_cs *cs)
{
    BOOL fence;

    if (cs->thread_id == GetCurrentThreadId())
    {
        static BOOL once;
        if (!once)
        {
            FIXME("flush_and_wait called from cs thread\n");
            once = TRUE;
        }
        return;
    }

    wined3d_cs_emit_fence(cs, &fence);

    /* A busy wait should be fine, we're not supposed to have to wait very
     * long. */
    while (!InterlockedCompareExchange(&fence, TRUE, TRUE));
}

static void wined3d_cs_mt_finish_prio(struct wined3d_cs *cs)
{
    BOOL fence;

    if (cs->thread_id == GetCurrentThreadId())
    {
        static BOOL once;
        if (!once)
        {
            FIXME("flush_and_wait called from cs thread\n");
            once = TRUE;
        }
        return;
    }

    wined3d_cs_emit_fence_prio(cs, &fence);

    /* A busy wait should be fine, we're not supposed to have to wait very
     * long. */
    while (!InterlockedCompareExchange(&fence, TRUE, TRUE));
}

static const struct wined3d_cs_ops wined3d_cs_mt_ops =
{
    wined3d_cs_mt_require_space,
    wined3d_cs_mt_require_space_prio,
    wined3d_cs_mt_submit,
    wined3d_cs_mt_submit_prio,
    wined3d_cs_mt_finish,
    wined3d_cs_mt_finish_prio,
};

static void wined3d_cs_st_submit(struct wined3d_cs *cs, size_t size)
{
    enum wined3d_cs_op opcode = *(const enum wined3d_cs_op *)&cs->queue.data;

    if (opcode >= WINED3D_CS_OP_STOP)
    {
        ERR("Invalid opcode %#x.\n", opcode);
        return;
    }

    wined3d_cs_op_handlers[opcode](cs, &cs->queue.data);
}

static void wined3d_cs_st_finish(struct wined3d_cs *cs)
{
}

static void *wined3d_cs_st_require_space(struct wined3d_cs *cs, size_t size)
{
    return cs->queue.data;
}

static const struct wined3d_cs_ops wined3d_cs_st_ops =
{
    wined3d_cs_st_require_space,
    wined3d_cs_st_require_space,
    wined3d_cs_st_submit,
    wined3d_cs_st_submit,
    wined3d_cs_st_finish,
    wined3d_cs_st_finish,
};

void wined3d_cs_switch_onscreen_ds(struct wined3d_cs *cs,
        struct wined3d_context *context, struct wined3d_surface *depth_stencil)
{
    if (cs->onscreen_depth_stencil)
    {
        surface_load_ds_location(cs->onscreen_depth_stencil, context, SFLAG_INTEXTURE);

        surface_modify_ds_location(cs->onscreen_depth_stencil, SFLAG_INTEXTURE,
                cs->onscreen_depth_stencil->ds_current_size.cx,
                cs->onscreen_depth_stencil->ds_current_size.cy);
        wined3d_surface_decref(cs->onscreen_depth_stencil);
    }
    cs->onscreen_depth_stencil = depth_stencil;
    wined3d_surface_incref(cs->onscreen_depth_stencil);
}

static DWORD WINAPI wined3d_cs_run(void *thread_param)
{
    struct wined3d_cs *cs = thread_param;
    enum wined3d_cs_op opcode;
    LONG tail;
    struct wined3d_cs_queue *queue;

    TRACE("Started.\n");

    cs->thread_id = GetCurrentThreadId();
    for (;;)
    {
        if (*((volatile LONG *)&cs->prio_queue.head) != cs->prio_queue.tail)
        {
            queue = &cs->prio_queue;
        }
        else if (*((volatile LONG *)&cs->queue.head) != cs->queue.tail)
        {
            queue = &cs->queue;
            if (*((volatile LONG *)&cs->prio_queue.head) != cs->prio_queue.tail)
                queue = &cs->prio_queue;
        }
        else
        {
            continue;
        }

        tail = queue->tail;
        opcode = *(const enum wined3d_cs_op *)&queue->data[tail];

        if (opcode >= WINED3D_CS_OP_STOP)
        {
            if (opcode > WINED3D_CS_OP_STOP)
                ERR("Invalid opcode %#x.\n", opcode);
            goto done;
        }

        tail += wined3d_cs_op_handlers[opcode](cs, &queue->data[tail]);
        tail &= (WINED3D_CS_QUEUE_SIZE - 1);
        InterlockedExchange(&queue->tail, tail);
    }

done:
    TRACE("Stopped.\n");
    return 0;
}

/* We could also create a single thread for all of wined3d, instead of one for
 * each device, at the cost of some extra overhead for each block. I'm not
 * sure that we'd gain anything from that though. */
struct wined3d_cs *wined3d_cs_create(struct wined3d_device *device)
{
    struct wined3d_cs *cs = NULL;
    const struct wined3d_adapter *adapter = device->adapter;
    const struct wined3d_d3d_info *d3d_info = &adapter->d3d_info;
    const struct wined3d_gl_info *gl_info = &adapter->gl_info;
    DWORD ret;

    if (!(cs = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cs))))
    {
        ERR("Failed to allocate wined3d command stream memory.\n");
        return NULL;
    }

    if (FAILED(state_init(&cs->state, d3d_info, gl_info)))
    {
        ERR("Failed to init state\n");
        goto err;
    }

    if (wined3d_settings.cs_multithreaded)
    {
        if (!(cs->thread = CreateThread(NULL, 0, wined3d_cs_run, cs, 0, &ret)))
        {
            ERR("Failed to create wined3d command stream thread.\n");
            goto err;
        }

        cs->ops = &wined3d_cs_mt_ops;
    }
    else
    {
        cs->ops = &wined3d_cs_st_ops;
    }
    cs->device = device;

    return cs;

err:
    state_cleanup(&cs->state, FALSE);
    HeapFree(GetProcessHeap(), 0, cs);
    return NULL;
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
    }

    /* The cs does not hold references to its bound, resources because this would delay
     * resource destruction and private data release, until the CS releases its reference,
     * making the CS visible to the client libraries and applications. */
    state_cleanup(&cs->state, FALSE);

    HeapFree(GetProcessHeap(), 0, cs);
}
