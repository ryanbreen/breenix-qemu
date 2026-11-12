/* SPDX-License-Identifier: MIT */
#ifndef BREENIX_QEMU_GL_PIN_H
#define BREENIX_QEMU_GL_PIN_H
/* Standalone builds use the fork version; a packager can override the cache
 * namespace using -DBREENIX_QEMU_GL_PIN=\"bundle-version\" in extra-cflags. */
#ifndef BREENIX_QEMU_GL_PIN
#define BREENIX_QEMU_GL_PIN "breenix-qemu-10.0.12-worker-v2"
#endif
#endif
