#include <plibsys.h>
#include <TeleClassic26/task_backlog.h>
#include <TeleClassic26/utils.h>

void tc_task_backlog_initialize(tc_task_backlog_t* backlog) {
    backlog->head_block = NULL;
    backlog->tail_block = NULL;
}

void tc_task_backlog_finalize(tc_task_backlog_t* backlog) {
    for (tc_task_backlog_block_t* block = backlog->head_block; block != NULL;) {
        tc_task_backlog_block_t* next_block = block->next_block;
        p_free(block);
        block = next_block;
    }
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
    if (backlog->head_block == NULL) {
        TC_ASSERT(backlog->tail_block == NULL, "Head and tail block cannot be NULL at the same time");

        tc_task_backlog_block_t* new_block = tc_task_backlog_block_new();
        if (new_block == NULL) {
            return FALSE;
        }
        backlog->head_block = new_block;
        backlog->tail_block = new_block;
    }

    if (backlog->tail_block->count == TC_TASK_BACKLOG_BUFFER_SIZE) {
        tc_task_backlog_block_t* new_block = tc_task_backlog_block_new();
        if (new_block == NULL) {    
            return FALSE;
        }
        backlog->tail_block->next_block = new_block;
        backlog->tail_block = new_block;
    }

    backlog->tail_block->entries[backlog->tail_block->count] = entry;
    backlog->tail_block->count++;

    return TRUE;
}

void tc_task_schedule_backlog(
    tc_task_backlog_t* backlog, 
    tc_thread_pool_t* pool, 
    tc_task_backlog_aquire_handler_t aquire_handler,
    tc_task_backlog_release_handler_t release_handler,
    void* result, 
    tc_thread_pool_task_priority_t current_priority
) {
    pboolean task_chain_continued = FALSE;
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
            if (aquire_handler) {
                aquire_handler(result);
            }
            args->result = result;
            args->context = entry->context;

            // use current task chain instead of terminating it
            if (entry->priority == current_priority && !task_chain_continued) {
                int continue_success =tc_thread_schedule_next2(
                    pool,
                    (tc_thread_pool_task_t)(entry->success_handler),
                    args,
                    current_priority,
                    entry->session_generation
                );
                if (!continue_success) {
                    entry->failure_handler(entry->context, entry->session_generation);
                    if (release_handler) {
                        release_handler(result);
                    }
                    p_free(args);
                    continue;
                }

                task_chain_continued = TRUE;
                continue;
            }

            // success then schedule success tasks
            if (!tc_thread_schedule_new(
                pool, 
                (tc_thread_pool_task_t)(entry->success_handler), 
                args, 
                entry->priority, 
                entry->session_generation
            )) {
                entry->failure_handler(entry->context, entry->session_generation);
                if (release_handler) {
                    release_handler(result);
                }
                p_free(args);
                continue;
            }
        }
    }

    return;
}

void tc_task_backlog_invoke_handler(
    tc_task_backlog_entry_t* entry, 
    tc_thread_pool_t* pool, 
    tc_task_backlog_aquire_handler_t aquire_handler,
    tc_task_backlog_release_handler_t release_handler,
    void* result, 
    tc_thread_pool_task_priority_t current_priority
) {
    if (result == NULL) {
        entry->failure_handler(entry->context, entry->session_generation);
        return;
    }

    tc_task_backlog_args_t* args = p_malloc(sizeof(tc_task_backlog_args_t));
    if (args == NULL) {
        entry->failure_handler(entry->context, entry->session_generation);
        return;
    }

    args->result = result;
    args->context = entry->context;

    if (aquire_handler) {
        aquire_handler(result);
    }
    if (current_priority != TC_THREAD_POOL_TASK_PRIORITY_INVALID && current_priority == entry->priority) {
        if (!tc_thread_schedule_next2(pool, (tc_thread_pool_task_t)(entry->success_handler), args, current_priority, entry->session_generation)) {
            entry->failure_handler(entry->context, entry->session_generation);
            if (release_handler) {
                release_handler(result);
            }
            p_free(args);
            return;
        }
    }
    else {
        if (!tc_thread_schedule_new(pool, (tc_thread_pool_task_t)(entry->success_handler), args, entry->priority, entry->session_generation)) {
            entry->failure_handler(entry->context, entry->session_generation);
            if (release_handler) {
                release_handler(result);
            }
            p_free(args);
            return;
        }
    }
    // do not free args here, it MUST be freed by the success handler
}