/*
 * Virtio GPU Device
 *
 * Copyright Red Hat, Inc. 2013-2014
 *
 * Authors:
 *     Dave Airlie <airlied@redhat.com>
 *     Gerd Hoffmann <kraxel@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "system/system.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-gpu.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu-pixman.h"
#include "hw/qdev-properties.h"

#define VIRGL_RENDERER_UNSTABLE_APIS
#include <virglrenderer.h>
#include "qemu/main-loop.h"
#include "qemu/coroutine.h"
#include "system/cpus.h"
#include "ui/egl-helpers.h"
#include "breenix-render-worker.h"
#include "breenix-render-call.h"
#include "breenix-virgl-rpc.h"

/* One GL device is permitted. Device/display state remains on the main loop;
 * the worker owns only VirGL/EGL calls and never acquires BQL. */
static BRW *renderer_worker;
static QEMUBH *renderer_done_bh;
static Coroutine *renderer_co;
static GQueue renderer_events = G_QUEUE_INIT;
static bool renderer_quiescing;
EGLContext breenix_renderer_share;

typedef struct RendererCall {
    BRWJob job;
    Coroutine *co;
    bool done;
    void (*fn)(void *);
    void *arg;
} RendererCall;

typedef struct RendererEvent {
    void (*fn)(void *);
    void *arg;
} RendererEvent;

static void renderer_notify(void *opaque)
{
    qemu_bh_schedule(opaque);
}

static void renderer_done(void *opaque G_GNUC_UNUSED)
{
    brw_complete(renderer_worker);
}

static void renderer_run_call(void *opaque)
{
    RendererCall *call = opaque;
    assert(!bql_locked());
    call->fn(call->arg);
}

static void renderer_retire_call(void *opaque)
{
    RendererCall *call = opaque;
    call->done = true;
    if (call->co) {
        qemu_coroutine_enter(call->co);
    }
}

void virtio_gpu_renderer_call(void (*fn)(void *), void *arg)
{
    RendererCall call = { .fn = fn, .arg = arg };
    if (!renderer_worker || g_thread_self() == renderer_worker->thread) {
        fn(arg);
        return;
    }
    assert(bql_locked());
    call.job = (BRWJob){ renderer_run_call, renderer_retire_call, &call };
    call.co = qemu_in_coroutine() ? qemu_coroutine_self() : NULL;
    brw_submit(renderer_worker, &call.job);
    if (call.co) {
        qemu_coroutine_yield();
    } else {
        /* Initialization, native texture release and drained teardown only.
         * Dispatching the main loop also releases BQL around its poll. */
        while (!call.done) {
            main_loop_wait(false);
        }
    }
    assert(call.done);
}

typedef struct RendererRelease {
    BRWJob job;
    brpc_release_handle_for_scanout_args args;
} RendererRelease;

static void renderer_release_done(void *opaque)
{
    g_free(opaque);
}

static void renderer_release_run(void *opaque)
{
    RendererRelease *release = opaque;
    brpc_release_handle_for_scanout_run(&release->args);
}

void virtio_gpu_renderer_release_handle(enum virgl_renderer_native_handle_type type,
                                        virgl_renderer_native_handle handle)
{
    if (!renderer_worker) {
        brpc_release_handle_for_scanout_args args = { type, handle };
        brpc_release_handle_for_scanout_run(&args);
    } else {
        RendererRelease *release = g_new0(RendererRelease, 1);
        release->args = (brpc_release_handle_for_scanout_args){ type, handle };
        release->job = (BRWJob){ renderer_release_run, renderer_release_done, release };
        brw_submit(renderer_worker, &release->job);
    }
}

static void renderer_flush(void *opaque G_GNUC_UNUSED)
{
    glFlush();
}

void virtio_gpu_renderer_flush(void)
{
    virtio_gpu_renderer_call(renderer_flush, NULL);
}

