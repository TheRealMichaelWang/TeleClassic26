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
pboolean tc_cpe_send_set_texture_url(PSocket* session, const pchar texture_url[]);

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

// solidity values
enum {
    TC_CPE_SOLIDITY_WALK_THROUGH = 0, //i.e. air
    TC_CPE_SOLIDITY_SWIM_THROUGH = 1, //i.e. water, lava
    TC_CPE_SOLIDITY_SOLID = 2, //block collides with players
    TC_CPE_SOLIDITY_PARTIALLY_SLIPPERY = 3, //player slides IFF on top (i.e. ice)
    TC_CPE_SOLIDITY_FULLY_SLIPPERY = 4, //player slides IFF any touch contact
    TC_CPE_SOLIDITY_WATER = 5, //behaves like water
    TC_CPE_SOLIDITY_LAVA = 6, //behaves like lava
    TC_CPE_SOLIDITY_ROPE = 7, //behaves like rope
};

// walk sound values
enum {
    TC_CPE_WALK_SOUND_NONE = 0,
    TC_CPE_WALK_SOUND_WOOD = 1,
    TC_CPE_WALK_SOUND_GRAVEL = 2,
    TC_CPE_WALK_SOUND_GRASS = 3,
    TC_CPE_WALK_SOUND_STONE = 4,
    TC_CPE_WALK_SOUND_METAL = 5,
    TC_CPE_WALK_SOUND_GLASS = 6,
    TC_CPE_WALK_SOUND_WOOL = 7,
    TC_CPE_WALK_SOUND_SAND = 8,
    TC_CPE_WALK_SOUND_SNOW = 9
};

// block draw values
enum {
    TC_CPE_BLOCK_DRAW_OPAQUE = 0,
    TC_CPE_BLOCK_DRAW_TRANSPARENT_GLASS = 1,
    TC_CPE_BLOCK_DRAW_TRANSPARENT_LEAVES = 2,
    TC_CPE_BLOCK_DRAW_TRANSLUCENT = 3,
    TC_CPE_BLOCK_DRAW_GAS = 4
};

// sends a set block definition packet
// - session: the session to send the packet to
// - block_id: the id of the block to send the packet to
// - block_name: the name of the block to send the packet to
// - solidity: the solidity of the block to send the packet to
// - movement_speed: the movement speed of the block to send the packet to
// - top_texture_id: the id of the top texture to send the packet to
// - bottom_texture_id: the id of the bottom texture to send the packet to
// - side_texture_id: the id of the side texture to send the packet to
// - transmits_light: the transmits light of the block to send the packet to
// - walk_sound: the walk sound of the block to send the packet to
// - full_bright: the full bright of the block to send the packet to
// - shape: the shape of the block to send the packet to
// - block_draw: the block draw of the block to send the packet to
// - fog_density: the fog density of the block to send the packet to
// - fog_color_red: the fog color red of the block to send the packet to
// - fog_color_green: the fog color green of the block to send the packet to
// - fog_color_blue: the fog color blue of the block to send the packet to

// These are parameters that arent part of the packet
// - use_extended_blocks: whether to use extended blocks
// - use_extended_textures: whether to use extended textures

// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_set_block_definition(
    PSocket* session, 
    puint16 block_id,
    const pchar block_name[],
    pchar solidity,
    pchar movement_speed,
    puint16 top_texture_id,
    puint16 bottom_texture_id,
    puint16 side_texture_id,
    pchar transmits_light,
    pchar walk_sound,
    pchar full_bright,
    pchar shape,
    pchar block_draw,
    pchar fog_density,
    pchar fog_color_red,
    pchar fog_color_green,
    pchar fog_color_blue,

    pboolean use_extended_blocks,
    pboolean use_extended_textures
);

// removes a block definition packet
// - session: the session to send the packet to
// - block_definition: the block definition to remove the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_cpe_send_remove_block_definition(PSocket* session, puint16 block_id);

// sends a message to the session
// - session: the session to send the message to
// - player_id: the id of the player (always 0 unless CPE MessageTypes extension is supported)
// - message: the message to send (must be less or equal to 64 characters)
// - return: TRUE if the message was sent, FALSE otherwise
pboolean tc_protocol_send_message(PSocket* session, pint8 player_id, const pchar message[]);

// sends a level initialize packet
// - session: the session to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_level_initialize(PSocket* session);

// sends a level initialize packet (use this with FastMap CPE enabled)
// - session: the session to send the packet to
// - block_count: the number of blocks to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_level_initialize2(PSocket* session, pint32 block_count);

