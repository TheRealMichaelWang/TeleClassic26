#ifndef TELECLASSIC26_TASK_BACKLOG_H
#define TELECLASSIC26_TASK_BACKLOG_H

#include <plibsys.h>
#include <TeleClassic26/thread_pool.h>

#define TC_TASK_BACKLOG_BUFFER_SIZE 32

/*
    Task Backlog

    The task backlog is a special buffer. When running a blocking task that produces a result
    that other future tasks depend on, these future tasks are pushed to the task backlog

    Once the blocking task completes, all tasks in the backlog are scheduled in order onto the thread pool

    Note that in case of scheduling failure (sometimes the task pool is busy/full), we execute a special failure handler
    that usually kicks the session. The failure handler must be short and cannot fail.
*/

typedef void (*tc_task_backlog_failure_handler_t)(void *context, pint session_generation);

typedef struct tc_task_backlog_args {
    void *result;
    void *context;
} tc_task_backlog_args_t;

typedef void (*tc_task_backlog_success_handler_t)(tc_task_backlog_args_t* args, tc_thread_pool_task_priority_t priority, pint session_generation);

typedef struct tc_task_backlog_entry {
    tc_task_backlog_success_handler_t success_handler; //the success handler to be scheduled if the main task is scheduled
    tc_task_backlog_failure_handler_t failure_handler; //the failure handler to be called if the main task cannot be scheduled

    void* context;
    
    tc_thread_pool_task_priority_t priority;
    pint session_generation;
} tc_task_backlog_entry_t;

typedef struct tc_task_backlog_block tc_task_backlog_block_t;

typedef struct tc_task_backlog_block {
    tc_task_backlog_entry_t entries[TC_TASK_BACKLOG_BUFFER_SIZE];
    psize count;

    tc_task_backlog_block_t* next_block;
} tc_task_backlog_block_t;

typedef struct tc_task_backlog {
    tc_task_backlog_block_t* head_block;
    tc_task_backlog_block_t* tail_block;

    PMutex* lock;
} tc_task_backlog_t;

// Initialize the task backlog
pboolean tc_task_backlog_initialize(tc_task_backlog_t* backlog);

// Finalize the task backlog
void tc_task_backlog_finalize(tc_task_backlog_t* backlog);

// Push a new task to the backlog
pboolean tc_task_backlog_push(tc_task_backlog_t* backlog, tc_task_backlog_entry_t entry);

// schedule all tasks in the backlog onto the thread pool
// - result: the result of the blocking task. Pass NULL if failure, then failure handlers will be invoked
void tc_task_schedule_backlog(tc_task_backlog_t* backlog, tc_thread_pool_t* pool, void* result);

#endif /* TELECLASSIC26_TASK_BACKLOG_H */