static void coroutine_fn renderer_dispatch(void *opaque G_GNUC_UNUSED)
{
    RendererEvent *event;
    while ((event = g_queue_pop_head(&renderer_events))) {
        event->fn(event->arg);
        g_free(event);
    }
    renderer_co = NULL;
}

void virtio_gpu_gl_schedule(VirtIOGPU *g G_GNUC_UNUSED, void (*fn)(void *), void *arg)
{
    RendererEvent *event;
    GList *entry;
    assert(bql_locked());
    if (renderer_quiescing) {
        return;
    }
    /* Queue notifications and poll ticks are level-triggered. Bound pending
     * work by coalescing each source, just as its original bottom half did. */
    for (entry = renderer_events.head; entry; entry = entry->next) {
        event = entry->data;
        if (event->fn == fn && event->arg == arg) {
            return;
        }
    }
    event = g_new(RendererEvent, 1);
    *event = (RendererEvent){ fn, arg };
    g_queue_push_tail(&renderer_events, event);
    if (!renderer_co) {
        renderer_co = qemu_coroutine_create(renderer_dispatch, NULL);
        qemu_coroutine_enter(renderer_co);
    }
}

static void renderer_quiesce(void)
{
    renderer_quiescing = true;
    while (renderer_co) {
        main_loop_wait(false);
    }
}

static void renderer_destroy_share(void *opaque G_GNUC_UNUSED)
{
    eglDestroyContext(qemu_egl_display, breenix_renderer_share);
    breenix_renderer_share = EGL_NO_CONTEXT;
}


static void virtio_gpu_gl_update_cursor_data(VirtIOGPU *g,
                                             struct virtio_gpu_scanout *s,
                                             uint32_t resource_id)
{
    VirtIOGPUGL *gl = VIRTIO_GPU_GL(g);
    uint32_t width, height;
    uint32_t pixels, *data;

    if (gl->renderer_state != RS_INITED) {
        return;
    }

    data = virgl_renderer_get_cursor_data(resource_id, &width, &height);
    if (!data) {
        return;
    }

    if (width != s->current_cursor->width ||
        height != s->current_cursor->height) {
        free(data);
        return;
    }

    pixels = s->current_cursor->width * s->current_cursor->height;
    memcpy(s->current_cursor->data, data, pixels * sizeof(uint32_t));
    free(data);
}

static void renderer_process_cmdq(void *opaque)
{
    virtio_gpu_process_cmdq(opaque);
}

static void virtio_gpu_gl_flushed(VirtIOGPUBase *b)
{
    VirtIOGPU *g = VIRTIO_GPU(b);

    virtio_gpu_gl_schedule(g, renderer_process_cmdq, g);
}

static void virtio_gpu_gl_ctrl_work(void *opaque)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(opaque);
    VirtQueue *vq = VIRTIO_GPU(opaque)->ctrl_vq;
    VirtIOGPU *g = VIRTIO_GPU(vdev);
    VirtIOGPUGL *gl = VIRTIO_GPU_GL(vdev);
    struct virtio_gpu_ctrl_command *cmd;

    if (!virtio_queue_ready(vq)) {
        return;
    }

    switch (gl->renderer_state) {
    case RS_RESET:
        virtio_gpu_virgl_reset(g);
        /* fallthrough */
    case RS_START:
        if (virtio_gpu_virgl_init(g)) {
            gl->renderer_state = RS_INIT_FAILED;
            return;
        }

        gl->renderer_state = RS_INITED;
        break;
    case RS_INIT_FAILED:
        return;
    case RS_INITED:
        break;
    }

    cmd = virtqueue_pop(vq, sizeof(struct virtio_gpu_ctrl_command));
    while (cmd) {
        virtio_gpu_ctrl_cmd_init(cmd, vq);
        QTAILQ_INSERT_TAIL(&g->cmdq, cmd, next);
        cmd = virtqueue_pop(vq, sizeof(struct virtio_gpu_ctrl_command));
    }

    virtio_gpu_process_cmdq(g);
    virtio_gpu_virgl_fence_poll(g);
}

