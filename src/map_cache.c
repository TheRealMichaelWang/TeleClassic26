#include "TeleClassic26/task_backlog.h"
#include <plibsys.h>
#include <TeleClassic26/gameplay/map_cache.h>
#include <TeleClassic26/log.h>
#include <TeleClassic26/utils.h>
#include <string.h>
#include <stdlib.h>

pboolean tc_map_cache_init(tc_map_cache_t* cache, psize memory_usage_threshold) {
    cache->memory_usage = 0;
    cache->head = NULL;
    cache->tail = NULL;
    cache->clock_hand = NULL;
    cache->num_entries = 0;
    cache->id_to_index = p_tree_new(P_TREE_TYPE_RB, (PCompareFunc)strcmp);
    if (!cache->id_to_index) { return FALSE; }
    cache->lock = p_rwlock_new();
    if (!cache->lock) { 
        p_tree_free(cache->id_to_index);
        return FALSE; 
    }
    cache->memory_usage_threshold = memory_usage_threshold;
    return TRUE;
}

void tc_map_cache_finalize(tc_map_cache_t* cache) {
    tc_map_cache_entry_t* entry = cache->head;
    while (entry) {
        tc_map_cache_entry_t* next = entry->next;
        tc_map_finalize(&entry->map);
        p_mutex_free(entry->lock);
        p_free(entry->key);
        p_free(entry);
        entry = next;
    }

    p_tree_free(cache->id_to_index);
    p_rwlock_free(cache->lock);
}


// please make sure to write lock the cache before calling this function!!!
// please make sure to aquire lock for the cache entry too and dont forget to unlock!!!
static void tc_map_cache_link_entry(tc_map_cache_t* cache, tc_map_cache_entry_t* entry) {
    if (cache->head == NULL) {
        TC_ASSERT(cache->tail == NULL, "Head and tail must be NULL at the same time");
        cache->head = entry;
        cache->tail = entry;
    } else {
        entry->next = cache->head;
        cache->head->prev = entry;
        cache->head = entry;
    }

    cache->memory_usage += entry->memory_usage;
    cache->num_entries++;
}

// please make sure to write lock the cache before calling this function!!!
// please make sure to aquire lock for the cache entry too but do not unlock (unlink will do it for you)!!!
static void tc_map_cache_unlink_entry(tc_map_cache_t* cache, tc_map_cache_entry_t* entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cache->head = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->tail = entry->prev;
    }

    cache->memory_usage -= entry->memory_usage;
    cache->num_entries--;

    p_mutex_unlock(entry->lock);
    p_mutex_free(entry->lock);
    p_free(entry->key);
    p_free(entry);
}


// please make sure to write lock the cache before calling this function!!!
static void tc_map_cache_evict(tc_map_cache_t* cache) {
    if (cache->head == NULL) { return; }
    if (cache->clock_hand == NULL) {
        cache->clock_hand = cache->head;
    } 

    // only one pass because we want to avoid thrashing
    pint max_iters = cache->num_entries;
    tc_map_cache_entry_t* current_entry = cache->tail;
    for(pint i = 0; i < max_iters; i++) {
        if (current_entry == NULL) { current_entry = cache->tail; }

        p_mutex_lock(current_entry->lock);
        if (!current_entry->is_loaded) { 
            current_entry = current_entry->prev; 
            p_mutex_unlock(current_entry->lock);
            continue; 
        }

        if (current_entry->open_count == 0) {
            if (current_entry->is_referenced) { //mark as not referenced
                current_entry->is_referenced = FALSE;
                current_entry = current_entry->prev;
            } else { //evict the map
                log_info("Evicting map %s from memory...", current_entry->key);
                p_tree_remove(cache->id_to_index, (ppointer)current_entry->key);
                tc_map_finalize(&current_entry->map);

                tc_map_cache_entry_t* prev = current_entry->prev;

                // unlink from the doubly linked list
                tc_map_cache_unlink_entry(cache, current_entry);

                // advance the clock hand past the evicted entry
                if (cache->clock_hand == current_entry) {
                    cache->clock_hand = prev;
                }

                if (cache->memory_usage < cache->memory_usage_threshold) {
                    if (cache->clock_hand == NULL) { cache->clock_hand = cache->tail; }
                    break;
                }
                current_entry = prev;
            }
        } else {
            p_mutex_unlock(current_entry->lock);
            current_entry = current_entry->prev;
        }
    }

    log_info("Map eviction complete (memory pressure: %zu/%zu bytes)", cache->memory_usage, cache->memory_usage_threshold);
}

