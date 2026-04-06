#ifndef TELECLASSIC26_SERVER_H
#define TELECLASSIC26_SERVER_H

#include <plibsys.h>
#include <TeleClassic26/thread_pool.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/authentication/heartbeat.h>

#define TC_SERVER_MAX_SESSIONS 128
#define TC_SERVER_PING_INTERVAL 15000000 //15 seconds in microseconds

typedef struct tc_server tc_server_t;
typedef struct tc_session tc_session_t;

typedef struct tc_session {
    pchar username[TC_PROTOCOL_MAX_STR_LEN];
    PSocket *client_socket;
    tc_server_t *server;

    PTimeProfiler* ping_profiler;
    tc_heartbeat_service_t* authenticated_service;

    pint pending_packet_opcode;
    pchar *pending_packet_buffer;
    psize pending_packet_buffer_size;
    pboolean supports_cpe;

    pint id;
} tc_session_t;

typedef struct tc_server {
    tc_session_t session_buffer[TC_SERVER_MAX_SESSIONS];
    pint id_buffer[TC_SERVER_MAX_SESSIONS];
    pint id_buffer_head;

    tc_thread_pool_t thread_pool;
    tc_heartbeat_manager_t heartbeat_manager;

    pchar motd[TC_PROTOCOL_MAX_STR_LEN];

    PSocketAddress* address;
    PSocket *listener_socket;

    PMutex *lock;
    volatile pint active_players;

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
    pint reserved_threads,
    tc_heartbeat_service_t* heartbeat_services,
    pint num_heartbeat_services,
    tc_heartbeat_info_t heartbeat_info
);

// Finalize the server
// - stops the server if it is started in addition to finalizing stuff
void tc_server_finalize(tc_server_t *server);

// Start the server
// - return: TRUE if the server was started, FALSE otherwise
pboolean tc_server_start(tc_server_t *server);

// Stop the server
// - stops the server if it is started
// - CAN BE INVOKED FROM ANY THREAD
void tc_server_stop(tc_server_t *server);

// kicks a session from the server and disconnects/disposes of the session
// - session: the session to kick
// - msg: the message to send to the session (can be NULL for no kick message)
void tc_server_kick_session(tc_session_t* session, const char* msg);

// cleans up pending packet buffer and schedules next task in client task chain
// - session: the session to cleanup
// - next_task: the next task to schedule; usually should be tc_server_client_listen_worker
// NOTE: call this function at the end of each protocol packet handler
void tc_server_protocol_handler_cleanup(tc_session_t* session, tc_thread_pool_task_t next_task);

// Task that kicks a client and disconnects gracefully
// - arg: pointer to the session to kick
// NOTE: use as part of client task chain as the shutdown task
void tc_server_shutdown_client_task(void* arg, tc_thread_pool_task_priority_t priority);

// Task that listening for new clients; part of client task chain
// - arg: pointer to the session to listen for new clients
// NOTE: use this function to schedule the next task in a task chain
void tc_server_client_listen_task(void *arg, tc_thread_pool_task_priority_t priority);

#endif /* TELECLASSIC26_SERVER_H */