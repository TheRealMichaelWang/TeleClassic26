#include "TeleClassic26/gameplay/joinable.h"
#include "TeleClassic26/networking/api.h"
#include "TeleClassic26/thread_pool.h"
#include <plibsys.h>
#include <curl/curl.h>
#include <stdio.h>
#include <time.h>
#include <TeleClassic26/version.h>
#include <TeleClassic26/networking/server.h>
#include <TeleClassic26/log.h>

tc_heartbeat_service_t heartbeat_services[] = {
    {
        .hostname = "www.classicube.net",
        .port = 443,
        .use_https = TRUE,
    }
};

tc_heartbeat_info_t heartbeat_info = {
    .server_name = "TeleClassic26",
    .port = 8080,
    .max_players = 100,
    .is_public = TRUE,
    .protocol_version = TC_PROTOCOL_VERSION,
    .software_name = "TeleClassic26",
};

/*
* Rig for testing server
*/

static void* test_attempt_join(void* this_context, tc_session_t* session, const pchar* world_name, pint session_generation) {
    pboolean success = tc_api_schedule_send_map(
        session,
        "lobby.cw",
        NULL,
        TC_THREAD_POOL_TASK_PRIORITY_MEDIUM,
        session_generation
    );
    if (!success) {
        return NULL;
    }
    return this_context;
}

static void test_leave(void* this_context, tc_session_t* session, pint session_generation) {
    return;
}

static pboolean test_handle_set_block(void* this_context, tc_session_t* session, pint16 x, pint16 y, pint16 z, pchar mode, pint16 block, tc_thread_pool_task_priority_t current_priority, pint session_generation) {
    return TRUE;
}

static pboolean test_handle_position_update(void* this_context, tc_session_t* session, pint16 x, pint16 y, pint16 z, pchar heading, pchar pitch, tc_thread_pool_task_priority_t current_priority, pint session_generation) {
    return TRUE;
}

static pboolean test_handle_message(void* this_context, tc_session_t* session, const pchar* message, pint message_length, tc_thread_pool_task_priority_t current_priority, pint session_generation) {
    return TRUE;
}

static void test_handle_server_stop(void* this_context) {
    return;
}

static void test_handle_map_send_failure(void* this_context, tc_session_t* session, tc_thread_pool_task_priority_t current_priority, pint session_generation) {
    return;
}

static void test_handle_map_send_success(void* this_context, tc_session_t* session, tc_thread_pool_task_priority_t current_priority, pint session_generation) {
    return;
}

tc_joinable_interface_t* build_test_joinable(void) {
    tc_joinable_interface_t* joinable = p_malloc(sizeof(tc_joinable_interface_t));
    if (joinable == NULL) {
        return NULL;
    }
    joinable->name = "lobby";
    joinable->attempt_join = test_attempt_join;
    joinable->leave = test_leave;
    joinable->handle_set_block = test_handle_set_block;
    joinable->handle_position_update = test_handle_position_update;
    joinable->handle_message = test_handle_message;
    joinable->handle_server_stop = test_handle_server_stop;
    joinable->handle_map_send_failure = test_handle_map_send_failure;
    joinable->handle_map_send_success = test_handle_map_send_success;
    return joinable;
}

// end test rig code

pboolean run_server(void) {
    tc_server_t* server = p_malloc(sizeof(tc_server_t));
    if (server == NULL) {
        return FALSE;
    }

    PList* joinables = NULL;
    joinables = p_list_append(joinables, build_test_joinable());

    int init_success = tc_server_init(
        server, 
        "0.0.0.0", 
        8080, 128, 2, 2,
        heartbeat_services, 
        sizeof(heartbeat_services) / sizeof(tc_heartbeat_service_t), 
        heartbeat_info,
        1 << 30, //1 GB
        joinables,
        "lobby"
    );
    if (!init_success) {
        log_fatal("Failed to initialize server");
        p_free(server);
        return FALSE;
    }
    if (!tc_server_start(server)) {
        log_fatal("Failed to start server");
        tc_server_finalize(server);
        p_free(server);
        return FALSE;
    }

    p_uthread_sleep(15000);

    tc_server_finalize(server);
    p_free(server);
    return TRUE;
}

static ppointer p_calloc_wrapper(psize nmemb, psize size) {
    return p_malloc0(nmemb * size);
}

int main(void)
{
    // print the version and copyright information
    puts("TeleClassic 2026 - (C) Michael Wang, 2026");
    printf("Version: %d.%d.%d\n", TELECLASSIC26_VERSION_MAJOR, TELECLASSIC26_VERSION_MINOR, TELECLASSIC26_VERSION_PATCH);

    // create the logs directory if it doesn't exist
    if (p_dir_is_exists("logs") == FALSE) {
        int dir_create_success = p_dir_create("logs", 0755, NULL);
        if (dir_create_success == FALSE) {
            printf("Failed to create logs directory\n");
            return 1;
        }
    }
    // get log file handle
    time_t now = time(NULL);
    struct tm* now_tm = localtime(&now);
    if (P_UNLIKELY(now_tm == NULL)) {
        printf("localtime failed\n");
        return 1;
    }
    char log_file_name[32];
    snprintf(
        log_file_name, 
        sizeof(log_file_name), 
        "logs/%d-%d-%d.log", 
        now_tm->tm_year + 1900, now_tm->tm_mon + 1, now_tm->tm_mday
    );
    FILE* log_file = fopen(log_file_name, "a");
    if (log_file == NULL) {
        printf("Failed to create log file at %s\n", log_file_name);
        return 1;
    }
    
    // set the log file to the stdout
    log_add_fp(log_file, LOG_INFO);

    log_info("Initializing...");
    p_libsys_init();
    curl_global_init_mem(
        CURL_GLOBAL_ALL,
        p_malloc,
        p_free,
        p_realloc,
        p_strdup,
        p_calloc_wrapper    
    );

    pboolean server_success = run_server();

    p_libsys_shutdown();
    curl_global_cleanup();

    return 0;
}