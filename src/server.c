#include "TeleClassic26/protocol.h"
#include "TeleClassic26/thread_pool.h"
#include "plibsys.h"
#include <TeleClassic26/server.h>

pboolean tc_server_init(
    tc_server_t *server, 
    const char* hostname, 
    pint port, 
    pint listener_backlog,
    pint reserved_threads
) {
    server->address = p_socket_address_new(hostname, port);
    if (server->address == NULL) {
        return FALSE;
    }

    server->listener_socket = p_socket_new(P_SOCKET_FAMILY_INET, P_SOCKET_TYPE_STREAM, P_SOCKET_PROTOCOL_TCP, NULL);
    if (server->listener_socket == NULL) {
        p_socket_address_free(server->address);
        return FALSE;
    }

    p_socket_set_listen_backlog(server->listener_socket, listener_backlog);
    p_socket_set_blocking(server->listener_socket, FALSE);
    p_socket_set_timeout(server->listener_socket, 50);

    if (!p_socket_bind(server->listener_socket, server->address, FALSE, NULL)) {
        p_socket_free(server->listener_socket);
        p_socket_address_free(server->address);
        return FALSE;
    }

    if (!tc_thread_pool_init(&server->thread_pool, reserved_threads)) {
        p_socket_free(server->listener_socket);
        p_socket_address_free(server->address);
        return FALSE;
    }

    server->lock = p_mutex_new();

    for (pint i = 0; i < TC_SERVER_MAX_SESSIONS; i++) {
        server->id_buffer[i] = i;
    }
    server->id_buffer_head = 0;

    return TRUE;
}

void tc_server_finalize(tc_server_t *server) {
    if (server->started) {
        tc_server_stop(server);
    }

    tc_thread_pool_finalize(&server->thread_pool);

    if (server->started) {
        p_socket_close(server->listener_socket, NULL);
    }
    p_socket_address_free(server->address);
    p_socket_free(server->listener_socket);
    p_mutex_free(server->lock);
}

// kicks a session from the server
// - server: the server to kick the session from
// - session_id: the id of the session to kick
// - msg: the message to send to the session (can be NULL for no kick message)
static void kick_session(tc_session_t* session, const char* msg) {
    p_mutex_lock(session->server->lock);

    if (msg) {
        tc_protocol_kick(session->client_socket, msg);
    }

    p_socket_shutdown(session->client_socket, TRUE, TRUE, NULL);
    p_socket_free(session->client_socket);
    
    session->server->id_buffer_head--;
    session->server->id_buffer[session->server->id_buffer_head] = session->id;

    p_mutex_unlock(session->server->lock);
}

// kicks a session from the server because the server is busy
// Just a wrapper for kick_session with a default message
// - session: the session to kick
static void shutdown_server_kick(void* arg) {
    tc_session_t *session = (tc_session_t *)arg;
    kick_session(session, "Server is Shutting Down: Please come back later!");
}

// worker thread for listening for new clients
// - checks whether a 
static void client_listen_worker(void *arg) {
    tc_session_t *session = (tc_session_t *)arg;
    // To be implemented later
}

static void client_negotiatiator(void* arg) {
    tc_session_t *session = (tc_session_t *)arg;
    // To be implemented later
}

// handles a new session
// - server: the server to handle the new session for
// - client_socket: the socket of the new client
static void handle_new_session(tc_server_t* server, PSocket* client_socket) {
    p_mutex_lock(server->lock);

    if (server->id_buffer_head == TC_SERVER_MAX_SESSIONS) {
        tc_protocol_kick(client_socket, "Server is Busy: Please try again or come back soon.");

        p_socket_shutdown(client_socket, TRUE, TRUE, NULL);
        p_socket_free(client_socket);
        p_mutex_unlock(server->lock);
        return;
    }

    pint session_id = server->id_buffer[server->id_buffer_head];
    server->id_buffer_head++;

    p_mutex_unlock(server->lock);

    tc_session_t* session = &server->session_buffer[session_id];
    session->client_socket = client_socket;
    session->id = session_id;
    session->server = server;

    pboolean task_chain_create_success = tc_thread_pool_add_task(
        &server->thread_pool,
        client_negotiatiator,
        session,
        shutdown_server_kick,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH,
        FALSE
    );

    if (!task_chain_create_success) {
        kick_session(session, "Server is Busy: Please try again or come back soon.");
    }
}

// Main task chain for listining/accepting new clients
static void listener_worker_thread(void *arg) {
    tc_server_t *server = (tc_server_t *)arg;

    PSocket* client_socket = p_socket_accept(server->listener_socket, NULL);
    if (client_socket) {
        handle_new_session(server, client_socket);
    }

    // enque the listener worker again yeilding to other tasks
    tc_thread_pool_add_task(
        &server->thread_pool,
        listener_worker_thread,
        server,
        NULL,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH,
        TRUE
    );
}

pboolean tc_server_start(tc_server_t *server) {
    if (server->started) {
        return FALSE;
    }

    pboolean listen_success = p_socket_listen(server->listener_socket, NULL);
    if (!listen_success) {
        return FALSE;
    }

    pboolean task_chain_create_success = tc_thread_pool_add_task(
        &server->thread_pool,
        listener_worker_thread,
        server,
        NULL,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH,
        FALSE
    );
    if (!task_chain_create_success) {
        p_socket_close(server->listener_socket, NULL);
        return FALSE;
    }

    server->started = TRUE;
    return TRUE;
}

void tc_server_stop(tc_server_t *server) {
    if (!server->started) {
        return;
    }

    tc_thread_pool_stop(&server->thread_pool);
}