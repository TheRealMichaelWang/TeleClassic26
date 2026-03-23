#include <TeleClassic26/networking/protocol.h>
#include "TeleClassic26/thread_pool.h"
#include <plibsys.h>
#include <TeleClassic26/networking/server.h>

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

static void disconnect_session(tc_session_t* session) {
    // free the pending packet buffer
    if (session->pending_packet_buffer) {
        p_free(session->pending_packet_buffer);
        session->pending_packet_buffer = NULL;
    }

    // shutdown and free the client socket
    p_socket_shutdown(session->client_socket, TRUE, TRUE, NULL);
    p_socket_free(session->client_socket);
    
    // add the session id back to the id buffer
    p_mutex_lock(session->server->lock);
    
    session->server->id_buffer_head--;
    session->server->id_buffer[session->server->id_buffer_head] = session->id;

    p_mutex_unlock(session->server->lock);
}

// kicks a session from the server
// - server: the server to kick the session from
// - session_id: the id of the session to kick
// - msg: the message to send to the session (can be NULL for no kick message)
static void kick_session(tc_session_t* session, const char* msg) {
    if (msg) {
        tc_protocol_kick(session->client_socket, msg);
    }
    disconnect_session(session);
}

// cleans up pending packet buffer and schedules next task in client task chain
void tc_server_protocol_handler_cleanup(tc_session_t* session, tc_thread_pool_task_t next_task) {
    p_free(session->pending_packet_buffer);
    session->pending_packet_opcode = -1;
    session->pending_packet_buffer = NULL;
    session->pending_packet_buffer_size = 0;

    if (next_task) {
        tc_thread_schedule_next(
            &session->server->thread_pool,
            next_task,
            tc_server_shutdown_client_task,
            session,
            TC_THREAD_POOL_TASK_PRIORITY_HIGH
        );
    }
}

// kicks a session from the server because the server is busy
// Just a wrapper for kick_session with a default message
// - session: the session to kick
static void tc_server_shutdown_client_task(void* arg) {
    tc_session_t *session = (tc_session_t *)arg;
    kick_session(session, "Server is Shutting Down: Please come back later!");
}

// worker thread for listening for new clients
void tc_server_client_listen_task(void *arg) {
    tc_session_t *session = (tc_session_t *)arg;

    if (!tc_protocol_ping(session->client_socket)) {
        disconnect_session(session);
        return;
    }

    PError *error = NULL;
    if (session->pending_packet_opcode != -1) {
        pchar opcode_buffer[1];
        psize read_size = p_socket_receive(
            session->client_socket, 
            opcode_buffer, 
            1, 
            &error
        );

        if (read_size > 0) {
            session->pending_packet_opcode = (pint)opcode_buffer[0];

            // validate the packet opcode
            if (session->pending_packet_opcode < 0 || session->pending_packet_opcode >= TC_PROTOCOL_TOTAL_PACKETS) {
                p_error_free(error);
                kick_session(session, "Invalid Packet Opcode: Please reconnect.");
                return;
            }

            // allocate the buffer for the packet data
            psize packet_buffer_size = tc_protocol_packet_sizes[session->pending_packet_opcode];
            session->pending_packet_buffer = p_malloc(packet_buffer_size);
            session->pending_packet_buffer_size = 0;
            if (session->pending_packet_buffer == NULL) {
                p_error_free(error);
                kick_session(session, "Out of Memory: Sorry");
                return;
            }
        }
        else if (read_size == 0) { // client disconnected
            kick_session(session, "Client Disconnected: Please reconnect.");
            p_error_free(error);
            return;
        }
    } else {
        psize packet_buffer_size = tc_protocol_packet_sizes[session->pending_packet_opcode];

        PError *error = NULL;
        psize read_size = p_socket_receive(
            session->client_socket, 
            &session->pending_packet_buffer[session->pending_packet_buffer_size], 
            packet_buffer_size - session->pending_packet_buffer_size, 
            &error
        );
        
        if (read_size > 0) {
            session->pending_packet_buffer_size += read_size;

            // buffer is full, process the packet
            if (session->pending_packet_buffer_size == packet_buffer_size) {
                tc_thread_pool_task_t handler = tc_protocol_packet_handlers[session->pending_packet_opcode];
                tc_thread_schedule_next(
                    &session->server->thread_pool,
                    handler,
                    tc_server_shutdown_client_task,
                    session,
                    TC_THREAD_POOL_TASK_PRIORITY_HIGH
                );
                return;
            }
        }
        else if (read_size == 0) { // client disconnected
            p_error_free(error);
            kick_session(session, "Client Disconnected: Please reconnect.");
            return;
        }
    }
    // if no data was read and the socket is not blocking, yeild
    if (error && p_error_get_code(error) == P_ERROR_IO_WOULD_BLOCK) {
        tc_thread_schedule_next(
            &session->server->thread_pool,
            tc_server_client_listen_task,
            tc_server_shutdown_client_task,
            session,
            TC_THREAD_POOL_TASK_PRIORITY_HIGH
        );
        p_error_free(error);
        return;
    }

    // if an error occurred, kick the session
    kick_session(session, "Error: Please reconnect.");
    p_error_free(error);
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
    session->pending_packet_opcode = -1;
    session->pending_packet_buffer = NULL;
    session->pending_packet_buffer_size = 0;

    pboolean schedule_success = tc_thread_schedule_new(
        &server->thread_pool,
        tc_server_client_listen_task,
        session,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH
    );
    if (!schedule_success) {
        kick_session(session, "Server is Busy: Please try again or come back soon.");
    }
}

// Main task chain for listining/accepting new clients
static void listener_worker_task(void *arg) {
    tc_server_t *server = (tc_server_t *)arg;

    PSocket* client_socket = p_socket_accept(server->listener_socket, NULL);
    if (client_socket) {
        p_socket_set_blocking(client_socket, FALSE);
        handle_new_session(server, client_socket);
    }

    // enque the listener worker again yeilding to other tasks
    tc_thread_schedule_next(
        &server->thread_pool,
        listener_worker_task,
        tc_server_shutdown_client_task,
        server,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH
    );
    return;
}

pboolean tc_server_start(tc_server_t *server) {
    if (server->started) {
        return FALSE;
    }

    pboolean listen_success = p_socket_listen(server->listener_socket, NULL);
    if (!listen_success) {
        return FALSE;
    }

    pboolean schedule_success = tc_thread_schedule_new(
        &server->thread_pool,
        listener_worker_task,
        server,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH
    );
    if (!schedule_success) {
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