static void virtio_gpu_gl_handle_ctrl(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtIOGPU *g = VIRTIO_GPU(vdev);
    if (!renderer_worker && qemu_egl_display) {
        QEMUGLParams params = { .major_ver = 3, .minor_ver = 0 };
        breenix_renderer_share = dpy_gl_ctx_create(g->parent_obj.scanout[0].con,
                                                  &params);
        if (!breenix_renderer_share) {
            VIRTIO_GPU_GL(g)->renderer_state = RS_INIT_FAILED;
            return;
        }
        /* This uncurrent shared context belongs to the worker. The display's
         * own root context and surfaces remain on their existing thread. */
        renderer_done_bh = qemu_bh_new(renderer_done, NULL);
        renderer_worker = brw_new(renderer_notify, renderer_done_bh);
#ifdef CONFIG_DARWIN
        renderer_worker->execute = breenix_renderer_invoke;
#endif
    }
    virtio_gpu_gl_schedule(g, virtio_gpu_gl_ctrl_work, g);
}

static void virtio_gpu_gl_reset_main(void *opaque)
{
    VirtIODevice *vdev = opaque;
    VirtIOGPU *g = VIRTIO_GPU(vdev);
    VirtIOGPUGL *gl = VIRTIO_GPU_GL(vdev);

    renderer_quiesce();
    virtio_gpu_reset(vdev);

    /*
     * GL functions must be called with the associated GL context in main
     * thread, and when the renderer is unblocked.
     */
    if (gl->renderer_state == RS_INITED) {
        virtio_gpu_virgl_reset_scanout(g);
        gl->renderer_state = RS_RESET;
    }
}

typedef struct RendererReset {
    VirtIODevice *vdev;
    QemuCond cond;
    bool done;
} RendererReset;

static void renderer_reset_bh(void *opaque)
{
    RendererReset *reset = opaque;
    virtio_gpu_gl_reset_main(reset->vdev);
    reset->done = true;
    renderer_quiescing = false;
    qemu_cond_signal(&reset->cond);
}

static void virtio_gpu_gl_reset(VirtIODevice *vdev)
{
    if (qemu_in_vcpu_thread()) {
        RendererReset reset = { .vdev = vdev };
        renderer_quiescing = true;
        qemu_cond_init(&reset.cond);
        aio_bh_schedule_oneshot(qemu_get_aio_context(), renderer_reset_bh, &reset);
        while (!reset.done) {
            qemu_cond_wait_bql(&reset.cond);
        }
        qemu_cond_destroy(&reset.cond);
    } else {
        virtio_gpu_gl_reset_main(vdev);
        renderer_quiescing = false;
    }
}

static void virtio_gpu_gl_device_realize(DeviceState *qdev, Error **errp)
{
    ERRP_GUARD();
    VirtIOGPU *g = VIRTIO_GPU(qdev);

#if HOST_BIG_ENDIAN
    error_setg(errp, "virgl is not supported on bigendian platforms");
    return;
#endif

    if (!object_resolve_path_type("", TYPE_VIRTIO_GPU_GL, NULL)) {
        error_setg(errp, "at most one %s device is permitted", TYPE_VIRTIO_GPU_GL);
        return;
    }

    if (!display_opengl) {
        error_setg(errp,
                   "The display backend does not have OpenGL support enabled");
        error_append_hint(errp,
                          "It can be enabled with '-display BACKEND,gl=on' "
                          "where BACKEND is the name of the display backend "
                          "to use.\n");
        return;
    }

    g->parent_obj.conf.flags |= (1 << VIRTIO_GPU_FLAG_VIRGL_ENABLED);
    g->capset_ids = virtio_gpu_virgl_get_capsets(g);
    VIRTIO_GPU_BASE(g)->virtio_config.num_capsets = g->capset_ids->len;

#if VIRGL_VERSION_MAJOR >= 1
    g->parent_obj.conf.flags |= 1 << VIRTIO_GPU_FLAG_CONTEXT_INIT_ENABLED;
#endif

    virtio_gpu_device_realize(qdev, errp);
}

