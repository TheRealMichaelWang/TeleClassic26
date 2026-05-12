#include "plibsys.h"
#include <TeleClassic26/thread_pool.h>
#include <TeleClassic26/utils.h>
#include <TeleClassic26/log.h>

// initialize a task buffer
static void init_task_buffer(tc_thread_pool_task_buf_t *task_buf, tc_thread_pool_context_t* buffer, psize capacity) {
    task_buf->head_index = 0;
    task_buf->tail_index = 0;
    task_buf->buffer = buffer;
    task_buf->capacity = capacity;
}

// get the remaining capacity of the task buffer
static psize task_buffer_remaining_capacity(tc_thread_pool_task_buf_t *task_buf) {
    psize used = (task_buf->head_index + task_buf->capacity - task_buf->tail_index) % task_buf->capacity;
    psize remaining = (task_buf->capacity - 1) - used;
    return remaining;
}

// check if the task buffer is empty
static pboolean task_buffer_is_empty(tc_thread_pool_task_buf_t *task_buf) {
    return task_buf->head_index == task_buf->tail_index;
}

// whether the buffer at `priority_index` currently has runnable work for a
// worker; the blocking buffer is hidden once we've hit the concurrent-blocking
// cap so other workers don't busy-pick it just to sit on the lock.
static pboolean buffer_eligible(tc_thread_pool_t *pool, pint priority_index) {
    if (task_buffer_is_empty(&pool->task_prio_buffer[priority_index])) {
        return FALSE;
    }
    if (priority_index == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING &&
        pool->blocking_active >= pool->max_blocking_threads) {
        return FALSE;
    }
    return TRUE;
}

// check if the thread pool has tasks that a worker can currently run
static pboolean thread_pool_has_tasks(tc_thread_pool_t *pool) {
    for (pint i = 0; i < TC_THREAD_POOL_MAX_PRIORITY; i++) {
        if (buffer_eligible(pool, i)) {
            return TRUE;
        }
    }
    return FALSE;
}

// enqueue a task to the thread pool
// - return: TRUE if the task was enqueued, FALSE otherwise
// - task_buf: the task buffer to enqueue the task to
// - task: the task to enqueue
static pboolean task_buffer_enqueue(
    tc_thread_pool_t *pool,
    tc_thread_pool_task_buf_t *task_buf, 
    tc_thread_pool_context_t *task, 
    pboolean is_yield,
    pboolean is_blocking
) {
    psize remaining_capacity = task_buffer_remaining_capacity(task_buf);
    // we reserve pool->num_threads tasks in each thread buffer for yeild continuations
    psize reserved_capacity = is_blocking ? pool->max_blocking_threads : pool->num_threads;
    if (!is_yield && remaining_capacity <= reserved_capacity) { 
        return FALSE; // only yeild continuations are allowed to schedule new tasks
    }

    psize next_head = (task_buf->head_index + 1) % task_buf->capacity;
    if (next_head == task_buf->tail_index) {
        return FALSE;
    }
    task_buf->buffer[task_buf->head_index] = *task;
    task_buf->head_index = next_head;
    return TRUE;
}

// dequeue a task from the thread pool
// - return: valid tc_thread_pool_context_t if a task was dequeued, an invalid struct otherwise
static tc_thread_pool_context_t task_buffer_dequeue(tc_thread_pool_task_buf_t *task_buf) {
    if (task_buf->head_index == task_buf->tail_index) {
        return (tc_thread_pool_context_t){.func = NULL, .arg = NULL, .priority = TC_THREAD_POOL_TASK_PRIORITY_LOW};
    }
    tc_thread_pool_context_t task = task_buf->buffer[task_buf->tail_index];
    task_buf->tail_index = (task_buf->tail_index + 1) % task_buf->capacity;
    return task;
}

