#include <plibsys.h>
#include <curl/curl.h>
#include <TeleClassic26/version.h>
#include <TeleClassic26/networking/server.h>
#include <stdio.h>

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
    printf("Server initialized\n");
    if (!init_success) {
        printf("Failed to initialize server\n");
        p_free(server);
        return FALSE;
    }
    if (!tc_server_start(server)) {
        printf("Failed to start server\n");
        tc_server_finalize(server);
        p_free(server);
        return FALSE;
    }

    printf("Server started\n");
    p_uthread_sleep(5000); // 5 seconds

    tc_server_finalize(server);
    p_free(server);

    printf("Server finalized\n");
    return TRUE;
}

int main(void)
{
    p_libsys_init();
    curl_global_init(CURL_GLOBAL_ALL);

    pboolean server_success = run_server();

    p_libsys_shutdown();
    curl_global_cleanup();

    return 0;
}