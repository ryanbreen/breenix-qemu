/* SPDX-License-Identifier: MIT */
#ifndef BREENIX_RENDER_WORKER_H
#define BREENIX_RENDER_WORKER_H
#include <glib.h>

typedef struct BRWJob {
    void (*run)(void *);
    void (*complete)(void *);
    void *opaque;
} BRWJob;

typedef struct BRW {
    GAsyncQueue *pending, *completed;
    GThread *thread;
    void (*notify)(void *);
    void (*execute)(void (*)(void *), void *);
    void *opaque;
} BRW;

static inline gpointer brw_thread(gpointer opaque)
{
    BRW *w = opaque;
    for (;;) {
        BRWJob *job = g_async_queue_pop(w->pending);
        if ((void *)job == (void *)w) {
            return NULL;
        }
        if (w->execute) {
            w->execute(job->run, job->opaque);
        } else {
            job->run(job->opaque);
        }
        g_async_queue_push(w->completed, job);
        w->notify(w->opaque);
    }
}

static inline BRW *brw_new(void (*notify)(void *), void *opaque)
{
    BRW *w = g_new0(BRW, 1);
    w->pending = g_async_queue_new();
    w->completed = g_async_queue_new();
    w->notify = notify;
    w->opaque = opaque;
    w->thread = g_thread_new("virgl-renderer", brw_thread, w);
    return w;
}

/* The submitter retains each job and all of its arguments until complete. */
static inline void brw_submit(BRW *w, BRWJob *job)
{
    g_assert(w->thread != NULL);
    g_async_queue_push(w->pending, job);
}

/* Only the main loop retires jobs; no renderer callback touches device state. */
static inline void brw_complete(BRW *w)
{
    BRWJob *job;
    while ((job = g_async_queue_try_pop(w->completed))) {
        job->complete(job->opaque);
    }
}

/* A FIFO barrier: all earlier calls finish before the thread exits. */
static inline void brw_stop(BRW *w)
{
    g_async_queue_push(w->pending, w);
    g_thread_join(w->thread);
    w->thread = NULL;
}

static inline void brw_free(BRW *w)
{
    g_assert(w->thread == NULL);
    g_assert(g_async_queue_length(w->pending) == 0);
    g_assert(g_async_queue_length(w->completed) == 0);
    g_async_queue_unref(w->pending);
    g_async_queue_unref(w->completed);
    g_free(w);
}
#endif
