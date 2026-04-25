#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/networking/handler.h>
#include <TeleClassic26/networking/server.h>
#include "TeleClassic26/authentication/heartbeat.h"
#include "TeleClassic26/thread_pool.h"
#include <TeleClassic26/log.h>
#include <TeleClassic26/utils.h>
#include <plibsys.h>
#include <string.h>

pboolean tc_server_init(
    tc_server_t *server, 
    const char* hostname, 
    pint port, 
    pint listener_backlog,
    pint reserved_threads,
    tc_heartbeat_service_t* heartbeat_services,
    pint num_heartbeat_services,
    tc_heartbeat_info_t heartbeat_info
) {
    log_info("Initializing server...");

    server->address = p_socket_address_new(hostname, port);
    if (server->address == NULL) {
        log_error("Failed to create socket address at %s:%d", hostname, port);
        return FALSE;
    }

    server->listener_socket = p_socket_new(P_SOCKET_FAMILY_INET, P_SOCKET_TYPE_STREAM, P_SOCKET_PROTOCOL_TCP, NULL);
    if (server->listener_socket == NULL) {
        log_error("Failed to create listener socket");
        p_socket_address_free(server->address);
        return FALSE;
    }

    p_socket_set_listen_backlog(server->listener_socket, listener_backlog);
    p_socket_set_blocking(server->listener_socket, FALSE);
    p_socket_set_timeout(server->listener_socket, 50);

    if (!p_socket_bind(server->listener_socket, server->address, FALSE, NULL)) {
        log_error("Failed to bind listener socket");
        p_socket_free(server->listener_socket);
        p_socket_address_free(server->address);
        return FALSE;
    }

    if (!tc_thread_pool_init(&server->thread_pool, "AABAABC", reserved_threads)) {
        log_error("Failed to initialize thread pool");
        p_socket_free(server->listener_socket);
        p_socket_address_free(server->address);
        return FALSE;
    }

    if (!heartbeat_manager_init(
        &server->heartbeat_manager, 
        heartbeat_info,
        heartbeat_services,
        &server->active_players,
        num_heartbeat_services
    )) {
        log_error("Failed to initialize heartbeat manager");
        tc_thread_pool_stop(&server->thread_pool);
        tc_thread_pool_finalize(&server->thread_pool);
        p_socket_free(server->listener_socket);
        p_socket_address_free(server->address);
        return FALSE;
    }

    server->lock = p_mutex_new();

    for (pint i = 0; i < TC_SERVER_MAX_SESSIONS; i++) {
        server->id_buffer[i] = i;
    }
    server->id_buffer_head = 0;
    server->active_players = 0;

    log_info("Server initialized successfully");
    return TRUE;
}

void tc_server_finalize(tc_server_t *server) {
    log_info("Finalizing server...");

    if (server->started) {
        tc_server_stop(server);
    }

    if (server->started) {
        p_socket_close(server->listener_socket, NULL);
    }
    p_socket_address_free(server->address);
    p_socket_free(server->listener_socket);
    p_mutex_free(server->lock);

    tc_thread_pool_finalize(&server->thread_pool);
    tc_heartbeat_manager_finalize(&server->heartbeat_manager);
}

static void disconnect_session(tc_session_t* session) {
    TC_LOG_SESSION(log_info, session, "Disconnecting session %d...", session->id);

    // free the pending packet buffer
    if (session->pending_packet_buffer) {
        p_free(session->pending_packet_buffer);
        session->pending_packet_buffer = NULL;
    }

    // shutdown and free the client socket
    p_socket_shutdown(session->client_socket, TRUE, TRUE, NULL);
    p_socket_free(session->client_socket);

    // free the ping profiler
    if (session->ping_profiler) {
        p_time_profiler_free(session->ping_profiler);
    }

    // add the session id back to the id buffer
    p_mutex_lock(session->server->lock);
    
    if (session->authenticated_service) {
        p_atomic_int_dec_and_test(&session->server->active_players);
    }

    session->server->id_buffer_head--;
    session->server->id_buffer[session->server->id_buffer_head] = session->id;

    p_mutex_unlock(session->server->lock);
}

// kicks a session from the server
void tc_server_kick_session(tc_session_t* session, const char* msg) {
    if (msg) {
        if (session->authenticated_service) {
            TC_LOG_SESSION(log_info, session, "Kicking session %d.", session->id);
        }
        tc_protocol_kick(session->client_socket, msg);
    }
    disconnect_session(session);
}

// cleans up pending packet buffer and schedules next task in client task chain
void tc_server_protocol_handler_cleanup(tc_session_t* session, tc_thread_pool_task_t next_task, tc_thread_pool_task_priority_t priority) {
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
            priority
        );
    }
}

// kicks a session from the server because the server is busy
// Just a wrapper for kick_session with a default message
// - session: the session to kick
 void tc_server_shutdown_client_task(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t *session = (tc_session_t *)arg;
    tc_server_kick_session(session, "Server is Shutting Down: Please come back later!");
}

