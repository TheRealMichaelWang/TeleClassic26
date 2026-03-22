#ifndef MICHAEL_THREADS_THREAD_POOL_H
#define MICHAEL_THREADS_THREAD_POOL_H

#include <plibsys.h>

// Constants that determine the max size of buffers; feel free to modify
#define TC_THREADS_MAX_BUFFER_SIZE 64 
#define TC_THREADS_MAX_THREADS 16
#define TC_THREAD_POOL_MAX_PRIORITY 3

typedef void (*tc_thread_pool_task_t)(void *arg);

typedef enum tc_thread_pool_task_priority {
    TC_THREAD_POOL_TASK_PRIORITY_HIGH = 0,
    TC_THREAD_POOL_TASK_PRIORITY_MEDIUM = 1,
    TC_THREAD_POOL_TASK_PRIORITY_LOW = 2
} tc_thread_pool_task_priority_t;

typedef struct tc_thread_pool_context {
    tc_thread_pool_task_priority_t priority;
    tc_thread_pool_task_t func;
    void *arg;
} tc_thread_pool_context_t;

typedef struct {
    tc_thread_pool_context_t buffer[TC_THREADS_MAX_BUFFER_SIZE];
    psize head_index;
    psize tail_index;
} tc_thread_pool_task_buf_t;

typedef struct {
    tc_thread_pool_task_buf_t task_prio_buffer[3];

    PUThread *thread_buffer[TC_THREADS_MAX_THREADS];
    psize num_threads;

    PMutex *lock;
    PCondVariable *not_empty;
    
    pint active_threads;
    pboolean shutdown;
} tc_thread_pool_t;

// Initialize the thread pool
// - return: TRUE if the thread pool was initialized, FALSE otherwise
// - reserved_threads: number of threads to reserve for other purposes outside of the thread pool
pboolean tc_thread_pool_init(tc_thread_pool_t *pool, psize reserved_threads);

// Finalize the thread pool
// - waits for the thread pool to be stopped
// - CANNOT BE INVOKED FROM A WORKER THREAD
// - SHOULD BE OBVIOUS BUT POOL CANNOT BE SUBSEQUENTLY USED AFTER FINALIZATION
void tc_thread_pool_finalize(tc_thread_pool_t *pool);

// Stop the thread pool
// - sets the shutdown flag to TRUE
// - signals all worker threads that the thread pool is shutting down
// - CAN BE INVOKED FROM ANY THREAD including in worker threads
void tc_thread_pool_stop(tc_thread_pool_t *pool);

// Schedule a new task to the thread pool
// - return: TRUE if the task was scheduled, FALSE otherwise
// - func: function to execute
// - arg: argument to pass to the function
// - priority: priority of the task
// NOTE: use this to start a new task chain
pboolean tc_thread_schedule_new(
    tc_thread_pool_t *pool,
    tc_thread_pool_task_t new_task,
    void *arg,
    tc_thread_pool_task_priority_t priority
);

// Schedule a next task to the thread pool
// - return: TRUE if the task was scheduled, FALSE otherwise
// - next_task: function to execute
// - shutdown_task: executed when pool is shutting down; you may pass NULL to nop
// - priority: priority of the task
// NOTE: use this to schedule the next task in a task chain
// NOTE: priority must be the same as the priority of the calling task!
void tc_thread_schedule_next(
    tc_thread_pool_t *pool,
    tc_thread_pool_task_t next_task,
    tc_thread_pool_task_t shutdown_task,
    void *arg,
    tc_thread_pool_task_priority_t priority
);

#endif /* MICHAEL_THREADS_THREAD_POOL_H */