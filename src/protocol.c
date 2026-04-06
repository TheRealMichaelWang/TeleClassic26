#include <plibsys.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/networking/server.h>
#include <stdint.h>
#include <string.h>

const tc_cpe_extension_t tc_supported_extensions[] = {
    
};

pboolean tc_protocol_send_byte(PSocket* socket, const pchar opcode) {
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

pboolean tc_protocol_send_short(PSocket* socket, const int16_t data) {
    pchar high = data >> 8;
    pchar low = data & 0xFF;

    if (!tc_protocol_send_byte(socket, high)) {
        return FALSE;
    }
    if (!tc_protocol_send_byte(socket, low)) {
        return FALSE;
    }
    return TRUE;
}

pboolean tc_protocol_send_int(PSocket* socket, const int32_t data) {
    for (pint i = 0; i < sizeof(int32_t); i++) {
        pchar byte = (data >> (i * 8)) & 0xFF;
        if (!tc_protocol_send_byte(socket, byte)) {
            return FALSE;
        }
    }
    return TRUE;
}

pboolean tc_protocol_send_string(PSocket* socket, const pchar str[]) {
    pchar buffer[TC_PROTOCOL_MAX_STR_LEN];

    const pchar* current = &str[0];
    while (*current != '\0') {
        buffer[current - str] = *current;
        current++;
    }
    for (psize i = current - str; i < TC_PROTOCOL_MAX_STR_LEN; i++) {
        buffer[i] = 0x20;
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

psize tc_protocol_decode_string(pchar* dest_buffer, pchar* packet_buffer) {
    psize length = TC_PROTOCOL_MAX_STR_LEN;
    memcpy(dest_buffer, packet_buffer, TC_PROTOCOL_MAX_STR_LEN);
    for (psize i = TC_PROTOCOL_MAX_STR_LEN; i > 0; i--) {
        if (dest_buffer[i - 1] != 0x20) break;
        dest_buffer[i - 1] = '\0';
        length = i - 1;
    }

    return length;
}

pboolean tc_protocol_server_identification(PSocket* session, const pchar server_name[], const pchar motd[], pchar user_type) {
    if (!tc_protocol_send_byte(session, 0x00)) {
        return FALSE;
    }
    if (!tc_protocol_send_byte(session, TC_PROTOCOL_VERSION)) {
        return FALSE;
    }
    if (!tc_protocol_send_string(session, server_name)) {
        return FALSE;
    }
    if (!tc_protocol_send_string(session, motd)) {
        return FALSE;
    }
    if (!tc_protocol_send_byte(session, user_type)) {
        return FALSE;
    }
    return TRUE;
}

pboolean tc_protocol_ping(PSocket* session) {
    if (!tc_protocol_send_byte(session, 0x01)) {
        return FALSE;
    }
    return TRUE;
}

pboolean tc_protocol_kick(PSocket* session, const pchar msg[]) {
    if (!tc_protocol_send_byte(session, 0x0e)) {
        return FALSE;
    }
    if (!tc_protocol_send_string(session, msg)) {
        return FALSE;
    }
    return TRUE;
}

pboolean tc_cpe_send_extinfo(PSocket* session, const char* appname) {
    if (!tc_protocol_send_byte(session, 0x10)) {
        return FALSE;
    }
    if (!tc_protocol_send_string(session, appname ? appname : "TeleClassic26")) {
        return FALSE;
    }

    psize num_supported_extensions = sizeof(tc_supported_extensions) / sizeof(tc_cpe_extension_t);
    if (!tc_protocol_send_short(session, num_supported_extensions)) {
        return FALSE;
    }

    return TRUE;
}

pboolean tc_cpe_send_extentry(PSocket* session, const tc_cpe_extension_t* extension) {
    if (!tc_protocol_send_byte(session, 0x11)) {
        return FALSE;
    }
    if (!tc_protocol_send_string(session, extension->name)) {
        return FALSE;
    }
    if (!tc_protocol_send_int(session, extension->version)) {
        return FALSE;
    }
    return TRUE;
}