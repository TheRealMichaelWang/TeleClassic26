#ifndef TELECLASSIC26_SESSION_H
#define TELECLASSIC26_SESSION_H

#include <plibsys.h>
#include <stdint.h>

#define TC_PROTOCOL_MAX_STR_LEN 64
#define TC_PROTOCOL_VERSION 0x07

#define TC_PROTOCOL_USER_TYPE_OPERATOR 0x64
#define TC_PROTOCOL_USER_TYPE_STANDARD 0x00

typedef struct tc_cpe_extension {
    pchar name[TC_PROTOCOL_MAX_STR_LEN];
    int32_t version;
} tc_cpe_extension_t;

extern const tc_cpe_extension_t tc_supported_extensions[];

// sends a byte to the socket
// - socket: the socket to send the byte to
// - opcode: the opcode/byte to send
// - return: TRUE if the byte was sent, FALSE otherwise
pboolean tc_protocol_send_byte(PSocket* socket, const pchar opcode);

// sends a short to the socket
// - socket: the socket to send the short to
// - short: the short to send
// - return: TRUE if the short was sent, FALSE otherwise
pboolean tc_protocol_send_short(PSocket* socket, const int16_t data);

// sends an integer to the socket
// - socket: the socket to send the integer to
// - integer: the integer to send
// - return: TRUE if the integer was sent, FALSE otherwise
pboolean tc_protocol_send_int(PSocket* socket, const int32_t data);

// sends a string to the socket
// - socket: the socket to send the string to
// - str: the string to send; only 64 bytes max will be sent
// - return: TRUE if the string was sent, FALSE otherwise
pboolean tc_protocol_send_string(PSocket* socket, const pchar str[]);

// decodes a string from a packet buffer
// - dest_buffer: the buffer to write the decoded string to
// - packet_buffer: must be a ptr to addr within packet_data buffer
// NOTE: this writes out a max of 64 bytes to the dest_buffer
psize tc_protocol_decode_string(pchar* dest_buffer, pchar* packet_buffer);

// sends a server identification packet
// - session: the session to send the packet to
// - server_name: the name of the server to send the packet to
// - motd: the message of the day to send the packet to
// - user_type: the type of user to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_server_identification(PSocket* session, const pchar server_name[], const pchar motd[], pchar user_type);

// sends a ping packet
// - session: the session to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_ping(PSocket* session);

// sends a kick packet
// - session: the session to send the packet to
// - msg: the message to send to the session
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_kick(PSocket* session, const pchar msg[]);

// sends a CPE extinfo packet
// - session: the session to send the packet to
// - appname: the name of the application to send the packet to (if NULL, then default to "TeleClassic26")
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_extinfo(PSocket* session, const char* appname);

// sends a CPE extentry packet
// - session: the session to send the packet to
// - extension: pointer tothe extension to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_extentry(PSocket* session, const tc_cpe_extension_t* extension);

#endif /* TELECLASSIC26_SESSION_H */