// dequeue a task from the thread pool; searches from highest priority to lowest 
static tc_thread_pool_context_t task_pool_dequeue(tc_thread_pool_t *pool) {
    // get the current buffer to dequeue from
    pchar current_buffer = pool->round_robin_pattern[pool->round_robin_index];
    if (current_buffer == 0) { // if the pattern is exhausted, reset the index
        pool->round_robin_index = 0;
        current_buffer = pool->round_robin_pattern[pool->round_robin_index];
    }
    pool->round_robin_index++;

    pint current_buffer_index = current_buffer - 'A';

    for (pint i = 0; i < TC_THREAD_POOL_MAX_PRIORITY; i++) {
        if (buffer_eligible(pool, current_buffer_index)) {
            return task_buffer_dequeue(&pool->task_prio_buffer[current_buffer_index]);
        }
        current_buffer_index += 1;
        current_buffer_index %= TC_THREAD_POOL_MAX_PRIORITY;
    }

    // should never happen
    return (tc_thread_pool_context_t){.func = NULL, .arg = NULL, .priority = TC_THREAD_POOL_TASK_PRIORITY_LOW};
}

static void* thread_pool_worker(void *arg) {
    tc_thread_pool_t *pool = (tc_thread_pool_t *)arg;
    
    p_mutex_lock(pool->lock);
    while (TRUE) {
        // wait for a task to be available 
        while (!thread_pool_has_tasks(pool)) {
            if (pool->shutdown && pool->active_threads == 0) {
                p_cond_variable_broadcast(pool->not_empty);
                p_mutex_unlock(pool->lock);
                return NULL;
            }
            p_cond_variable_wait(pool->not_empty, pool->lock);
        }

        // dequeue a task from the thread pool
        tc_thread_pool_context_t task = task_pool_dequeue(pool);
        pboolean is_blocking = (task.priority == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING);

        pool->active_threads++;
        if (is_blocking) {
            pool->blocking_active++;
        }
        p_mutex_unlock(pool->lock);

        // execute the task
        task.func(task.arg, task.priority);

        p_mutex_lock(pool->lock);
        pool->active_threads--;
        if (is_blocking) {
            pool->blocking_active--;
            // a blocking slot just freed; wake a worker that may have ignored
            // the blocking buffer because it was at capacity.
            p_cond_variable_signal(pool->not_empty);
        }
        if (pool->shutdown && pool->active_threads == 0 && !thread_pool_has_tasks(pool)) {
            p_cond_variable_broadcast(pool->not_empty);
        }
    }
}

// initialize the thread pool
pboolean tc_thread_pool_init(
    tc_thread_pool_t *pool,
    const pchar* round_robin_pattern,
    psize reserved_threads,
    psize max_blocking_threads
) {
    pint num_threads = p_uthread_ideal_count();
    if (reserved_threads >= num_threads) {
        log_error("Failed to initialize thread pool: reserved threads (%d) is greater than or equal to ideal threads (%d)", reserved_threads, num_threads);
        return FALSE;
    }
    num_threads -= reserved_threads;
    if (num_threads >= TC_THREADS_MAX_THREADS) {
        num_threads = TC_THREADS_MAX_THREADS;
    }

    log_info("Initializing thread pool with %d threads (system has %d ideal threads)", num_threads, p_uthread_ideal_count());
    
    pool->num_threads = num_threads;
    pool->round_robin_pattern = round_robin_pattern;
    pool->round_robin_index = 0;
    pool->lock = p_mutex_new();
    if (pool->lock == NULL) {
        return FALSE;
    }
    pool->not_empty = p_cond_variable_new();
    if (pool->not_empty == NULL) {
        p_mutex_free(pool->lock);
        return FALSE;
    }
    pool->shutdown = FALSE;
    pool->active_threads = 0;
    pool->blocking_active = 0;
    pool->max_blocking_threads = max_blocking_threads;

    init_task_buffer(&pool->task_prio_buffer[TC_THREAD_POOL_TASK_PRIORITY_HIGH], pool->high_prio_buffer, TC_THREADS_STD_BUFFER_SIZE / 2);
    init_task_buffer(&pool->task_prio_buffer[TC_THREAD_POOL_TASK_PRIORITY_MEDIUM], pool->medium_prio_buffer, TC_THREADS_STD_BUFFER_SIZE);
    init_task_buffer(&pool->task_prio_buffer[TC_THREAD_POOL_TASK_PRIORITY_BLOCKING], pool->blocking_prio_buffer, TC_THREADS_STD_BUFFER_SIZE);
    init_task_buffer(&pool->task_prio_buffer[TC_THREAD_POOL_TASK_PRIORITY_LOW], pool->low_prio_buffer, 2 * TC_THREADS_STD_BUFFER_SIZE);

    log_info("Creating %d worker threads...", num_threads);
    for (pint i = 0; i < num_threads; i++) {
        log_info("Creating worker thread %d...", i + 1);
        pool->thread_buffer[i] = p_uthread_create(
            thread_pool_worker, 
            pool, 
            TRUE, 
            NULL
        );
        if (pool->thread_buffer[i] == NULL) {
            p_mutex_free(pool->lock);
            p_cond_variable_free(pool->not_empty);
            for (pint j = 0; j < i; j++) {
                p_uthread_unref(pool->thread_buffer[j]);
            }
            return FALSE;
        }
    }

    return TRUE;
}

