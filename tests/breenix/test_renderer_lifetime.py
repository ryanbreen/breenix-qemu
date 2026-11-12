#!/usr/bin/env python3
"""Exercise production BH setup and quiesce bodies without a guest or GL."""
import os
from pathlib import Path
import re
import resource
import signal
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(text, name):
    match = re.search(r'^.*\b' + name + r'\([^;]*?\)\n\{', text, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


class RendererLifetime(unittest.TestCase):
    def test_response_mmio_reset_and_self_quiesce(self):
        source = (ROOT / 'hw/display/virtio-gpu-gl.c').read_text()
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define G_GNUC_UNUSED __attribute__((unused))
typedef void Coroutine;
typedef struct { int state; } VirtIOGPU;
typedef VirtIOGPU VirtIODevice;
typedef VirtIOGPU VirtQueue;
typedef struct { int renderer_state; } VirtIOGPUGL;
typedef struct { int major_ver, minor_ver; } QEMUGLParams;
typedef struct { bool guarded; } QEMUBH;
typedef struct { int unused; } BRW;
#define VIRTIO_GPU(p) ((VirtIOGPU *)(p))
#define VIRTIO_GPU_GL(p) ((VirtIOGPUGL *)(p))
#define DEVICE(p) (p)
#define RS_INIT_FAILED 3
static BRW *renderer_worker;
static QEMUBH *renderer_done_bh;
static Coroutine *renderer_co;
static bool renderer_quiescing, engaged;
static int self_token;
static void *breenix_renderer_share;
static void *qemu_egl_display = (void *)1;
static Coroutine *qemu_coroutine_self(void) { return &self_token; }
static void main_loop_wait(bool unused) { (void)unused; exit(77); }
#define AIO_WAIT_WHILE(ctx, cond) do { while (cond) main_loop_wait(false); } while (0)
QUIESCE
static void reset_from_response(void) {
    /* Like memory_region_access_valid: a BAR access re-entering this
     * transport is refused while its BH's mem_reentrancy_guard is set. */
    if (!engaged) renderer_quiesce();
}
static void brw_complete(BRW *w) {
    (void)w;
    renderer_co = &self_token;
    reset_from_response();
    renderer_co = NULL;
}
DONE
static QEMUBH bh;
static QEMUBH *qemu_bh_new(void (*cb)(void *), void *arg) {
    (void)cb; (void)arg; bh.guarded = false; return &bh;
}
static QEMUBH *virtio_bh_io_new_guarded(void *dev, void (*cb)(void *), void *arg) {
    (void)dev; (void)cb; (void)arg; bh.guarded = true; return &bh;
}
static void renderer_notify(void *opaque) { (void)opaque; }
static BRW worker;
static BRW *brw_new(void (*notify)(void *), void *arg) {
    (void)notify; (void)arg; return &worker;
}
static void virtio_gpu_gl_schedule(VirtIOGPU *g, void (*fn)(void *), void *arg) {
    (void)g; (void)fn; (void)arg;
}
static void virtio_gpu_gl_ctrl_work(void *arg) { (void)arg; }
static void *dpy_gl_ctx_create(void *con, QEMUGLParams *params) {
    (void)con; (void)params; return (void *)1;
}
HANDLE
int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "self")) {
        renderer_co = &self_token;
        renderer_quiesce();
        return 78;
    }
    VirtIOGPU gpu = {0};
    virtio_gpu_gl_handle_ctrl(&gpu, &gpu);
    assert(renderer_done_bh);
    engaged = renderer_done_bh->guarded;
    renderer_done(NULL);
    assert(!renderer_co);
    /* Keep both BH constructors referenced on either source version. */
    (void)qemu_bh_new; (void)virtio_bh_io_new_guarded;
    (void)qemu_coroutine_self;
    return 0;
}
'''
        handle = function(source, 'virtio_gpu_gl_handle_ctrl')
        # Console lookup is irrelevant to the transport reset mechanism.
        handle = handle.replace('g->parent_obj.scanout[0].con', 'NULL')
        handle = handle.replace('VirtQueue *vq)', 'VirtQueue *vq G_GNUC_UNUSED)')
        code = code.replace('QUIESCE', function(source, 'renderer_quiesce'))
        code = code.replace('DONE', function(source, 'renderer_done'))
        code = code.replace('HANDLE', handle)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'reset.c'
            path.write_text(code)
            exe = path.with_suffix('')
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(path), '-o', str(exe)], check=True)
            def no_core():
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            for mode, expected in [('response', 0), ('self', -signal.SIGABRT)]:
                with self.subTest(mode=mode):
                    result = subprocess.run([str(exe), mode], capture_output=True,
                                            timeout=3, preexec_fn=no_core)
                    self.assertEqual(result.returncode, expected, result.stderr.decode())


if __name__ == '__main__':
    unittest.main()