typedef struct tc_map_cache_load_task_args {
    tc_map_cache_t* cache;
    const pchar* name;
    tc_thread_pool_t* thread_pool;
    tc_map_cache_entry_t* entry;
} tc_map_cache_load_task_args_t;

void tc_map_nolock_ref(tc_map_t* map) {
    tc_map_cache_entry_t* list_entry = (tc_map_cache_entry_t*)map;
    list_entry->open_count++;
    list_entry->is_referenced = TRUE;
}

void tc_map_nolock_unref(tc_map_t* map) {
    // do not do any eviction logic here, it should be handled by the cache itself
    tc_map_cache_entry_t* list_entry = (tc_map_cache_entry_t*)map;
    list_entry->open_count--;
}

static void tc_map_cache_open_from_entry(tc_map_cache_t* cache, tc_map_cache_entry_t* entry, tc_thread_pool_t* thread_pool, tc_task_backlog_entry_t schedule_info, tc_thread_pool_task_priority_t current_priority) {
    p_mutex_lock(entry->lock);
    if (!entry->is_loaded) {
        tc_task_backlog_push(&entry->backlog, schedule_info);
    }
    else {
        entry->open_count++;
        entry->is_referenced = TRUE;
    }
    p_mutex_unlock(entry->lock);

    tc_task_backlog_invoke_handler(
        &schedule_info, 
        thread_pool, 
        (tc_task_backlog_aquire_handler_t)tc_map_nolock_ref,
        (tc_task_backlog_release_handler_t)tc_map_nolock_ref,
        &entry->map,
        current_priority
    );
}

static void tc_map_cache_load_task(void* arg, tc_thread_pool_task_priority_t priority, pint session_generation) {
    TC_ASSERT(priority == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING, "Map loading must be run with blocking priority");

    tc_map_cache_load_task_args_t* load_task_args = (tc_map_cache_load_task_args_t*)arg;
    
    p_mutex_lock(load_task_args->entry->lock);

    if (!tc_map_load(&load_task_args->entry->map, load_task_args->name)) {
        log_error("Failed to load map %s", load_task_args->name);
        tc_task_schedule_backlog(
            &load_task_args->entry->backlog, 
            load_task_args->thread_pool, 
            NULL, NULL,
            NULL, 
            priority
        );
        p_mutex_unlock(load_task_args->entry->lock);
        p_free(load_task_args);
        return;
    }

    load_task_args->entry->is_loaded = TRUE;
    load_task_args->entry->memory_usage += tc_map_get_memory_usage(&load_task_args->entry->map);

    log_info("Succesfully loaded map %s", load_task_args->name);

    tc_task_schedule_backlog(
        &load_task_args->entry->backlog, 
        load_task_args->thread_pool, 
        (tc_task_backlog_aquire_handler_t)tc_map_nolock_ref,
        (tc_task_backlog_release_handler_t)tc_map_nolock_unref,
        &load_task_args->entry->map, 
        priority
    );
    p_mutex_unlock(load_task_args->entry->lock);

    // perform eviction logic
    p_rwlock_writer_lock(load_task_args->cache->lock);
    if (load_task_args->cache->memory_usage > load_task_args->cache->memory_usage_threshold) {
        tc_map_cache_evict(load_task_args->cache);
    }
    p_rwlock_writer_unlock(load_task_args->cache->lock);

    p_free(load_task_args);
}

