#include <TeleClassic26/authentication/heartbeat.h>
#include <TeleClassic26/networking/protocol.h>
#include <stdlib.h>
#include <string.h>

// manager must be locked before calling this function
static void* heartbeat_worker(void* arg) {
    tc_heartbeat_manager_t* manager = (tc_heartbeat_manager_t*)arg;
    p_cond_variable_wait(manager->start_signal, manager->lock);

    while (!manager->shutdown) {
        for (pint i = 0; i < manager->num_services; i++) {
            heartbeat_generate_salt(manager->services[i].current_salt);
            if (!manager->services[i].web_play_url) {
                p_free(manager->services[i].web_play_url);
                manager->services[i].web_play_url = NULL;
            }
            tc_heartbeat_send_info(
                &manager->services[i],
                &manager->info,
                manager->services[i].current_salt
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
    tc_heartbeat_manager_t* manager, 
    tc_heartbeat_info_t info,
    tc_heartbeat_service_t* services,
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

    manager->start_signal = p_cond_variable_new();
    if (P_UNLIKELY(manager->start_signal == NULL)) {
        p_mutex_free(manager->lock);
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
        p_cond_variable_free(manager->start_signal);
        p_mutex_free(manager->lock);
        return FALSE;
    }

    // set all web play urls to NULL so we dont accidentally free dangling pointers
    for (pint i = 0; i < manager->num_services; i++) {
        manager->services[i].web_play_url = NULL;
    }
    return TRUE;
}

// Finalizes the heartbeat manager
void tc_heartbeat_manager_finalize(tc_heartbeat_manager_t* manager) {
    p_uthread_join(manager->heartbeat_thread);
    p_uthread_unref(manager->heartbeat_thread);
    p_mutex_free(manager->lock);

    for (pint i = 0; i < manager->num_services; i++) {
        if (manager->services[i].web_play_url) {
            p_free(manager->services[i].web_play_url);
        }
    }
}

// Generates a new salt for the service
void heartbeat_generate_salt(pchar salt[TC_HEARTBEAT_SALT_LENGTH]) {
    static const pchar base62_alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (psize i = 0; i < TC_HEARTBEAT_SALT_LENGTH; i++) {
        salt[i] = base62_alphabet[rand() % 62];
    }
}

// Starts the heartbeat manager
void tc_heartbeat_manager_start(tc_heartbeat_manager_t* manager) {
    p_cond_variable_signal(manager->start_signal);
}

// Stops the heartbeat manager
void tc_heartbeat_manager_stop(tc_heartbeat_manager_t* manager) {
    manager->shutdown = TRUE;
    p_cond_variable_signal(manager->start_signal);
}

// Validates the username with the given key
tc_heartbeat_service_t* tc_heartbeat_manager_validate(
    tc_heartbeat_manager_t* manager, 
    const pchar* username, 
    const pchar* key
) {
    if (P_UNLIKELY(manager == NULL || username == NULL || key == NULL)) {
        return NULL;
    }

    psize username_len = strnlen(username, TC_PROTOCOL_MAX_STR_LEN);

    p_mutex_lock(manager->lock);

    for (pint i = 0; i < manager->num_services; i++) {
        PCryptoHash* md5 = p_crypto_hash_new(P_CRYPTO_HASH_TYPE_MD5);
        if (P_UNLIKELY(md5 == NULL)) {
            continue;
        }

        p_crypto_hash_update(md5, (const puchar*)manager->services[i].current_salt, TC_HEARTBEAT_SALT_LENGTH);
        p_crypto_hash_update(md5, (const puchar*)username, username_len);

        pchar* hex = p_crypto_hash_get_string(md5);
        p_crypto_hash_free(md5);

        if (hex == NULL) {
            continue;
        }

        if (strncmp(key, hex, TC_PROTOCOL_MAX_STR_LEN) == 0) {
            p_free(hex);
            p_mutex_unlock(manager->lock);
            return &manager->services[i];
        }
        p_free(hex);
    }

    p_mutex_unlock(manager->lock);
    return NULL;
}