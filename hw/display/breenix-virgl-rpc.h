/* SPDX-License-Identifier: MIT */
#ifndef BREENIX_VIRGL_RPC_H
#define BREENIX_VIRGL_RPC_H
void virtio_gpu_renderer_call(void (*fn)(void *), void *arg);
void virtio_gpu_renderer_flush(void);
void virtio_gpu_renderer_release_handle(enum virgl_renderer_native_handle_type type, virgl_renderer_native_handle handle);
typedef struct { void *cookie; } brpc_cleanup_args;
static inline void brpc_cleanup_run(void *opaque) {
    brpc_cleanup_args *a = opaque;
    virgl_renderer_cleanup(a->cookie);
}
static inline void brpc_cleanup(void *cookie) {
    brpc_cleanup_args a = { .cookie = cookie };
    virtio_gpu_renderer_call(brpc_cleanup_run, &a);

}
typedef struct { uint32_t handle; uint32_t nlen; const char *name; int result; } brpc_context_create_args;
static inline void brpc_context_create_run(void *opaque) {
    brpc_context_create_args *a = opaque;
    a->result = virgl_renderer_context_create(a->handle, a->nlen, a->name);
}
static inline int brpc_context_create(uint32_t handle, uint32_t nlen, const char *name) {
    brpc_context_create_args a = { .handle = handle, .nlen = nlen, .name = name };
    virtio_gpu_renderer_call(brpc_context_create_run, &a);
    return a.result;
}
typedef struct { uint32_t ctx_id; uint32_t flags; uint32_t ring_idx; uint64_t fence_id; int result; } brpc_context_create_fence_args;
static inline void brpc_context_create_fence_run(void *opaque) {
    brpc_context_create_fence_args *a = opaque;
    a->result = virgl_renderer_context_create_fence(a->ctx_id, a->flags, a->ring_idx, a->fence_id);
}
static inline int brpc_context_create_fence(uint32_t ctx_id, uint32_t flags, uint32_t ring_idx, uint64_t fence_id) {
    brpc_context_create_fence_args a = { .ctx_id = ctx_id, .flags = flags, .ring_idx = ring_idx, .fence_id = fence_id };
    virtio_gpu_renderer_call(brpc_context_create_fence_run, &a);
    return a.result;
}
typedef struct { uint32_t ctx_id; uint32_t ctx_flags; uint32_t nlen; const char *name; int result; } brpc_context_create_with_flags_args;
static inline void brpc_context_create_with_flags_run(void *opaque) {
    brpc_context_create_with_flags_args *a = opaque;
    a->result = virgl_renderer_context_create_with_flags(a->ctx_id, a->ctx_flags, a->nlen, a->name);
}
static inline int brpc_context_create_with_flags(uint32_t ctx_id, uint32_t ctx_flags, uint32_t nlen, const char *name) {
    brpc_context_create_with_flags_args a = { .ctx_id = ctx_id, .ctx_flags = ctx_flags, .nlen = nlen, .name = name };
    virtio_gpu_renderer_call(brpc_context_create_with_flags_run, &a);
    return a.result;
}
typedef struct { uint32_t handle; } brpc_context_destroy_args;
static inline void brpc_context_destroy_run(void *opaque) {
    brpc_context_destroy_args *a = opaque;
    virgl_renderer_context_destroy(a->handle);
}
static inline void brpc_context_destroy(uint32_t handle) {
    brpc_context_destroy_args a = { .handle = handle };
    virtio_gpu_renderer_call(brpc_context_destroy_run, &a);

}
typedef struct { int client_fence_id; uint32_t ctx_id; int result; } brpc_create_fence_args;
static inline void brpc_create_fence_run(void *opaque) {
    brpc_create_fence_args *a = opaque;
    a->result = virgl_renderer_create_fence(a->client_fence_id, a->ctx_id);
}
static inline int brpc_create_fence(int client_fence_id, uint32_t ctx_id) {
    brpc_create_fence_args a = { .client_fence_id = client_fence_id, .ctx_id = ctx_id };
    virtio_gpu_renderer_call(brpc_create_fence_run, &a);
    return a.result;
}
typedef struct { uint32_t res_id; uint32_t width; uint32_t height; uint32_t virgl_format; uint32_t padding; uint32_t stride; uint32_t offset; virgl_renderer_native_handle *handle; enum virgl_renderer_native_handle_type result; } brpc_create_handle_for_scanout_args;
static inline void brpc_create_handle_for_scanout_run(void *opaque) {
    brpc_create_handle_for_scanout_args *a = opaque;
    a->result = virgl_renderer_create_handle_for_scanout(a->res_id, a->width, a->height, a->virgl_format, a->padding, a->stride, a->offset, a->handle);
}
static inline enum virgl_renderer_native_handle_type brpc_create_handle_for_scanout(uint32_t res_id, uint32_t width, uint32_t height, uint32_t virgl_format, uint32_t padding, uint32_t stride, uint32_t offset, virgl_renderer_native_handle *handle) {
    brpc_create_handle_for_scanout_args a = { .res_id = res_id, .width = width, .height = height, .virgl_format = virgl_format, .padding = padding, .stride = stride, .offset = offset, .handle = handle };
    virtio_gpu_renderer_call(brpc_create_handle_for_scanout_run, &a);
    return a.result;
}
typedef struct { int ctx_id; int res_handle; } brpc_ctx_attach_resource_args;
static inline void brpc_ctx_attach_resource_run(void *opaque) {
    brpc_ctx_attach_resource_args *a = opaque;
    virgl_renderer_ctx_attach_resource(a->ctx_id, a->res_handle);
}
static inline void brpc_ctx_attach_resource(int ctx_id, int res_handle) {
    brpc_ctx_attach_resource_args a = { .ctx_id = ctx_id, .res_handle = res_handle };
    virtio_gpu_renderer_call(brpc_ctx_attach_resource_run, &a);

}
typedef struct { int ctx_id; int res_handle; } brpc_ctx_detach_resource_args;
static inline void brpc_ctx_detach_resource_run(void *opaque) {
    brpc_ctx_detach_resource_args *a = opaque;
    virgl_renderer_ctx_detach_resource(a->ctx_id, a->res_handle);
}
static inline void brpc_ctx_detach_resource(int ctx_id, int res_handle) {
    brpc_ctx_detach_resource_args a = { .ctx_id = ctx_id, .res_handle = res_handle };
    virtio_gpu_renderer_call(brpc_ctx_detach_resource_run, &a);

}
typedef struct { uint32_t set; uint32_t version; void *caps; } brpc_fill_caps_args;
static inline void brpc_fill_caps_run(void *opaque) {
    brpc_fill_caps_args *a = opaque;
    virgl_renderer_fill_caps(a->set, a->version, a->caps);
}
static inline void brpc_fill_caps(uint32_t set, uint32_t version, void *caps) {
    brpc_fill_caps_args a = { .set = set, .version = version, .caps = caps };
    virtio_gpu_renderer_call(brpc_fill_caps_run, &a);

}
typedef struct { char unused; } brpc_force_ctx_0_args;
static inline void brpc_force_ctx_0_run(void *opaque) {
    brpc_force_ctx_0_args *a = opaque;
    (void)a; virgl_renderer_force_ctx_0();
}
static inline void brpc_force_ctx_0(void) {
    brpc_force_ctx_0_args a = { 0 };
    virtio_gpu_renderer_call(brpc_force_ctx_0_run, &a);

}
typedef struct { uint32_t set; uint32_t *max_ver; uint32_t *max_size; } brpc_get_cap_set_args;
static inline void brpc_get_cap_set_run(void *opaque) {
    brpc_get_cap_set_args *a = opaque;
    virgl_renderer_get_cap_set(a->set, a->max_ver, a->max_size);
}
static inline void brpc_get_cap_set(uint32_t set, uint32_t *max_ver, uint32_t *max_size) {
    brpc_get_cap_set_args a = { .set = set, .max_ver = max_ver, .max_size = max_size };
    virtio_gpu_renderer_call(brpc_get_cap_set_run, &a);

}
typedef struct { uint32_t resource_id; uint32_t *width; uint32_t *height; void * result; } brpc_get_cursor_data_args;
static inline void brpc_get_cursor_data_run(void *opaque) {
    brpc_get_cursor_data_args *a = opaque;
    a->result = virgl_renderer_get_cursor_data(a->resource_id, a->width, a->height);
}
static inline void * brpc_get_cursor_data(uint32_t resource_id, uint32_t *width, uint32_t *height) {
    brpc_get_cursor_data_args a = { .resource_id = resource_id, .width = width, .height = height };
    virtio_gpu_renderer_call(brpc_get_cursor_data_run, &a);
    return a.result;
}
typedef struct { void *cookie; int flags; struct virgl_renderer_callbacks *cb; int result; } brpc_init_args;
static inline void brpc_init_run(void *opaque) {
    brpc_init_args *a = opaque;
    a->result = virgl_renderer_init(a->cookie, a->flags, a->cb);
}
static inline int brpc_init(void *cookie, int flags, struct virgl_renderer_callbacks *cb) {
    brpc_init_args a = { .cookie = cookie, .flags = flags, .cb = cb };
    virtio_gpu_renderer_call(brpc_init_run, &a);
    return a.result;
}
typedef struct { char unused; } brpc_poll_args;
static inline void brpc_poll_run(void *opaque) {
    brpc_poll_args *a = opaque;
    (void)a; virgl_renderer_poll();
}
static inline void brpc_poll(void) {
    brpc_poll_args a = { 0 };
    virtio_gpu_renderer_call(brpc_poll_run, &a);

}
typedef struct { enum virgl_renderer_native_handle_type type; virgl_renderer_native_handle handle; } brpc_release_handle_for_scanout_args;
static inline void brpc_release_handle_for_scanout_run(void *opaque) {
    brpc_release_handle_for_scanout_args *a = opaque;
    virgl_renderer_release_handle_for_scanout(a->type, a->handle);
}
static inline void brpc_release_handle_for_scanout(enum virgl_renderer_native_handle_type type, virgl_renderer_native_handle handle) {
    virtio_gpu_renderer_release_handle(type, handle);
}
typedef struct { char unused; } brpc_reset_args;
static inline void brpc_reset_run(void *opaque) {
    brpc_reset_args *a = opaque;
    (void)a; virgl_renderer_reset();
}
static inline void brpc_reset(void) {
    brpc_reset_args a = { 0 };
    virtio_gpu_renderer_call(brpc_reset_run, &a);

}
typedef struct { int res_handle; struct iovec *iov; int num_iovs; int result; } brpc_resource_attach_iov_args;
static inline void brpc_resource_attach_iov_run(void *opaque) {
    brpc_resource_attach_iov_args *a = opaque;
    a->result = virgl_renderer_resource_attach_iov(a->res_handle, a->iov, a->num_iovs);
}
static inline int brpc_resource_attach_iov(int res_handle, struct iovec *iov, int num_iovs) {
    brpc_resource_attach_iov_args a = { .res_handle = res_handle, .iov = iov, .num_iovs = num_iovs };
    virtio_gpu_renderer_call(brpc_resource_attach_iov_run, &a);
    return a.result;
}
typedef struct { struct virgl_renderer_resource_create_args *args; struct iovec *iov; uint32_t num_iovs; int result; } brpc_resource_create_args;
static inline void brpc_resource_create_run(void *opaque) {
    brpc_resource_create_args *a = opaque;
    a->result = virgl_renderer_resource_create(a->args, a->iov, a->num_iovs);
}
static inline int brpc_resource_create(struct virgl_renderer_resource_create_args *args, struct iovec *iov, uint32_t num_iovs) {
    brpc_resource_create_args a = { .args = args, .iov = iov, .num_iovs = num_iovs };
    virtio_gpu_renderer_call(brpc_resource_create_run, &a);
    return a.result;
}
typedef struct { const struct virgl_renderer_resource_create_blob_args *args; int result; } brpc_resource_create_blob_args;
static inline void brpc_resource_create_blob_run(void *opaque) {
    brpc_resource_create_blob_args *a = opaque;
    a->result = virgl_renderer_resource_create_blob(a->args);
}
static inline int brpc_resource_create_blob(const struct virgl_renderer_resource_create_blob_args *args) {
    brpc_resource_create_blob_args a = { .args = args };
    virtio_gpu_renderer_call(brpc_resource_create_blob_run, &a);
    return a.result;
}
typedef struct { int res_handle; struct iovec **iov; int *num_iovs; } brpc_resource_detach_iov_args;
static inline void brpc_resource_detach_iov_run(void *opaque) {
    brpc_resource_detach_iov_args *a = opaque;
    virgl_renderer_resource_detach_iov(a->res_handle, a->iov, a->num_iovs);
}
static inline void brpc_resource_detach_iov(int res_handle, struct iovec **iov, int *num_iovs) {
    brpc_resource_detach_iov_args a = { .res_handle = res_handle, .iov = iov, .num_iovs = num_iovs };
    virtio_gpu_renderer_call(brpc_resource_detach_iov_run, &a);

}
typedef struct { int res_handle; struct virgl_renderer_resource_info *info; int result; } brpc_resource_get_info_args;
static inline void brpc_resource_get_info_run(void *opaque) {
    brpc_resource_get_info_args *a = opaque;
    a->result = virgl_renderer_resource_get_info(a->res_handle, a->info);
}
static inline int brpc_resource_get_info(int res_handle, struct virgl_renderer_resource_info *info) {
    brpc_resource_get_info_args a = { .res_handle = res_handle, .info = info };
    virtio_gpu_renderer_call(brpc_resource_get_info_run, &a);
    return a.result;
}
typedef struct { int res_handle; struct virgl_renderer_resource_info_ext *info; int result; } brpc_resource_get_info_ext_args;
static inline void brpc_resource_get_info_ext_run(void *opaque) {
    brpc_resource_get_info_ext_args *a = opaque;
    a->result = virgl_renderer_resource_get_info_ext(a->res_handle, a->info);
}
static inline int brpc_resource_get_info_ext(int res_handle, struct virgl_renderer_resource_info_ext *info) {
    brpc_resource_get_info_ext_args a = { .res_handle = res_handle, .info = info };
    virtio_gpu_renderer_call(brpc_resource_get_info_ext_run, &a);
    return a.result;
}
typedef struct { uint32_t res_handle; uint32_t *map_info; int result; } brpc_resource_get_map_info_args;
static inline void brpc_resource_get_map_info_run(void *opaque) {
    brpc_resource_get_map_info_args *a = opaque;
    a->result = virgl_renderer_resource_get_map_info(a->res_handle, a->map_info);
}
static inline int brpc_resource_get_map_info(uint32_t res_handle, uint32_t *map_info) {
    brpc_resource_get_map_info_args a = { .res_handle = res_handle, .map_info = map_info };
    virtio_gpu_renderer_call(brpc_resource_get_map_info_run, &a);
    return a.result;
}
typedef struct { uint32_t res_handle; void **map; uint64_t *out_size; int result; } brpc_resource_map_args;
static inline void brpc_resource_map_run(void *opaque) {
    brpc_resource_map_args *a = opaque;
    a->result = virgl_renderer_resource_map(a->res_handle, a->map, a->out_size);
}
static inline int brpc_resource_map(uint32_t res_handle, void **map, uint64_t *out_size) {
    brpc_resource_map_args a = { .res_handle = res_handle, .map = map, .out_size = out_size };
    virtio_gpu_renderer_call(brpc_resource_map_run, &a);
    return a.result;
}
typedef struct { uint32_t res_handle; int result; } brpc_resource_unmap_args;
static inline void brpc_resource_unmap_run(void *opaque) {
    brpc_resource_unmap_args *a = opaque;
    a->result = virgl_renderer_resource_unmap(a->res_handle);
}
static inline int brpc_resource_unmap(uint32_t res_handle) {
    brpc_resource_unmap_args a = { .res_handle = res_handle };
    virtio_gpu_renderer_call(brpc_resource_unmap_run, &a);
    return a.result;
}
typedef struct { uint32_t res_handle; } brpc_resource_unref_args;
static inline void brpc_resource_unref_run(void *opaque) {
    brpc_resource_unref_args *a = opaque;
    virgl_renderer_resource_unref(a->res_handle);
}
static inline void brpc_resource_unref(uint32_t res_handle) {
    brpc_resource_unref_args a = { .res_handle = res_handle };
    virtio_gpu_renderer_call(brpc_resource_unref_run, &a);

}
typedef struct { void *buffer; int ctx_id; int ndw; int result; } brpc_submit_cmd_args;
static inline void brpc_submit_cmd_run(void *opaque) {
    brpc_submit_cmd_args *a = opaque;
    a->result = virgl_renderer_submit_cmd(a->buffer, a->ctx_id, a->ndw);
}
static inline int brpc_submit_cmd(void *buffer, int ctx_id, int ndw) {
    brpc_submit_cmd_args a = { .buffer = buffer, .ctx_id = ctx_id, .ndw = ndw };
    virtio_gpu_renderer_call(brpc_submit_cmd_run, &a);
    return a.result;
}
typedef struct { uint32_t handle; uint32_t ctx_id; uint32_t level; uint32_t stride; uint32_t layer_stride; struct virgl_box *box; uint64_t offset; struct iovec *iov; int iovec_cnt; int result; } brpc_transfer_read_iov_args;
static inline void brpc_transfer_read_iov_run(void *opaque) {
    brpc_transfer_read_iov_args *a = opaque;
    a->result = virgl_renderer_transfer_read_iov(a->handle, a->ctx_id, a->level, a->stride, a->layer_stride, a->box, a->offset, a->iov, a->iovec_cnt);
}
static inline int brpc_transfer_read_iov(uint32_t handle, uint32_t ctx_id, uint32_t level, uint32_t stride, uint32_t layer_stride, struct virgl_box *box, uint64_t offset, struct iovec *iov, int iovec_cnt) {
    brpc_transfer_read_iov_args a = { .handle = handle, .ctx_id = ctx_id, .level = level, .stride = stride, .layer_stride = layer_stride, .box = box, .offset = offset, .iov = iov, .iovec_cnt = iovec_cnt };
    virtio_gpu_renderer_call(brpc_transfer_read_iov_run, &a);
    return a.result;
}
typedef struct { uint32_t handle; uint32_t ctx_id; int level; uint32_t stride; uint32_t layer_stride; struct virgl_box *box; uint64_t offset; struct iovec *iovec; unsigned int iovec_cnt; int result; } brpc_transfer_write_iov_args;
static inline void brpc_transfer_write_iov_run(void *opaque) {
    brpc_transfer_write_iov_args *a = opaque;
    a->result = virgl_renderer_transfer_write_iov(a->handle, a->ctx_id, a->level, a->stride, a->layer_stride, a->box, a->offset, a->iovec, a->iovec_cnt);
}
static inline int brpc_transfer_write_iov(uint32_t handle, uint32_t ctx_id, int level, uint32_t stride, uint32_t layer_stride, struct virgl_box *box, uint64_t offset, struct iovec *iovec, unsigned int iovec_cnt) {
    brpc_transfer_write_iov_args a = { .handle = handle, .ctx_id = ctx_id, .level = level, .stride = stride, .layer_stride = layer_stride, .box = box, .offset = offset, .iovec = iovec, .iovec_cnt = iovec_cnt };
    virtio_gpu_renderer_call(brpc_transfer_write_iov_run, &a);
    return a.result;
}
#define virgl_renderer_cleanup brpc_cleanup
#define virgl_renderer_context_create brpc_context_create
#define virgl_renderer_context_create_fence brpc_context_create_fence
#define virgl_renderer_context_create_with_flags brpc_context_create_with_flags
#define virgl_renderer_context_destroy brpc_context_destroy
#define virgl_renderer_create_fence brpc_create_fence
#define virgl_renderer_create_handle_for_scanout brpc_create_handle_for_scanout
#define virgl_renderer_ctx_attach_resource brpc_ctx_attach_resource
#define virgl_renderer_ctx_detach_resource brpc_ctx_detach_resource
#define virgl_renderer_fill_caps brpc_fill_caps
#define virgl_renderer_force_ctx_0 brpc_force_ctx_0
#define virgl_renderer_get_cap_set brpc_get_cap_set
#define virgl_renderer_get_cursor_data brpc_get_cursor_data
#define virgl_renderer_init brpc_init
#define virgl_renderer_poll brpc_poll
#define virgl_renderer_release_handle_for_scanout brpc_release_handle_for_scanout
#define virgl_renderer_reset brpc_reset
#define virgl_renderer_resource_attach_iov brpc_resource_attach_iov
#define virgl_renderer_resource_create brpc_resource_create
#define virgl_renderer_resource_create_blob brpc_resource_create_blob
#define virgl_renderer_resource_detach_iov brpc_resource_detach_iov
#define virgl_renderer_resource_get_info brpc_resource_get_info
#define virgl_renderer_resource_get_info_ext brpc_resource_get_info_ext
#define virgl_renderer_resource_get_map_info brpc_resource_get_map_info
#define virgl_renderer_resource_map brpc_resource_map
#define virgl_renderer_resource_unmap brpc_resource_unmap
#define virgl_renderer_resource_unref brpc_resource_unref
#define virgl_renderer_submit_cmd brpc_submit_cmd
#define virgl_renderer_transfer_read_iov brpc_transfer_read_iov
#define virgl_renderer_transfer_write_iov brpc_transfer_write_iov
#endif
