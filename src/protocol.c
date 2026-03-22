#include <plibsys.h>
#include <TeleClassic26/networking/protocol.h>

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

const psize tc_protocol_packet_sizes[TC_PROTOCOL_TOTAL_PACKETS] = {
    [0x00] = 131 // player identification packet 
};

const tc_thread_pool_task_func_t tc_protocol_packet_handlers[TC_PROTOCOL_TOTAL_PACKETS] = { 
    [0x00] = NULL, // not implemented
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

pboolean tc_protocol_kick(PSocket* session, const pchar msg[]) {
    if (!send_byte(session, 0x0e)) {
        return FALSE;
    }
    if (!send_string(session, msg)) {
        return FALSE;
    }
    return TRUE;
}