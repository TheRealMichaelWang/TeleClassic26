#ifndef MICHAEL_THREADS_THREAD_POOL_H
#define MICHAEL_THREADS_THREAD_POOL_H

#include <plibsys.h>

// Constants that determine the max size of buffers; feel free to modify
#define TC_THREADS_MAX_BUFFER_SIZE 64 
#define TC_THREADS_MAX_THREADS 16

typedef void (*tc_thread_pool_task_func_t)(void *arg);

typedef enum {
    TC_THREAD_POOL_TASK_PRIORITY_LOW = 0,
    TC_THREAD_POOL_TASK_PRIORITY_MEDIUM = 1,
    TC_THREAD_POOL_TASK_PRIORITY_HIGH = 2,
} tc_thread_pool_task_priority_t;

typedef struct {
    tc_thread_pool_task_priority_t priority;
    tc_thread_pool_task_func_t func;
    void *arg;
} tc_thread_pool_task_t;

typedef struct {
    tc_thread_pool_task_t buffer[TC_THREADS_MAX_BUFFER_SIZE];
    psize head_index;
    psize tail_index;
} tc_thread_pool_task_buf_t;

typedef struct {
    tc_thread_pool_task_buf_t task_prio_buffer[3];

    PUThread *thread_buffer[TC_THREADS_MAX_THREADS];
    psize num_threads;

    PMutex *lock;
    PCondVariable *not_empty;
    pboolean shutdown;
} tc_thread_pool_t;

// Initialize the thread pool
pboolean tc_thread_pool_init(tc_thread_pool_t *pool, psize reserved_threads);

// Finalize the thread pool
// - CANNOT BE INVOKED FROM A THREAD WITHIN THE POOL
// - SHOULD BE OBVIOUS BUT POOL CANNOT BE SUBSEQUENTLY USED AFTER FINALIZATION
void tc_thread_pool_finalize(tc_thread_pool_t *pool);

// Add a task to the thread pool
// - return: TRUE if the task was added, FALSE otherwise
// - func: function to execute
// - arg: argument to pass to the function
// - priority: priority of the task
pboolean tc_thread_pool_add_task(
    tc_thread_pool_t *pool, 
    tc_thread_pool_task_func_t func, 
    void *arg, 
    tc_thread_pool_task_priority_t priority,
    pboolean is_yield
);

#endif /* MICHAEL_THREADS_THREAD_POOL_H */