// sends a level data chunk packet
// - session: the session to send the packet to
// - data: the data to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_level_data_chunk(PSocket* session, puint16 chunk_length, const pchar chunk_data[1024], pchar percent_complete);

// sends a level finalize packet
// - session: the session to send the packet to
// - x size: the size of the x chunk to send the packet to
// - y size: the size of the y chunk to send the packet to
// - z size: the size of the z chunk to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_level_finalize(PSocket* session, pint16 x_size, pint16 y_size, pint16 z_size);

// sends a set block packet
// - session: the session to send the packet to
// - x: the x position of the block to send the packet to (short)
// - y: the y position of the block to send the packet to (short)
// - z: the z position of the block to send the packet to (short)
// - block: the block to send the packet to (byte)
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_set_block(PSocket* session, pint16 x, pint16 y, pint16 z, puint16 block, pboolean use_extended_blocks);

// sends a spawn player packet
// - session: the session to send the packet to
// - player_id: the id of the player to send the packet to
// - player_name: the name of the player to send the packet to
// - x: the x position of the player to send the packet to (fshort/fint)
// - y: the y position of the player to send the packet to (fshort/fint)
// - z: the z position of the player to send the packet to (fshort/fint)
// - heading: the heading of the player to send the packet to (byte)
// - pitch: the pitch of the player to send the packet to (byte)
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_spawn_player(
    PSocket* session, 
    pint8 player_id, 
    const pchar player_name[], 
    pint32 x, pint32 y, pint32 z, 
    pchar heading, pchar pitch, 
    pboolean use_extended_positions
);

// sends a set player position and orientation packet
// - session: the session to send the packet to
// - player_id: the id of the player to send the packet to
// - x: the x position of the player to send the packet to (fshort/fint)
// - y: the y position of the player to send the packet to (fshort/fint)
// - z: the z position of the player to send the packet to (fshort/fint)
// - heading: the heading of the player to send the packet to (byte)
// - pitch: the pitch of the player to send the packet to (byte)
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_set_player_position_and_orientation(
    PSocket* session, 
    pint8 player_id, 
    pint32 x, pint32 y, pint32 z, 
    pchar heading, pchar pitch, 
    pboolean use_extended_positions
);

// updates the position and orientation of a player
// - session: the session to send the packet to
// - player_id: the id of the player to send the packet to
// - delta_x: the delta x position of the player to send the packet to (fshort/fint)
// - delta_y: the delta y position of the player to send the packet to (fshort/fint)
// - delta_z: the delta z position of the player to send the packet to (fshort/fint)
// - heading: the heading of the player to send the packet to (byte)
// - pitch: the pitch of the player to send the packet to (byte)
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_send_update_player_position_and_orientation_delta(
    PSocket* session, 
    pint8 player_id, 
    pint32 delta_x, pint32 delta_y, pint32 delta_z, 
    pchar heading, pchar pitch, 
    pboolean use_extended_positions
);

// updates the position of a player
// - session: the session to send the packet to
// - player_id: the id of the player to send the packet to
// - delta_x: the delta x position of the player to send the packet to (fshort/fint)
// - delta_y: the delta y position of the player to send the packet to (fshort/fint)
// - delta_z: the delta z position of the player to send the packet to (fshort/fint)
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_update_player_position_delta(
    PSocket* session, 
    pint8 player_id, 
    pint32 delta_x, pint32 delta_y, pint32 delta_z, 
    pboolean use_extended_positions
);

// updates the heading and pitch of a player
// - session: the session to send the packet to
// - player_id: the id of the player to send the packet to
// - delta_heading: the delta heading of the player to send the packet to (byte)
// - delta_pitch: the delta pitch of the player to send the packet to (byte)
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_update_player_heading_and_pitch(
    PSocket* session, 
    pint8 player_id, 
    pchar heading, pchar pitch
);

// despawns a player
// - session: the session to send the packet to
// - player_id: the id of the player to send the packet to
// - return: TRUE if the packet was sent, FALSE otherwise
pboolean tc_protocol_despawn_player(PSocket* session, pint8 player_id);

// Gets the index of a supported extension by name
// - extension_name: the name of the extension to get the index of
// - return: the index of the extension, -1 if not found
pint tc_cpe_get_extension_index(const pchar extension_name[TC_PROTOCOL_MAX_STR_LEN]);

#endif /* TELECLASSIC26_SESSION_H */