#include "TeleClassic26/utils.h"
#include <TeleClassic26/networking/handler.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/networking/server.h>
#include <TeleClassic26/log.h>

static void finalize_player_identification(tc_session_t* session, tc_thread_pool_task_priority_t priority);

static void handle_begin_cpe_negotiation(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t* session = (tc_session_t*)arg;
    TC_ASSERT(session->supports_cpe, "Session does not support CPE.");

    TC_LOG_SESSION(log_info, session, "Begining CPE negotiation with client.");

    // send extinfo packet
    pboolean extinfo_success = tc_cpe_send_extinfo(session->client_socket, "TeleClassic26");
    if (!extinfo_success) {
        tc_server_kick_session(session, "Could not send CPE extinfo packet.");
        return;
    }

    // send extentry packets for each supported extension
    for (psize i = 0; i < TC_CPE_EXTENSION_MAX_SUPPORTED; i++) {
        pboolean extentry_success = tc_cpe_send_extentry(
            session->client_socket, 
            tc_supported_extensions[i].name, 
            tc_supported_extensions[i].version
        );
        if (!extentry_success) {
            tc_server_kick_session(session, "Could not send CPE extentry packet.");
            return;
        }
    }

    tc_server_protocol_handler_cleanup(session, tc_server_client_listen_task, priority);
}

static void handle_cpe_extinfo(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t* session = (tc_session_t*)arg;
    TC_ASSERT(session->supports_cpe, "Session does not support CPE.");

    if (session->remaining_cpe_ext_packets >= 0) {
        tc_server_kick_session(session, "Client cannot send more than 1 CPE extinfo packet.");
        return;
    }

    // copy the appname from the packet buffer
    pchar appname[TC_PROTOCOL_MAX_STR_LEN];
    tc_protocol_decode_string(appname, &session->pending_packet_buffer[1]);

    // read extension count
    pshort extension_count = tc_protocol_decode_short(&session->pending_packet_buffer[1 + TC_PROTOCOL_MAX_STR_LEN]);
    session->remaining_cpe_ext_packets = extension_count;

    TC_LOG_SESSION(log_info, session, "Received CPE extinfo packet (app: %.*s, extension count: %d)", TC_PROTOCOL_MAX_STR_LEN, appname, extension_count);

    tc_server_protocol_handler_cleanup(session, tc_server_client_listen_task, priority);
}

static void handle_cpe_extentry(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t* session = (tc_session_t*)arg;
    TC_ASSERT(session->supports_cpe, "Session does not support CPE.");

    if (session->remaining_cpe_ext_packets == 0) {
        tc_server_kick_session(session, "Client has already sent all extension packets.");
        return;
    }

    pchar extension_name[TC_PROTOCOL_MAX_STR_LEN];
    tc_protocol_decode_string(extension_name, &session->pending_packet_buffer[1]);
    pint extension_index = tc_cpe_get_extension_index(extension_name);
    if (extension_index < 0) {
        tc_server_kick_session(session, "Invalid extension name: TeleClassic26 does not support this extension.");
        return;
    }

    pint extension_version = tc_protocol_decode_int(&session->pending_packet_buffer[1 + TC_PROTOCOL_MAX_STR_LEN]);
    if (extension_version < 1 || extension_version > 3) {
        tc_server_kick_session(session, "Invalid extension version: TeleClassic26 only supports extensions 1-3.");
        return;
    }

    // log information
    TC_LOG_SESSION(log_info, session, "Received CPE extentry packet (extension: %.*s, version: %d)", TC_PROTOCOL_MAX_STR_LEN, extension_name, extension_version);

    // set the extension version in the ext_cpe_versions array
    session->ext_cpe_versions[extension_index / 4] |= ((extension_version & 0x3) << (extension_index % 4 * 2));

    if (session->remaining_cpe_ext_packets == 0) {
        finalize_player_identification(session, priority);
    } else {
        session->remaining_cpe_ext_packets--;
        tc_server_protocol_handler_cleanup(session, tc_server_client_listen_task, priority);
    }
}

static void finalize_player_identification(tc_session_t* session, tc_thread_pool_task_priority_t priority) {
    pboolean identify_success = tc_protocol_server_identification(
        session->client_socket, 
        session->server->heartbeat_manager.info.server_name, 
        session->server->motd, 
        TC_PROTOCOL_USER_TYPE_STANDARD
    );
    if (!identify_success) {
        tc_server_kick_session(session, "Could not send server identification packet.");
        return;
    }

    tc_server_protocol_handler_cleanup(session, NULL, priority);

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

    TC_LOG_SESSION(log_info, session, "Finished handshake (no CPE) successfully.");
}

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

    TC_LOG_SESSION(log_info, session, "User succesfully authenticated.");

    session->supports_cpe = (session->pending_packet_buffer[129] == 0x42);

    if (session->supports_cpe) {
        // schedule CPE negotiation
        tc_server_protocol_handler_cleanup(session, handle_begin_cpe_negotiation, priority);
    } else {
        finalize_player_identification(session, priority);
    }
}

const psize tc_packet_data_sizes[TC_PACKET_HANDLERS_MAX_PACKETS] = {
    [TC_PACKET_CPE_EXTINFO] = 66, // CPE extinfo packet
    [TC_PACKET_CPE_EXTENTRY] = 68, // CPE extentry packet
    [TC_PACKET_PLAYER_IDENTIFICATION] = 130 // player identification packet 
};

const tc_thread_pool_task_t tc_packet_handlers[TC_PACKET_HANDLERS_MAX_PACKETS] = { 
    [TC_PACKET_CPE_EXTINFO] = handle_cpe_extinfo,
    [TC_PACKET_CPE_EXTENTRY] = handle_cpe_extentry,
    [TC_PACKET_PLAYER_IDENTIFICATION] = handle_player_identification,
};