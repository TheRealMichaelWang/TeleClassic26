#include <plibsys.h>
#include <TeleClassic26/task_backlog.h>
#include <TeleClassic26/utils.h>

pboolean tc_task_backlog_initialize(tc_task_backlog_t* backlog) {
    backlog->head_block = NULL;
    backlog->tail_block = NULL;
    backlog->lock = p_mutex_new();
    if (backlog->lock == NULL) {
        return FALSE;
    }
    return TRUE;
}

void tc_task_backlog_finalize(tc_task_backlog_t* backlog) {
    for (tc_task_backlog_block_t* block = backlog->head_block; block != NULL;) {
        tc_task_backlog_block_t* next_block = block->next_block;
        p_free(block);
        block = next_block;
    }
    p_mutex_free(backlog->lock);
}

static tc_task_backlog_block_t* tc_task_backlog_block_new(void) {
    tc_task_backlog_block_t* block = p_malloc(sizeof(tc_task_backlog_block_t));
    if (block == NULL) {
        return NULL;
    }
    block->count = 0;
    block->next_block = NULL;
    return block;
}

pboolean tc_task_backlog_push(tc_task_backlog_t* backlog, tc_task_backlog_entry_t entry) {
    p_mutex_lock(backlog->lock);

    if (backlog->head_block == NULL) {
        TC_ASSERT(backlog->tail_block == NULL, "Head and tail block cannot be NULL at the same time");

        tc_task_backlog_block_t* new_block = tc_task_backlog_block_new();
        if (new_block == NULL) {
            p_mutex_unlock(backlog->lock);
            return FALSE;
        }
        backlog->head_block = new_block;
        backlog->tail_block = new_block;
    }

    if (backlog->tail_block->count == TC_TASK_BACKLOG_BUFFER_SIZE) {
        tc_task_backlog_block_t* new_block = tc_task_backlog_block_new();
        if (new_block == NULL) {
            p_mutex_unlock(backlog->lock);
            return FALSE;
        }
        backlog->tail_block->next_block = new_block;
        backlog->tail_block = new_block;
    }

    backlog->tail_block->entries[backlog->tail_block->count] = entry;
    backlog->tail_block->count++;

    p_mutex_unlock(backlog->lock);
    return TRUE;
}

void tc_task_schedule_backlog(tc_task_backlog_t* backlog, tc_thread_pool_t* pool, void* result) {
    p_mutex_lock(backlog->lock);
    
    for (tc_task_backlog_block_t* block = backlog->head_block; block != NULL; block = block->next_block) {
        for (psize i = 0; i < block->count; i++) {
            tc_task_backlog_entry_t* entry = &block->entries[i];

            // if failure then invoke failure handler
            if (result == NULL) {
                entry->failure_handler(entry->context, entry->session_generation);
                continue;
            }

            tc_task_backlog_args_t* args = p_malloc(sizeof(tc_task_backlog_args_t));
            if (args == NULL) {
                entry->failure_handler(entry->context, entry->session_generation);
                continue;
            }
            args->result = result;
            args->context = entry->context;

            // success then schedule success tasks
            if (!tc_thread_schedule_new(
                pool, 
                (tc_thread_pool_task_t)(entry->success_handler), 
                args, 
                entry->priority, 
                entry->session_generation
            )) {
                entry->failure_handler(entry->context, entry->session_generation);
                p_free(args);
                continue;
            }
        }
    }

    p_mutex_unlock(backlog->lock);
    return;
}