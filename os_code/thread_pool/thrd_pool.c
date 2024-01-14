#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "thrd_pool.h"

typedef struct task_t {
    handler_pt func;
    void * arg;
} task_t;

typedef struct task_queue_t {
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    task_t *queue;
} task_queue_t;

struct thread_pool_t {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t *threads;
    task_queue_t task_queue;

    int closed;
    int started; // 当前运行的线程数

    int thrd_count;
    int queue_size;
};

static void * thread_worker(void *thrd_pool);
static void thread_pool_free(thread_pool_t *pool);

thread_pool_t * thread_pool_create(int thrd_count, int queue_size) {
    thread_pool_t *pool;

    if (thrd_count <= 0 || queue_size <= 0) {
        return NULL;
    }

    pool = (thread_pool_t*) malloc(sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }

    pool->thrd_count = 0;
    pool->queue_size = queue_size;
    pool->task_queue.head = 0;
    pool->task_queue.tail = 0;
    pool->task_queue.count = 0;

    pool->started = pool->closed = 0;

    pool->task_queue.queue = (task_t*)malloc(sizeof(task_t)*queue_size);
    if (pool->task_queue.queue == NULL) {
        // TODO: free pool
        return NULL;
    }

    pool->threads = (pthread_t*) malloc(sizeof(pthread_t) * thrd_count);
    if (pool->threads == NULL) {
        // TODO: free pool
        return NULL;
    }

    int i = 0;
    for (; i < thrd_count; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, thread_worker, (void*)pool) != 0) {
            // TODO: free pool
            return NULL;
        }
        pool->thrd_count++;
        pool->started++;
    }
    return pool;
}

int thread_pool_post(thread_pool_t *pool, handler_pt func, void *arg) {
    if (pool == NULL || func == NULL) {
        return -1;
    }

    task_queue_t *task_queue = &(pool->task_queue);

    if (pthread_mutex_lock(&(pool->mutex)) != 0) {
        return -2;
    }

    if (pool->closed) {
        pthread_mutex_unlock(&(pool->mutex));
        return -3;
    }

    if (task_queue->count == pool->queue_size) {
        pthread_mutex_unlock(&(pool->mutex));
        return -4;
    }

    task_queue->queue[task_queue->tail].func = func;
    task_queue->queue[task_queue->tail].arg = arg;
    task_queue->tail = (task_queue->tail + 1) % pool->queue_size;
    task_queue->count++;

    if (pthread_cond_signal(&(pool->condition)) != 0) {
        pthread_mutex_unlock(&(pool->mutex));
        return -5;
    }
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

static void  thread_pool_free(thread_pool_t *pool) {
    if (pool == NULL || pool->started > 0) {
        return;
    }

    if (pool->threads) {
        free(pool->threads);
        pool->threads = NULL;

        pthread_mutex_lock(&(pool->mutex));
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->condition);
    }

    if (pool->task_queue.queue) {
        free(pool->task_queue.queue);
        pool->task_queue.queue = NULL;
    }
    free(pool);
}

int wait_all_done(thread_pool_t *pool) {
    int i, ret=0;
    for (i=0; i < pool->thrd_count; i++) {
        if (pthread_join(pool->threads[i], NULL) != 0) {
            ret=1;
        }
    }
    return ret;
}

int thread_pool_destroy(thread_pool_t *pool) {
    if (pool == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&(pool->mutex)) != 0) {
        return -2;
    }

    if (pool->closed) {
        thread_pool_free(pool);
        return -3;
    }

    pool->closed = 1;

    if (pthread_cond_broadcast(&(pool->condition)) != 0 || 
            pthread_mutex_unlock(&(pool->mutex)) != 0) {
        thread_pool_free(pool);
        return -4;
    }

    wait_all_done(pool);

    thread_pool_free(pool);
    return 0;
}

static void * thread_worker(void *thrd_pool) {
    thread_pool_t *pool = (thread_pool_t*)thrd_pool;
    task_queue_t *que;
    task_t task;
    for (;;) {
        pthread_mutex_lock(&(pool->mutex));
        que = &pool->task_queue;
        // 虚假唤醒   linux  pthread_cond_signal
        // linux 可能被信号唤醒
        // 业务逻辑不严谨，被其他线程抢了该任务
        while (que->count == 0 && pool->closed == 0) {
            // pthread_mutex_unlock(&(pool->mutex))
            // 阻塞在 condition
            // ===================================
            // 解除阻塞
            // pthread_mutex_lock(&(pool->mutex));
            pthread_cond_wait(&(pool->condition), &(pool->mutex));
        }
        if (pool->closed == 1) break;
        task = que->queue[que->head];
        que->head = (que->head + 1) % pool->queue_size;
        que->count--;
        pthread_mutex_unlock(&(pool->mutex));
        (*(task.func))(task.arg);
    }
    pool->started--;
    pthread_mutex_unlock(&(pool->mutex));
    pthread_exit(NULL);
    return NULL;
}