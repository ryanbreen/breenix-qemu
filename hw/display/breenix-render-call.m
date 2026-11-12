/* SPDX-License-Identifier: MIT */
#include "qemu/osdep.h"
#import <Foundation/Foundation.h>
#include "breenix-render-call.h"

void breenix_renderer_invoke(void (*fn)(void *), void *arg)
{
    @autoreleasepool {
        fn(arg);
    }
}