// worker thread for listening for new clients
void tc_server_client_listen_task(void *arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t *session = (tc_session_t *)arg;

    if (p_time_profiler_elapsed_usecs(session->ping_profiler) > TC_SERVER_PING_INTERVAL) {
        if (!tc_protocol_ping(session->client_socket)) {
            disconnect_session(session);
            return;
        }
        p_time_profiler_reset(session->ping_profiler);
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
            if (session->pending_packet_opcode < 0 || session->pending_packet_opcode >= TC_PACKET_HANDLERS_MAX_PACKETS) {
                p_error_free(error);
                tc_server_kick_session(session, "Packet Opcode Out of Range: Please reconnect.");
                return;
            }
            if (tc_packet_handlers[session->pending_packet_opcode] == NULL) {
                p_error_free(error);
                tc_server_kick_session(session, "Invalid Packet Opcode: Please reconnect.");
                return;
            }

            // allocate the buffer for the packet data
            psize packet_buffer_size = tc_packet_data_sizes[session->pending_packet_opcode];
            session->pending_packet_buffer = p_malloc(packet_buffer_size);
            session->pending_packet_buffer_size = 0;
            if (session->pending_packet_buffer == NULL) {
                p_error_free(error);
                tc_server_kick_session(session, "Out of Memory: Sorry");
                return;
            }
        }
        else if (read_size == 0) { // client disconnected
            tc_server_kick_session(session, "Client Disconnected: Please reconnect.");
            p_error_free(error);
            return;
        }
    } else {
        psize packet_buffer_size = tc_packet_data_sizes[session->pending_packet_opcode];

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
                tc_thread_pool_task_t handler = tc_packet_handlers[session->pending_packet_opcode];
                tc_thread_schedule_next(
                    &session->server->thread_pool,
                    handler,
                    tc_server_shutdown_client_task,
                    session,
                    priority
                );
                return;
            }
        }
        else if (read_size == 0) { // client disconnected
            p_error_free(error);
            tc_server_kick_session(session, "Client Disconnected: Please reconnect.");
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
            priority
        );
        p_error_free(error);
        return;
    }

    // if an error occurred, kick the session
    tc_server_kick_session(session, "Error: Please reconnect.");
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

    // Initialize the session here
    tc_session_t* session = &server->session_buffer[session_id];
    session->client_socket = client_socket;
    session->id = session_id;
    session->server = server;
    session->pending_packet_opcode = -1;
    session->pending_packet_buffer = NULL;
    session->pending_packet_buffer_size = 0;
    session->supports_cpe = FALSE;
    session->authenticated_service = NULL;
    session->remaining_cpe_ext_packets = -1;
    memset(session->ext_cpe_versions, 0, sizeof(session->ext_cpe_versions));

    session->ping_profiler = p_time_profiler_new();
    if (!session->ping_profiler) {
        tc_server_kick_session(session, "Out of Memory: Sorry");
        return;
    }

    pboolean schedule_success = tc_thread_schedule_new(
        &server->thread_pool,
        tc_server_client_listen_task,
        session,
        TC_THREAD_POOL_TASK_PRIORITY_LOW
    );
    if (!schedule_success) {
        log_warn("Task pool is busy, failed to accomodate new client.");
        tc_server_kick_session(session, "Server is Busy: Please try again or come back soon.");
    }
}

// Main task chain for listining/accepting new clients
static void listener_worker_task(void *arg, tc_thread_pool_task_priority_t priority) {
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
        NULL,
        server,
        priority
    );
    return;
}

pboolean tc_server_start(tc_server_t *server) {
    log_info("Starting server...");
    
    if (server->started) {
        return FALSE;
    }

    pboolean listen_success = p_socket_listen(server->listener_socket, NULL);
    if (!listen_success) {
        log_error("Failed to listen on socket");
        return FALSE;
    }

    pboolean schedule_success = tc_thread_schedule_new(
        &server->thread_pool,
        listener_worker_task,
        server,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH
    );
    if (!schedule_success) {
        log_error("Failed to schedule listener worker task");
        p_socket_close(server->listener_socket, NULL);
        return FALSE;
    }

    tc_heartbeat_manager_start(&server->heartbeat_manager);

    server->started = TRUE;
    return TRUE;
}

void tc_server_stop(tc_server_t *server) {
    if (!server->started) {
        return;
    }

    tc_heartbeat_manager_stop(&server->heartbeat_manager);
    tc_thread_pool_stop(&server->thread_pool);
}

pint tc_server_get_extension_version(tc_session_t* session, const pint extension_index) {
    if (extension_index < 0 || extension_index >= TC_CPE_EXTENSION_MAX_SUPPORTED) {
        return -1;
    }
    return (session->ext_cpe_versions[extension_index / 4] >> (extension_index % 4 * 2)) & 0x3;
}