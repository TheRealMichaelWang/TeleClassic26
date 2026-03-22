#ifndef TELECLASSIC26_SERVER_H
#define TELECLASSIC26_SERVER_H

#include <plibsys.h>
#include "thread_pool.h"
#include "protocol.h"

#define TC_SERVER_MAX_SESSIONS 128

typedef struct tc_server tc_server_t;
typedef struct tc_session tc_session_t;

typedef struct tc_session {
    pchar username[TC_PROTOCOL_MAX_STR_LEN];
    PSocket *client_socket;
    tc_server_t *server;

    pint pending_packet_opcode;
    pchar *pending_packet_buffer;
    psize pending_packet_buffer_size;

    pint id;
} tc_session_t;

typedef struct tc_server {
    tc_session_t session_buffer[TC_SERVER_MAX_SESSIONS];
    pint id_buffer[TC_SERVER_MAX_SESSIONS];
    pint id_buffer_head;

    tc_thread_pool_t thread_pool;

    PSocketAddress* address;
    PSocket *listener_socket;

    PMutex *lock;

    pboolean started;
} tc_server_t;

// Initialize the server
// - return: TRUE if the server was initialized, FALSE otherwise
// - hostname: hostname to bind to
// - port: port to bind to
// - reserved_threads: number of threads to reserve for other purposes outside of the server
pboolean tc_server_init(
    tc_server_t *server, 
    const char* hostname, 
    pint port, 
    pint listener_backlog,
    pint reserved_threads
);

// Finalize the server
// - stops the server if it is started in addition to finalizing stuff
void tc_server_finalize(tc_server_t *server);

pboolean tc_server_start(tc_server_t *server);
void tc_server_stop(tc_server_t *server);


#endif /* TELECLASSIC26_SERVER_H */