void tc_map_cache_schedule_open(
    tc_map_cache_t* cache, 
    const pchar* name, 
    tc_thread_pool_t* thread_pool, 
    tc_task_backlog_entry_t schedule_info, 
    tc_thread_pool_task_priority_t current_priority
) {
    p_rwlock_reader_lock(cache->lock);
    tc_map_cache_entry_t* list_entry = (tc_map_cache_entry_t*)p_tree_lookup(cache->id_to_index, name);

    if (list_entry) {
        tc_map_cache_open_from_entry(cache, list_entry, thread_pool, schedule_info, current_priority);
        p_rwlock_reader_unlock(cache->lock);
    }
    else {
        p_rwlock_reader_unlock(cache->lock);

        p_rwlock_writer_lock(cache->lock);
        list_entry = (tc_map_cache_entry_t*)p_tree_lookup(cache->id_to_index, name);
        if (list_entry) {
            tc_map_cache_open_from_entry(cache, list_entry, thread_pool, schedule_info, current_priority);
            p_rwlock_writer_unlock(cache->lock);
            return;
        }

        log_info("Begin loading map from file: %s", name);

        tc_map_cache_entry_t* new_entry = (tc_map_cache_entry_t*)p_malloc(sizeof(tc_map_cache_entry_t));
        if (!new_entry) {
            log_error("Failed to allocate memory for new map entry for map %s", name);
            p_rwlock_writer_unlock(cache->lock);

            tc_task_backlog_invoke_handler(
                &schedule_info, 
                thread_pool, 
                NULL, NULL, 
                NULL, 
                TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
            );
            return;
        }
        new_entry->lock = p_mutex_new();
        if (!new_entry->lock) {
            log_error("Failed to allocate lock for new map entry for map %s", name);
            p_free(new_entry);
            p_rwlock_writer_unlock(cache->lock);

            tc_task_backlog_invoke_handler(
                &schedule_info, 
                thread_pool, 
                NULL, NULL, 
                NULL, 
                TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
            );
            return;
        }
        new_entry->key = p_strdup(name);
        if (!new_entry->key) {
            log_error("Failed to allocate memory for key of new map entry for map %s", name);
            p_mutex_free(new_entry->lock);
            p_free(new_entry);
            p_rwlock_writer_unlock(cache->lock);

            tc_task_backlog_invoke_handler(
                &schedule_info, 
                thread_pool, 
                NULL, NULL, 
                NULL, 
                TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
            );
            return;
        }

        tc_map_cache_load_task_args_t* load_task_args = (tc_map_cache_load_task_args_t*)p_malloc(sizeof(tc_map_cache_load_task_args_t));
        if (!load_task_args) {
            log_error("Failed to allocate memory for load task args for map %s", name);
            p_mutex_free(new_entry->lock);
            p_free(new_entry->key);
            p_free(new_entry);
            p_rwlock_writer_unlock(cache->lock);

            tc_task_backlog_invoke_handler(
                &schedule_info, 
                thread_pool, 
                NULL, NULL, 
                NULL, 
                TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
            );
            return;
        }
        load_task_args->cache = cache;
        load_task_args->name = name;
        load_task_args->thread_pool = thread_pool;
        load_task_args->entry = new_entry;

        tc_task_backlog_initialize(&new_entry->backlog);
        if (!tc_task_backlog_push(&new_entry->backlog, schedule_info)) {
            log_error("Failed to push schedule info to backlog for map %s", name);
            p_mutex_free(new_entry->lock);
            p_free(new_entry->key);
            tc_task_backlog_finalize(&new_entry->backlog);
            p_free(new_entry);
            p_rwlock_writer_unlock(cache->lock);
            p_free(load_task_args);

            tc_task_backlog_invoke_handler(
                &schedule_info, 
                thread_pool, 
                NULL, NULL, 
                NULL, 
                TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
            );
            return;
        }

        new_entry->open_count = 0;
        new_entry->is_referenced = TRUE;
        new_entry->is_loaded = FALSE;
        new_entry->memory_usage = sizeof(tc_map_cache_entry_t);
        new_entry->next = NULL;
        new_entry->prev = NULL;

        p_mutex_lock(new_entry->lock);

        tc_map_cache_link_entry(cache, new_entry);

        int schedule_success = tc_thread_schedule_new(
            thread_pool,
            (tc_thread_pool_task_t)(tc_map_cache_load_task),
            load_task_args,
            TC_THREAD_POOL_TASK_PRIORITY_BLOCKING,
            -1
        );
        if (!schedule_success) {
            log_error("Failed to schedule map load task for map %s", name);
            tc_map_cache_unlink_entry(cache, new_entry);
            p_mutex_unlock(new_entry->lock);

            p_free(load_task_args);
            p_mutex_free(new_entry->lock);
            p_free(new_entry->key);
            tc_task_backlog_finalize(&new_entry->backlog);
            p_free(new_entry);
            p_rwlock_writer_unlock(cache->lock);

            tc_task_backlog_invoke_handler(
                &schedule_info, 
                thread_pool, 
                NULL, NULL, 
                NULL, 
                TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
            );
            return;
        }

        p_mutex_unlock(new_entry->lock);
        p_rwlock_writer_unlock(cache->lock);
    }
}

void tc_map_ref(tc_map_t* map) {
    tc_map_cache_entry_t* list_entry = (tc_map_cache_entry_t*)map;
    p_mutex_lock(list_entry->lock);
    list_entry->open_count++;
    list_entry->is_referenced = TRUE;
    p_mutex_unlock(list_entry->lock);
}

void tc_map_unref(tc_map_t* map) {
    // do not do any eviction logic here, it should be handled by the cache itself
    tc_map_cache_entry_t* list_entry = (tc_map_cache_entry_t*)map;

    p_mutex_lock(list_entry->lock);
    list_entry->open_count--;
    p_mutex_unlock(list_entry->lock);
}