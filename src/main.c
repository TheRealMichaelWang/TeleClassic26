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

pboolean run_server(void) {
    tc_server_t* server = p_malloc(sizeof(tc_server_t));
    if (server == NULL) {
        return FALSE;
    }

    int init_success = tc_server_init(
        server, 
        "0.0.0.0", 
        8080, 128, 2, 
        heartbeat_services, 
        sizeof(heartbeat_services) / sizeof(tc_heartbeat_service_t), 
        heartbeat_info
    );

    log_info("Server initialized");
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

    log_info("Server started");

    p_uthread_sleep(5000);

    tc_server_finalize(server);
    p_free(server);

    log_info("Server finalized");
    return TRUE;
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
    curl_global_init(CURL_GLOBAL_ALL);

    pboolean server_success = run_server();

    p_libsys_shutdown();
    curl_global_cleanup();

    return 0;
}