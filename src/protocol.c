#include <plibsys.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/networking/server.h>
#include <string.h>

static pboolean send_byte(PSocket* socket, const pchar opcode) {
    PError *error = NULL;
    pssize sent = p_socket_send(socket, &opcode, 1, &error);
    if (sent > 0) {
        return TRUE;
    }
    if (sent == 0) { // disconnected
        p_error_free(error);
        return FALSE;
    }
    pboolean is_waiting = error && p_error_get_code(error) == P_ERROR_IO_WOULD_BLOCK;
    p_error_free(error);
    return is_waiting;
}

static pboolean send_string(PSocket* socket, const pchar str[]) {
    pchar buffer[TC_PROTOCOL_MAX_STR_LEN];

    const pchar* current = &str[0];
    while (*current != '\0') {
        buffer[current - str] = *current;
        current++;
    }
    for (psize i = current - str; i < TC_PROTOCOL_MAX_STR_LEN; i++) {
        buffer[i] = '\0';
    }

    PError *error = NULL;
    pssize sent = p_socket_send(
        socket,
        buffer,
        TC_PROTOCOL_MAX_STR_LEN,
        NULL
    );

    if (sent > 0) {
        return TRUE;
    }
    if (sent == 0) { // disconnected
        p_error_free(error);
        return FALSE;
    }
    pboolean is_waiting = error && p_error_get_code(error) == P_ERROR_IO_WOULD_BLOCK;
    p_error_free(error);
    return is_waiting;
}

static void handle_player_identification(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_session_t* session = (tc_session_t*)arg;

    if (session->authenticated) {
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
    memcpy(session->username, &session->pending_packet_buffer[1], TC_PROTOCOL_MAX_STR_LEN);

    pchar key[TC_PROTOCOL_MAX_STR_LEN];
    memcpy(key, &session->pending_packet_buffer[1 + TC_PROTOCOL_MAX_STR_LEN], TC_PROTOCOL_MAX_STR_LEN);

    if (!heartbeat_manager_validate(session->username, key)) {
        tc_server_kick_session(session, "Could not validate your identity.");
        return;
    }

    session->authenticated = TRUE;
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
}

const psize tc_protocol_packet_sizes[TC_PROTOCOL_TOTAL_PACKETS] = {
    [0x00] = 130 // player identification packet 
};

const tc_thread_pool_task_t tc_protocol_packet_handlers[TC_PROTOCOL_TOTAL_PACKETS] = { 
    [0x00] = handle_player_identification,
};

pboolean tc_protocol_server_identification(PSocket* session, const pchar server_name[], const pchar motd[], pchar user_type) {
    if (!send_byte(session, 0x00)) {
        return FALSE;
    }
    if (!send_byte(session, TC_PROTOCOL_VERSION)) {
        return FALSE;
    }
    if (!send_string(session, server_name)) {
        return FALSE;
    }
    if (!send_string(session, motd)) {
        return FALSE;
    }
    if (!send_byte(session, user_type)) {
        return FALSE;
    }
    return TRUE;
}

pboolean tc_protocol_ping(PSocket* session) {
    if (!send_byte(session, 0x01)) {
        return FALSE;
    }
    return TRUE;
}

pboolean tc_protocol_kick(PSocket* session, const pchar msg[]) {
    if (!send_byte(session, 0x0e)) {
        return FALSE;
    }
    if (!send_string(session, msg)) {
        return FALSE;
    }
    return TRUE;
}