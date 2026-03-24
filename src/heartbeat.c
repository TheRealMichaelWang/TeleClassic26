#include <TeleClassic26/authentication/heartbeat.h>

// manager must be locked before calling this function
static void* heartbeat_worker(void* arg) {
    heartbeat_manager_t* manager = (heartbeat_manager_t*)arg;

    while (!manager->shutdown) {
        heartbeat_generate_salt(manager->current_salt);

        for (pint i = 0; i < manager->num_services; i++) {
            if (!manager->services[i].web_play_url) {
                p_free(manager->services[i].web_play_url);
                manager->services[i].web_play_url = NULL;
            }
            tc_heartbeat_send_info(
                &manager->services[i],
                &manager->info,
                manager->current_salt
            );
        }
        p_mutex_unlock(manager->lock);

        p_uthread_sleep(45000000); // 45 seconds
        
        p_mutex_lock(manager->lock);
    }
    p_mutex_unlock(manager->lock);
    return NULL;
}

// Initializes the heartbeat manager
pboolean heartbeat_manager_init(
    heartbeat_manager_t* manager, 
    tc_heartbeat_info_t info,
    heartbeat_service_t* services,
    pint num_services
) {
    manager->info = info;
    manager->services = services;
    manager->num_services = num_services;
    manager->shutdown = FALSE;
    manager->lock = p_mutex_new();

    if (P_UNLIKELY(manager->lock == NULL)) {
        return FALSE;
    }

    p_mutex_lock(manager->lock);
    manager->heartbeat_thread = p_uthread_create(
        heartbeat_worker, 
        manager, 
        TRUE, 
        NULL
    );
    if (P_UNLIKELY(manager->heartbeat_thread == NULL)) {
        p_mutex_unlock(manager->lock);
        return FALSE;
    }

    // set all web play urls to NULL so we dont accidentally free dangling pointers
    for (pint i = 0; i < manager->num_services; i++) {
        manager->services[i].web_play_url = NULL;
    }
    return TRUE;
}

// Finalizes the heartbeat manager
void heartbeat_manager_finalize(heartbeat_manager_t* manager) {
    manager->shutdown = TRUE;
    p_uthread_join(manager->heartbeat_thread);
    p_uthread_unref(manager->heartbeat_thread);
    p_mutex_free(manager->lock);

    for (pint i = 0; i < manager->num_services; i++) {
        if (manager->services[i].web_play_url) {
            p_free(manager->services[i].web_play_url);
        }
    }
}

// Validates the username with the given key
heartbeat_service_t* heartbeat_manager_validate(
    heartbeat_manager_t* manager, 
    const pchar* username, 
    const pchar* key
) {
    for (pint i = 0; i < manager->num_services; i++) {
        
    }
    return NULL;
}