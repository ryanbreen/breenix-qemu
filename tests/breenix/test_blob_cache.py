#!/usr/bin/env python3
"""Force cache contention, replacement accounting and optional-EGL failures."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
PREFIX = Path(os.environ.get('QEMU_GL_PREFIX', '/tmp/breenix-qemu-gl-build/prefix'))

class BlobCache(unittest.TestCase):
    def test_disk_callbacks_and_optional_install(self):
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <epoxy/egl.h>
static size_t cache_confstr(int name, char *buffer, size_t size) {
    (void)name; memset(buffer, 'x', size); return size + 1;
}
static bool extension = true;
static bool has_extension(EGLDisplay d, const char *name) {
    (void)d; (void)name; return extension;
}
static void install(EGLDisplay d, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get) {
    (void)d; (void)set; (void)get; abort();
}
static __eglMustCastToProperFunctionPointerType proc(const char *name) {
    (void)name; return (__eglMustCastToProperFunctionPointerType)install;
}
static unsigned queries;
static const char *query(EGLDisplay d, EGLint name) {
    (void)d; (void)name; queries++; return "mock";
}
#define confstr cache_confstr
#define epoxy_has_egl_extension has_extension
#undef eglGetProcAddress
#define eglGetProcAddress proc
#undef eglQueryString
#define eglQueryString query
#include "breenix-blob-cache.h"
static void sparse(const char *path, size_t size) {
    int fd = open(path, O_CREAT|O_RDWR, 0600); assert(fd >= 0);
    assert(ftruncate(fd, size) == 0); close(fd);
}
int main(int argc, char **argv) {
    assert(argc == 3);
    if (!strcmp(argv[1], "truncated")) {
        breenix_blob_cache_install((void *)1, "pin");
        assert(!bbc_directory); assert(queries == 0); return 0;
    }
    if (!strcmp(argv[1], "extension")) {
        extension = false;
        breenix_blob_cache_install((void *)1, "pin");
        assert(!bbc_directory); return 0;
    }
    bbc_directory = g_strdup(argv[2]);
    char *lock = g_build_filename(bbc_directory, ".lock", NULL);
    bbc_lock = open(lock, O_CREAT|O_RDWR, 0600); assert(bbc_lock >= 0);
    const char key[] = "key", other[] = "other", value[] = "value";
    if (!strcmp(argv[1], "locked")) {
        int pipefd[2], release[2];
        assert(pipe(pipefd) == 0); assert(pipe(release) == 0);
        pid_t pid = fork(); assert(pid >= 0);
        if (!pid) {
            close(pipefd[0]); close(release[1]);
            int fd = open(lock, O_RDWR); assert(fd >= 0);
            assert(flock(fd, LOCK_EX) == 0);
            assert(write(pipefd[1], "!", 1) == 1);
            char c; (void)read(release[0], &c, 1);
            _exit(0);
        }
        close(pipefd[1]); close(release[0]);
        char c; assert(read(pipefd[0], &c, 1) == 1);
        assert(bbc_get(key, sizeof(key), NULL, 0) == 0);
        bbc_set(key, sizeof(key), value, sizeof(value));
        close(release[1]); assert(waitpid(pid, NULL, 0) == pid);
        return 0;
    }
    char *path = bbc_path(key, sizeof(key));
    char *other_path = bbc_path(other, sizeof(other));
    if (!strcmp(argv[1], "replace")) {
        sparse(path, BBC_TOTAL_MAX - 1024);
        sparse(other_path, 1024);
        struct timespec old[2] = {{1, 0}, {1, 0}};
        assert(utimensat(AT_FDCWD, other_path, old, 0) == 0);
        bbc_set(key, sizeof(key), value, sizeof(value));
        assert(access(other_path, F_OK) == 0);
    } else {
        bbc_set(key, sizeof(key), value, sizeof(value));
        struct timespec old[2] = {{1, 0}, {1, 0}};
        assert(utimensat(AT_FDCWD, path, old, 0) == 0);
        char buffer[sizeof(value)];
        assert(bbc_get(key, sizeof(key), buffer, sizeof(buffer)) == sizeof(value));
        struct stat st; assert(stat(path, &st) == 0); assert(st.st_mtime > 1);
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            host = tmp / 'cache.c'; host.write_text(code)
            exe = tmp / 'cache'
            flags = shlex.split(subprocess.check_output(
                ['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(ROOT / 'ui'), '-I' + str(PREFIX / 'include'),
                            str(host), '-o', str(exe), *flags], check=True)
            for mode in ('locked', 'replace', 'touch', 'extension', 'truncated'):
                with self.subTest(mode=mode):
                    directory = tmp / mode; directory.mkdir()
                    try:
                        result = subprocess.run([str(exe), mode, str(directory)],
                                                capture_output=True, text=True, timeout=3)
                    except subprocess.TimeoutExpired:
                        self.fail(mode + ': cache blocked on a held process lock')
                    self.assertEqual(result.returncode, 0, result.stderr)
                    if mode == 'extension':
                        self.assertIn('EGL_ANDROID_blob_cache unavailable', result.stderr)

if __name__ == '__main__':
    unittest.main()
