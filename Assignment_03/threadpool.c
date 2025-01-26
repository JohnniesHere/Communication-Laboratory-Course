#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>

threadpool* create_threadpool(int num_threads_in_pool, int max_queue_size) {
    // Input validation
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL ||
        max_queue_size <= 0 || max_queue_size > MAXW_IN_QUEUE) {
        return NULL;
    }

    threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
    if (pool == NULL) {
        return NULL;
    }

    // Initialize members
    pool->num_threads = num_threads_in_pool;
    pool->max_qsize = max_queue_size;
    pool->qsize = 0;
    pool->qhead = NULL;
    pool->qtail = NULL;
    pool->shutdown = 0;
    pool->dont_accept = 0;

    // Initialize mutex and condition variables
    if (pthread_mutex_init(&pool->qlock, NULL) != 0) {
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->q_not_empty, NULL) != 0) {
        pthread_mutex_destroy(&pool->qlock);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->q_empty, NULL) != 0) {
        pthread_mutex_destroy(&pool->qlock);
        pthread_cond_destroy(&pool->q_not_empty);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->q_not_full, NULL) != 0) {
        pthread_mutex_destroy(&pool->qlock);
        pthread_cond_destroy(&pool->q_not_empty);
        pthread_cond_destroy(&pool->q_empty);
        free(pool);
        return NULL;
    }

    // Create array of threads
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads_in_pool);
    if (pool->threads == NULL) {
        pthread_mutex_destroy(&pool->qlock);
        pthread_cond_destroy(&pool->q_not_empty);
        pthread_cond_destroy(&pool->q_empty);
        pthread_cond_destroy(&pool->q_not_full);
        free(pool);
        return NULL;
    }

    // Create the threads
    for (int i = 0; i < num_threads_in_pool; i++) {
        if (pthread_create(&pool->threads[i], NULL, do_work, pool) != 0) {
            destroy_threadpool(pool);
            return NULL;
        }
    }

    return pool;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) {
    if (from_me == NULL || dispatch_to_here == NULL) {
        return;
    }

    work_t* work = (work_t*)malloc(sizeof(work_t));
    if (work == NULL) {
        return;
    }
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;

    pthread_mutex_lock(&from_me->qlock);

    // Don't accept if destruction has begun
    if (from_me->dont_accept) {
        pthread_mutex_unlock(&from_me->qlock);
        free(work);
        return;
    }

    // Wait if queue is full
    while (from_me->qsize >= from_me->max_qsize && !from_me->dont_accept) {
        pthread_cond_wait(&from_me->q_not_full, &from_me->qlock);
    }

    // Check again after waiting
    if (from_me->dont_accept) {
        pthread_mutex_unlock(&from_me->qlock);
        free(work);
        return;
    }

    // Add work to queue
    if (from_me->qsize == 0) {
        from_me->qhead = work;
        from_me->qtail = work;
    } else {
        from_me->qtail->next = work;
        from_me->qtail = work;
    }
    from_me->qsize++;

    pthread_cond_signal(&from_me->q_not_empty);
    pthread_mutex_unlock(&from_me->qlock);
}

void* do_work(void* p) {
    threadpool* pool = (threadpool*)p;
    work_t* work;

    while (1) {
        pthread_mutex_lock(&pool->qlock);

        // Exit if shutting down
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->qlock);
            pthread_exit(NULL);
        }

        // Wait for work
        while (pool->qsize == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->q_not_empty, &pool->qlock);
        }

        // Check shutdown again
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->qlock);
            pthread_exit(NULL);
        }

        // Get work
        work = pool->qhead;
        pool->qsize--;

        if (pool->qsize == 0) {
            pool->qhead = NULL;
            pool->qtail = NULL;
            if (pool->dont_accept) {
                pthread_cond_signal(&pool->q_empty);
            }
        } else {
            pool->qhead = work->next;
        }

        // Signal space available
        pthread_cond_signal(&pool->q_not_full);
        pthread_mutex_unlock(&pool->qlock);

        // Execute work
        if (work != NULL) {
            work->routine(work->arg);
            free(work);
        }
    }
    return NULL;
}

void destroy_threadpool(threadpool* destroyme) {
    if (destroyme == NULL) {
        return;
    }

    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;

    // Wait for queue to empty
    while (destroyme->qsize > 0) {
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }

    destroyme->shutdown = 1;
    pthread_cond_broadcast(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);

    // Wait for threads to finish
    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);
    }

    // Cleanup
    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_cond_destroy(&destroyme->q_empty);
    pthread_cond_destroy(&destroyme->q_not_full);
    free(destroyme->threads);
    free(destroyme);
}