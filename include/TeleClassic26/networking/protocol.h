#ifndef TELECLASSIC26_SESSION_H
#define TELECLASSIC26_SESSION_H

#include <plibsys.h>
#include <stdint.h>

#define TC_PROTOCOL_MAX_STR_LEN 64
#define TC_PROTOCOL_VERSION 0x07

#define TC_PROTOCOL_USER_TYPE_OPERATOR 0x64
#define TC_PROTOCOL_USER_TYPE_STANDARD 0x00

#define TC_CPE_EXTENSION_MAX_SUPPORTED 10

#define TC_CPE_CUSTOM_BLOCKS_EXTENSION_INDEX 0
#define TC_CPE_BLOCK_DEFINITIONS_EXTENSION_INDEX 1
#define TC_CPE_EXTENDED_BLOCKS_EXTENSION_INDEX 2
#define TC_CPE_EXTENDED_TEXTURES_EXTENSION_INDEX 3
#define TC_CPE_MESSAGE_TYPES_EXTENSION_INDEX 4
#define TC_CPE_FASTMAP_EXTENSION_INDEX 5
#define TC_CPE_ENV_MAP_APPEARANCE_EXTENSION_INDEX 6
#define TC_CPE_ENV_MAP_WEATHER_TYPE_EXTENSION_INDEX 7
#define TC_CPE_ENV_MAP_COLORS_EXTENSION_INDEX 8
#define TC_CPE_ENV_MAP_ASPECT_EXTENSION_INDEX 9

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
pboolean tc_protocol_send_short(PSocket* socket, const pint16 data);

// sends an integer to the socket
// - socket: the socket to send the integer to
// - integer: the integer to send
// - return: TRUE if the integer was sent, FALSE otherwise
pboolean tc_protocol_send_int(PSocket* socket, const pint32 data);

// sends a string to the socket
// - socket: the socket to send the string to
// - str: the string to send; only 64 bytes max will be sent
// - return: TRUE if the string was sent, FALSE otherwise
pboolean tc_protocol_send_string(PSocket* socket, const pchar str[]);

// decodes a short from a packet buffer
// - dest_buffer: the buffer to write the decoded short to
// - packet_buffer: must be a ptr to addr within packet_data buffer
// NOTE: this writes out a max of 2 bytes to the dest_buffer
pint16 tc_protocol_decode_short(pchar* packet_buffer);

// decodes an integer from a packet buffer
// - dest_buffer: the buffer to write the decoded integer to
// - packet_buffer: must be a ptr to addr within packet_data buffer
// NOTE: this writes out a max of 4 bytes to the dest_buffer
pint32 tc_protocol_decode_int(pchar* packet_buffer);

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

// sends a set map env url packet
// - session: the session to send the packet to
// - texture_url: the url of the texture to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_set_map_env_url(PSocket* session, const pchar texture_url[]);



//map property values
enum {
    TC_CPE_MAP_ENV_PROPERTY_SIDE_BLOCK = 0,
    TC_CPE_MAP_ENV_PROPERTY_EDGE_BLOCK = 1,
    TC_CPE_MAP_ENV_PROPERTY_EDGE_HEIGHT = 2,
    TC_CPE_MAP_ENV_PROPERTY_CLOUDS_HEIGHT = 3,
    TC_CPE_MAP_ENV_PROPERTY_MAX_VIEW_DISTANCE = 4,
    TC_CPE_MAP_ENV_PROPERTY_CLOUDS_SPEED = 5,
    TC_CPE_MAP_ENV_PROPERTY_WEATHER_SPEED = 6,
    TC_CPE_MAP_ENV_PROPERTY_WEATHER_FADE = 7,
    TC_CPE_MAP_ENV_PROPERTY_USE_EXPONENTIAL_FOG = 8,
    TC_CPE_MAP_ENV_PROPERTY_SIDE_OFFSET = 9,
};
// sends a set map env property packet
// - session: the session to send the packet to
// - property: the property to send the packet to
// - value: the value of the property to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_set_map_env_property(PSocket* session, pchar property, pint32 value);

// sends an env map appearance v1 packet
// - session: the session to send the packet to
// - texture_url: the url of the texture to send the packet to
// - side_block: the side block to send the packet to
// - edge_block: the edge block to send the packet to
// - side_level: the side level to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_env_map_appearance1(PSocket* session, const pchar texture_url[], pchar side_block, pchar edge_block, pint16 side_level);

// sends an env map appearance v2 packet
// - session: the session to send the packet to
// - texture_url: the url of the texture to send the packet to
// - side_block: the side block to send the packet to
// - edge_block: the edge block to send the packet to
// - side_level: the side level to send the packet to
// - cloud_level: the cloud level to send the packet to
// - maximum_view_distance: the maximum view distance to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_env_map_appearance2(PSocket* session, const pchar texture_url[], pchar side_block, pchar edge_block, pint16 side_level, pint16 cloud_level, pint16 maximum_view_distance);

//weather type values
enum {
    TC_CPE_WEATHER_TYPE_SUNNY = 0,
    TC_CPE_WEATHER_TYPE_RAINING = 1,
    TC_CPE_WEATHER_TYPE_SNOWING = 2
};

// sends an env set weather type packet
// - session: the session to send the packet to
// - weather_type: the weather type to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_env_set_weather_type(PSocket* session, pchar weather_type);

//color field values
enum {
    TC_CPE_ENV_COLOR_SKY = 0,
    TC_CPE_ENV_COLOR_CLOUD = 1,
    TC_CPE_ENV_COLOR_FOG = 2,
    TC_CPE_ENV_COLOR_AMBIENT = 3,
    TC_CPE_ENV_COLOR_SUNLIGHT = 4,
    TC_CPE_ENV_COLOR_SKYBOX = 5
};

// sends a set env colors packet
// - session: the session to send the packet to
// - color_field: the color field to send the packet to
// - red: the red value of the color to send the packet to
// - green: the green value of the color to send the packet to
// - blue: the blue value of the color to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_set_env_color(PSocket* session, pchar color_field, pint16 red, pint16 green, pint16 blue);

// sends a message to the session
// - session: the session to send the message to
// - player_id: the id of the player (always 0 unless CPE MessageTypes extension is supported)
// - message: the message to send (must be less or equal to 64 characters)
// - return: TRUE if the message was sent, FALSE otherwise
pboolean tc_send_message(PSocket* session, pint8 player_id, const pchar message[]);

// sends a level initialize packet
// - session: the session to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_level_initialize(PSocket* session);

// sends a level initialize packet (use this with FastMap CPE enabled)
// - session: the session to send the packet to
// - block_count: the number of blocks to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_level_initialize2(PSocket* session, pint32 block_count);

// sends a level data chunk packet
// - session: the session to send the packet to
// - data: the data to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_level_data_chunk(PSocket* session, puint16 chunk_length, const pchar chunk_data[1024], pchar percent_complete);

// sends a level finalize packet
// - session: the session to send the packet to
// - x size: the size of the x chunk to send the packet to
// - y size: the size of the y chunk to send the packet to
// - z size: the size of the z chunk to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_level_finalize(PSocket* session, pint16 x_size, pint16 y_size, pint16 z_size);

// Gets the index of a supported extension by name
// - extension_name: the name of the extension to get the index of
// - return: the index of the extension, -1 if not found
pint tc_cpe_get_extension_index(const pchar extension_name[TC_PROTOCOL_MAX_STR_LEN]);

#endif /* TELECLASSIC26_SESSION_H */