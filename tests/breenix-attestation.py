#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the production writer without a guest or GL context."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--checker', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / 'hw/display/virtio-gpu-virgl.c').read_text()
block = source[source.index('#define BREENIX_ATTEST'):source.index('static void breenix_attest(const char *event')]
stub = r'''
#include <glib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#define GL_VENDOR 1
#define GL_RENDERER 2
#define GL_VERSION 3
#define EGL_VENDOR 4
#define EGL_VERSION 5
#define EGL_NO_CONTEXT 0
#define error_report(...) fprintf(stderr, __VA_ARGS__)
static int qemu_egl_display=1;
static int eglGetCurrentContext(void) { return 1; }
static const char *renderer="ANGLE Metal Renderer: Apple fixture";
static const char *glGetString(int key) { return key==GL_RENDERER ? renderer : "fixture"; }
static const char *eglQueryString(int display,int key) { g_assert(display==1); g_assert(key>=4); return "fixture"; }
BLOCK
static void event(const char *name, uint32_t id) {
    BreenixAttest a={ name,id }; breenix_attest_job(&a);
}
int main(int argc,char **argv) {
    g_assert(argc==3);
    g_setenv("BREENIX_HOST_RENDERER_JSON",argv[1],true);
    breenix_attest_reset(NULL);
    g_assert(breenix_attest_submitted==NULL);
    if (!strcmp(argv[2],"software")) renderer="llvmpipe Software";
    if (!strcmp(argv[2],"missing")) event("first-submit-3d",9999);
    for (unsigned i=1;i<=70;i++) {
        event("context-create",i); event("first-submit-3d",i);
        renderer="ANGLE Metal Renderer: Apple fixture";
    }
    event("context-create",71);
    OVERFLOW
    return 0;
}
'''.replace('BLOCK', block)
# The overflow mode targets the production byte bound without a large output.
overflow = '''if (!strcmp(argv[2],"overflow")) {
    breenix_attest_bytes=BREENIX_ATTEST_MAX_BYTES;
    event("first-submit-3d",71);
    event("context-create",72);
}'''
stub = stub.replace('OVERFLOW', overflow if 'BREENIX_ATTEST_MAX_BYTES' in block else '')
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / 'writer.c').write_text(stub)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
    subprocess.run(['taskpolicy', '-b', 'cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    str(path / 'writer.c'), '-o', str(path / 'writer'), *flags], check=True)
    for mode in ['hardware', 'software', 'missing', 'overflow']:
        output = path / f'{mode}.json'
        subprocess.run([str(path / 'writer'), str(output), mode], check=True)
        result = subprocess.run(['python3', str(args.checker), str(output)], capture_output=True, text=True)
        if mode == 'hardware':
            assert result.returncode == 0, result.stdout + result.stderr
            records = json.loads(output.read_text())['records']
            assert len(records) == 141, len(records)
            assert records[0]['ctx_id'] == 1
        else:
            assert result.returncode == 1, (mode, result.stdout)
            reason = {'software':'software renderer', 'missing':'no context-create', 'overflow':'attestation-overflow'}[mode]
            assert reason in result.stdout, result.stdout
        print(mode, result.stdout.strip())
