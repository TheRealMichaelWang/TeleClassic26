#ifndef TELECLASSIC26_MAP_CACHE_H
#define TELECLASSIC26_MAP_CACHE_H

#include <plibsys.h>
#include <TeleClassic26/gameplay/map.h>
#include <TeleClassic26/task_backlog.h>
#include <TeleClassic26/thread_pool.h>

typedef struct tc_map_cache_entry tc_map_cache_entry_t;

typedef struct tc_map_cache_entry {
    tc_map_t map;
    tc_task_backlog_t backlog;
    psize memory_usage;

    pint open_count;
    pboolean is_referenced;
    pboolean is_loaded;

    PMutex* lock;
    pchar* key;
    tc_map_cache_entry_t* next;
    tc_map_cache_entry_t* prev;
} tc_map_cache_entry_t;

typedef struct tc_map_cache {
    PTree* id_to_index;
    tc_map_cache_entry_t* head;
    tc_map_cache_entry_t* tail;
    tc_map_cache_entry_t* clock_hand;

    size_t num_entries;

    PRWLock* lock;

    psize memory_usage;
    psize memory_usage_threshold; //theshold before we start evicting maps
} tc_map_cache_t;

pboolean tc_map_cache_init(tc_map_cache_t* cache, psize memory_usage_threshold);
void tc_map_cache_finalize(tc_map_cache_t* cache);

// schedule a map open task
// - cache: the map cache
// - name: the name of the map to open
// - thread_pool: the thread pool to schedule the task on
// - schedule_info: the task backlog entry 
// - current_priority: the priority of the current task calling this function
// NOTE: this schedules a task that accepts the returned loaded map as an argument
void tc_map_cache_schedule_open(
    tc_map_cache_t* cache, 
    const pchar* name, 
    tc_thread_pool_t* thread_pool, 
    tc_task_backlog_entry_t schedule_info, 
    tc_thread_pool_task_priority_t current_priority
);

// reference a map
// - map: the map to reference
// NOTE: Only use this if map was loaded from the map cache
void tc_map_ref(tc_map_t* map);

// unreference a map
// - map: the map to unreference
// NOTE: Only use this if map was loaded from the map cache
void tc_map_unref(tc_map_t* map);

#endif