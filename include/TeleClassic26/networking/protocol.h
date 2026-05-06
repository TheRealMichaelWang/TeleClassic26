#ifndef TELECLASSIC26_SESSION_H
#define TELECLASSIC26_SESSION_H

#include <plibsys.h>
#include <stdint.h>

#define TC_PROTOCOL_MAX_STR_LEN 64
#define TC_PROTOCOL_VERSION 0x07

#define TC_PROTOCOL_USER_TYPE_OPERATOR 0x64
#define TC_PROTOCOL_USER_TYPE_STANDARD 0x00

#define TC_CPE_EXTENSION_MAX_SUPPORTED 2

#define TC_CPE_CUSTOM_BLOCKS_EXTENSION_INDEX 0
#define TC_CPE_BLOCK_DEFINITIONS_EXTENSION_INDEX 1

#define TC_CPE_CUSTOM_BLOCKS_MAX_SUPPORT_LEVEL 1

typedef struct tc_cpe_extension {
    pchar name[TC_PROTOCOL_MAX_STR_LEN];
    pchar version; //must be between 0-3, inclusive
} tc_cpe_extension_t;

extern const tc_cpe_extension_t tc_supported_extensions[TC_CPE_EXTENSION_MAX_SUPPORTED];

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

// decodes a short from a packet buffer
// - dest_buffer: the buffer to write the decoded short to
// - packet_buffer: must be a ptr to addr within packet_data buffer
// NOTE: this writes out a max of 2 bytes to the dest_buffer
int16_t tc_protocol_decode_short(pchar* packet_buffer);

// decodes an integer from a packet buffer
// - dest_buffer: the buffer to write the decoded integer to
// - packet_buffer: must be a ptr to addr within packet_data buffer
// NOTE: this writes out a max of 4 bytes to the dest_buffer
int32_t tc_protocol_decode_int(pchar* packet_buffer);

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
// - appname: the name of the application to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_extinfo(PSocket* session, const char* appname);

// sends a CPE extentry packet
// - session: the session to send the packet to
// - extension_name: the name of the extension to send the packet to
// - extension_version: the version of the extension to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_extentry(PSocket* session, const pchar extension_name[TC_PROTOCOL_MAX_STR_LEN], pchar extension_version);

// sends a custom block support level packet
// - session: the session to send the packet to
// - support_level: the support level to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_custom_block_support_level(PSocket* session, pchar support_level);

// Gets the index of a supported extension by name
// - extension_name: the name of the extension to get the index of
// - return: the index of the extension, -1 if not found
pint tc_cpe_get_extension_index(const pchar extension_name[TC_PROTOCOL_MAX_STR_LEN]);

#endif /* TELECLASSIC26_SESSION_H */