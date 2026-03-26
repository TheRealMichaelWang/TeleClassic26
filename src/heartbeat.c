#include "plibsys.h"
#include <curl/urlapi.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <TeleClassic26/authentication/heartbeat.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/utils.h>

// manager must be locked before calling this function
static void* heartbeat_worker(void* arg) {
    tc_heartbeat_manager_t* manager = (tc_heartbeat_manager_t*)arg;
    p_cond_variable_wait(manager->start_signal, manager->lock);

    while (!manager->shutdown) {
        // lock the manager to prevent race conditions
        p_mutex_lock(manager->lock);
        for (pint i = 0; i < manager->num_services; i++) {
            heartbeat_generate_salt(manager->services[i].current_salt);
            if (manager->services[i].web_play_url) {
                p_free(manager->services[i].web_play_url);
                manager->services[i].web_play_url = NULL;
            }
            tc_heartbeat_send_info(
                &manager->services[i],
                p_atomic_int_get(manager->active_players),
                &manager->info,
                manager->services[i].current_salt
            );
        }
        p_tree_clear(manager->auth_tree);
        p_mutex_unlock(manager->lock);

        p_uthread_sleep(45000000); // 45 seconds
    }
    return NULL;
}

// Initializes the heartbeat manager
pboolean heartbeat_manager_init(
    tc_heartbeat_manager_t* manager, 
    tc_heartbeat_info_t info,
    tc_heartbeat_service_t* services,
    volatile pint* active_players,
    pint num_services
) {
    manager->info = info;
    manager->services = services;
    manager->num_services = num_services;
    manager->active_players = active_players;
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

    manager->auth_tree = p_tree_new_full(
        P_TREE_TYPE_AVL, 
        tc_string_compare,
        NULL,
        p_free,
        NULL
    );
    if (P_UNLIKELY(manager->auth_tree == NULL)) {
        p_cond_variable_free(manager->start_signal);
        p_mutex_free(manager->lock);
        return FALSE;
    }

    manager->heartbeat_thread = p_uthread_create(
        heartbeat_worker, 
        manager, 
        TRUE, 
        NULL
    );
    if (P_UNLIKELY(manager->heartbeat_thread == NULL)) {
        p_cond_variable_free(manager->start_signal);
        p_mutex_free(manager->lock);
        p_tree_free(manager->auth_tree);
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
    p_cond_variable_free(manager->start_signal);
    p_tree_free(manager->auth_tree);
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

    // prevent "double tap" authentication attacks
    if (p_tree_lookup(manager->auth_tree, (ppointer)key) != NULL) {
        p_mutex_unlock(manager->lock);
        return NULL;
    }

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

        psize hex_len = strlen(hex);
        if (strncmp(key, hex, hex_len) == 0) {
            p_tree_insert(manager->auth_tree, hex, (ppointer)&manager->services[i]);
            p_mutex_unlock(manager->lock);
            return &manager->services[i];
        }
        p_free(hex);
    }

    p_mutex_unlock(manager->lock);
    return NULL;
}

typedef struct heartbeat_response_wb_data {
    pchar response[TC_URL_BUFFER_SIZE];
    psize size;
} heartbeat_response_wb_data_t;

static size_t heartbeat_response_wb(void* data, size_t size, size_t nmemb, void* userdata) {
    heartbeat_response_wb_data_t* hb_data = (heartbeat_response_wb_data_t*)userdata;
    size_t remaining_capacity = TC_URL_BUFFER_SIZE - hb_data->size;

    size_t realsize = size * nmemb;
    if (realsize > remaining_capacity) {
        return 0;
    }

    memcpy(&hb_data->response[hb_data->size], data, realsize);
    hb_data->size += realsize;

    return realsize;
}