// finalize the thread pool
void tc_thread_pool_finalize(tc_thread_pool_t *pool) {
    // wait for all worker threads to finish
    log_info("Waiting for %d worker threads to finish...", pool->num_threads);
    for (pint i = 0; i < pool->num_threads; i++) {
        // wait for worker thread i to finish
        log_info("Waiting for worker thread %d to finish...", i + 1);
        p_uthread_join(pool->thread_buffer[i]); 
        p_uthread_unref(pool->thread_buffer[i]);
        log_info("Worker thread %d finished", i + 1);
    }

    p_mutex_free(pool->lock);
    p_cond_variable_free(pool->not_empty);
}

void tc_thread_pool_stop(tc_thread_pool_t *pool) {
    log_info("Stopping thread pool...");
    
    p_mutex_lock(pool->lock);

    pool->shutdown = TRUE;
    // signal all worker threads that the thread pool is shutting down
    p_cond_variable_broadcast(pool->not_empty); 

    p_mutex_unlock(pool->lock);
}

// Schedule a new task to the thread pool
pboolean tc_thread_schedule_new(
    tc_thread_pool_t *pool,
    tc_thread_pool_task_t new_task,
    void *arg,
    tc_thread_pool_task_priority_t priority
) {
    p_mutex_lock(pool->lock);

    if (pool->shutdown) {
        p_mutex_unlock(pool->lock);
        return FALSE;
    }

    tc_thread_pool_context_t task = {
        .func = new_task,
        .arg = arg,
        .priority = priority
    };

    pboolean enqueue_success = task_buffer_enqueue(
        pool, 
        &pool->task_prio_buffer[priority], 
        &task, 
        FALSE,
        priority == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING
    );
    if (!enqueue_success) {
        p_mutex_unlock(pool->lock);
        return FALSE;
    }

    p_cond_variable_signal(pool->not_empty);
    p_mutex_unlock(pool->lock);
    return TRUE;
}

// Schedule a next task to the thread pool
void tc_thread_schedule_next(
    tc_thread_pool_t *pool,
    tc_thread_pool_task_t next_task,
    tc_thread_pool_task_t shutdown_task,
    void *arg,
    tc_thread_pool_task_priority_t current_priority
) {
    p_mutex_lock(pool->lock);

    tc_thread_pool_context_t task = {
        .func = pool->shutdown ? shutdown_task : next_task, 
        .arg = arg, 
        .priority = current_priority
    };

    if (task.func == NULL) {
        p_mutex_unlock(pool->lock);
        return;
    }

    pboolean enqueue_success = task_buffer_enqueue(
        pool, 
        &pool->task_prio_buffer[current_priority], 
        &task, 
        TRUE,
        current_priority == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING
    );
    TC_ASSERT(enqueue_success, "Failed to enqueue next task in task chain");

    p_cond_variable_signal(pool->not_empty);
    p_mutex_unlock(pool->lock);
}