static const Property virtio_gpu_gl_properties[] = {
    DEFINE_PROP_BIT("stats", VirtIOGPU, parent_obj.conf.flags,
                    VIRTIO_GPU_FLAG_STATS_ENABLED, false),
    DEFINE_PROP_BIT("venus", VirtIOGPU, parent_obj.conf.flags,
                    VIRTIO_GPU_FLAG_VENUS_ENABLED, false),
    DEFINE_PROP_BIT("neptune", VirtIOGPU, parent_obj.conf.flags,
                    VIRTIO_GPU_FLAG_NEPTUNE_ENABLED, false),
};

static void virtio_gpu_gl_device_unrealize(DeviceState *qdev)
{
    VirtIOGPU *g = VIRTIO_GPU(qdev);
    VirtIOGPUGL *gl = VIRTIO_GPU_GL(qdev);

    renderer_quiesce();
    if (gl->renderer_state >= RS_INITED) {
        virtio_gpu_virgl_reset_scanout(g);
        /* Stop renderer fence producers while their BHs are still alive. */
        virgl_renderer_cleanup(NULL);
#if VIRGL_VERSION_MAJOR >= 1
        qemu_bh_delete(gl->cmdq_resume_bh);

        if (gl->async_fence_bh) {
            virtio_gpu_virgl_reset_async_fences(g);
            qemu_bh_delete(gl->async_fence_bh);
        }
#endif
        if (virtio_gpu_stats_enabled(g->parent_obj.conf)) {
            timer_free(gl->print_stats);
        }
        timer_free(gl->fence_poll);
    }

    if (renderer_worker) {
        virtio_gpu_renderer_call(renderer_destroy_share, NULL);
        brw_stop(renderer_worker);
        brw_complete(renderer_worker);
        qemu_bh_delete(renderer_done_bh);
        brw_free(renderer_worker);
        renderer_worker = NULL;
        renderer_done_bh = NULL;
    }
    renderer_quiescing = false;
    gl->renderer_state = RS_START;

    g_array_unref(g->capset_ids);
}

static void virtio_gpu_gl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    VirtIOGPUBaseClass *vbc = VIRTIO_GPU_BASE_CLASS(klass);
    VirtIOGPUClass *vgc = VIRTIO_GPU_CLASS(klass);

    vbc->gl_flushed = virtio_gpu_gl_flushed;
    vgc->schedule = virtio_gpu_gl_schedule;
    vgc->handle_ctrl = virtio_gpu_gl_handle_ctrl;
    vgc->process_cmd = virtio_gpu_virgl_process_cmd;
    vgc->update_cursor_data = virtio_gpu_gl_update_cursor_data;
    vgc->resource_destroy = virtio_gpu_virgl_resource_destroy;

    vdc->realize = virtio_gpu_gl_device_realize;
    vdc->unrealize = virtio_gpu_gl_device_unrealize;
    vdc->reset = virtio_gpu_gl_reset;
    device_class_set_props(dc, virtio_gpu_gl_properties);
}

static const TypeInfo virtio_gpu_gl_info = {
    .name = TYPE_VIRTIO_GPU_GL,
    .parent = TYPE_VIRTIO_GPU,
    .instance_size = sizeof(VirtIOGPUGL),
    .class_init = virtio_gpu_gl_class_init,
};
module_obj(TYPE_VIRTIO_GPU_GL);
module_kconfig(VIRTIO_GPU);

static void virtio_register_types(void)
{
    type_register_static(&virtio_gpu_gl_info);
}

type_init(virtio_register_types)

module_dep("hw-display-virtio-gpu");
module_dep("ui-opengl");