pboolean tc_heartbeat_send_info(
    tc_heartbeat_service_t* service,
    const pint active_players,
    const tc_heartbeat_info_t* info,
    const pchar salt[TC_HEARTBEAT_SALT_LENGTH]
) {
    CURL* curl = curl_easy_init();
    if (P_UNLIKELY(curl == NULL)) {
        return FALSE;
    }

    CURLU* url = curl_url();
    if (P_UNLIKELY(url == NULL)) {
        curl_easy_cleanup(curl);
        return FALSE;
    }

    const char* scheme = service->use_https ? "https" : "http";
    curl_url_set(url, CURLUPART_SCHEME, scheme, 0);
    curl_url_set(url, CURLUPART_HOST, service->hostname, 0);
    curl_url_set(url, CURLUPART_PATH, "/heartbeat", 0);

    char http_port_str[6];
    snprintf(http_port_str, sizeof(http_port_str), "%d", service->port);
    curl_url_set(url, CURLUPART_PORT, http_port_str, 0);

    char port_str[11];
    snprintf(port_str, sizeof(port_str), "port=%d", info->port);
    curl_url_set(url, CURLUPART_QUERY, port_str, CURLU_APPENDQUERY);

    char max_players_str[11];
    snprintf(max_players_str, sizeof(max_players_str), "max=%d", info->max_players);
    curl_url_set(url, CURLUPART_QUERY, max_players_str, CURLU_APPENDQUERY);

    char name_str[TC_PROTOCOL_MAX_STR_LEN + 6] = "name=";
    strncat(name_str, info->server_name, TC_PROTOCOL_MAX_STR_LEN);
    curl_url_set(url, CURLUPART_QUERY, name_str, CURLU_APPENDQUERY | CURLU_URLENCODE);

    char is_public_str[14];
    snprintf(is_public_str, sizeof(is_public_str), "public=%s", (info->is_public ? "True" : "False"));
    curl_url_set(url, CURLUPART_QUERY, is_public_str, CURLU_APPENDQUERY);

    char protocol_version_str[13];
    snprintf(protocol_version_str, sizeof(protocol_version_str), "version=%d", info->protocol_version);
    curl_url_set(url, CURLUPART_QUERY, protocol_version_str, CURLU_APPENDQUERY);

    char salt_str[TC_HEARTBEAT_SALT_LENGTH + 6] = "salt=";
    strncat(salt_str, salt, TC_HEARTBEAT_SALT_LENGTH);
    curl_url_set(url, CURLUPART_QUERY, salt_str, CURLU_APPENDQUERY);

    char active_players_str[18];
    snprintf(active_players_str, sizeof(active_players_str), "users=%d", active_players);
    curl_url_set(url, CURLUPART_QUERY, active_players_str, CURLU_APPENDQUERY);

    if (info->software_name) {
        char software_name_str[TC_PROTOCOL_MAX_STR_LEN + 10] = "software=";
        strncat(software_name_str, info->software_name, TC_PROTOCOL_MAX_STR_LEN);
        curl_url_set(url, CURLUPART_QUERY, software_name_str, CURLU_APPENDQUERY | CURLU_URLENCODE);
    }

    if (info->allow_web_play) {
        curl_url_set(url, CURLUPART_QUERY, "web=True", CURLU_APPENDQUERY);
    }

    char *full_url_str = NULL;
    curl_url_get(url, CURLUPART_URL, &full_url_str, 0);
    if (P_UNLIKELY(full_url_str == NULL)) {
        curl_url_cleanup(url);
        curl_easy_cleanup(curl);
        return FALSE;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, full_url_str);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, scheme);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, scheme);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    heartbeat_response_wb_data_t hb_data;
    hb_data.size = 0;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, heartbeat_response_wb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &hb_data);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_url_cleanup(url);
    curl_free(full_url_str);

    if (res != CURLE_OK) {
        return FALSE;
    }

    service->web_play_url = p_malloc(hb_data.size + 1);
    if (P_UNLIKELY(service->web_play_url == NULL)) {
        return FALSE;
    }

    memcpy(service->web_play_url, hb_data.response, hb_data.size);
    service->web_play_url[hb_data.size] = '\0';

    return TRUE;
}