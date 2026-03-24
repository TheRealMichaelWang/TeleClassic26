#include <plibsys.h>
#include <TeleClassic26/version.h>
#include <TeleClassic26/networking/server.h>

pboolean run_server(void) {
    tc_server_t* server = p_malloc(sizeof(tc_server_t));
    if (server == NULL) {
        return FALSE;
    }
    int init_success = tc_server_init(server, "0.0.0.0", 8080, 128, 16,);
    if (!init_success) {
        tc_server_finalize(server);
        p_free(server);
        return FALSE;
    }
    if (!tc_server_start(server)) {
        tc_server_finalize(server);
        p_free(server);
        return FALSE;
    }

    tc_server_finalize(server);
    p_free(server);
    return TRUE;
}

int main(void)
{
    p_libsys_init();

    pboolean server_success = run_server();

    p_libsys_shutdown();

    return 0;
}