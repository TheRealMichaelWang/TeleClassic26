#include <TeleClassic26/networking/handler.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/networking/server.h>

static void handle_player_identification(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t* session = (tc_session_t*)arg;

    if (session->authenticated_service) {
        tc_server_kick_session(session, "Already Identified: Your client has a bug.");
        return;
    }

    // validate the protocol version
    pchar protocol_version = session->pending_packet_buffer[0];
    if (protocol_version <= TC_PROTOCOL_VERSION) {
        tc_server_kick_session(session, "Invalid Protocol Version: Please update your client.");
        return;
    }

    // copy the username from the packet buffer
    tc_protocol_decode_string(session->username, &session->pending_packet_buffer[1]);

    // copy the key from the packet buffer
    pchar key[TC_PROTOCOL_MAX_STR_LEN];
    tc_protocol_decode_string(key, &session->pending_packet_buffer[1 + TC_PROTOCOL_MAX_STR_LEN]);

    session->authenticated_service = tc_heartbeat_manager_validate(
        &session->server->heartbeat_manager, 
        session->username, 
        key
    );
    if (!session->authenticated_service) {
        tc_server_kick_session(session, "Could not validate your identity.");
        return;
    }

    pboolean identify_success = tc_protocol_server_identification(
        session->client_socket, 
        session->server->heartbeat_manager.info.server_name, 
        session->server->motd, 
        TC_PROTOCOL_USER_TYPE_STANDARD
    );
    if (!identify_success) {
        tc_server_kick_session(session, "Could not identify you to the server.");
        return;
    }

    session->supports_cpe = (session->pending_packet_buffer[129] == 0x42);

    tc_server_protocol_handler_cleanup(session, NULL);

    pboolean schedule_success = tc_thread_schedule_new(
        &session->server->thread_pool,
        tc_server_client_listen_task,
        session,
        TC_THREAD_POOL_TASK_PRIORITY_HIGH
    );
    if (!schedule_success) {
        tc_server_kick_session(session, "Server is Busy: Please try again or come back soon.");
        return;
    }

    p_atomic_int_inc(&session->server->active_players);
    return;
}

const psize tc_packet_data_sizes[TC_PACKET_HANDLERS_MAX_PACKETS] = {
    [0x00] = 130 // player identification packet 
};

const tc_thread_pool_task_t tc_packet_handlers[TC_PACKET_HANDLERS_MAX_PACKETS] = { 
    [0x00] = handle_player_identification,
};