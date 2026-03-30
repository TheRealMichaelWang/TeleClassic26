#ifndef TELECLASSIC26_GAMEPLAY_MAP_H
#define TELECLASSIC26_GAMEPLAY_MAP_H

#include <plibsys.h>
#include <TeleClassic26/utils.h>

typedef enum tc_map_generation_mode {
    TC_MAP_GENERATION_MODE_FLAT = 0
} tc_map_generation_mode_t;

typedef struct tc_map_custom_blocks_extension {
    pchar* fallback_blocks;
    pint extension_version;
    pshort support_level;
} tc_map_custom_blocks_extension_t;

typedef struct tc_map_color_config {
    pshort red;
    pshort green;
    pshort blue;
} tc_map_color_config_t;

typedef struct tc_map_env_colors_extension {
    tc_map_color_config_t sky;
    tc_map_color_config_t cloud;
    tc_map_color_config_t fog;
    tc_map_color_config_t ambient;
    tc_map_color_config_t sunlight;
    pint extension_version;
} tc_map_env_colors_extension_t;

typedef struct tc_map_env_appearance_extension {
    pchar* texture_url;
    pint extension_version;
    pshort side_level;
    pchar side_block;
    pchar edge_block;
} tc_map_env_appearance_extension_t;

typedef struct tc_map_env_weather_extension {
    pint extension_version;
    pchar weather_type;
} tc_map_env_weather_extension_t;

typedef struct tc_map_block_definition {
    pchar textures[6];
    pchar coords[6];
    pchar fog[4];

    pchar *block_name;
    pfloat speed;

    pchar transmits_light;
    pchar walk_sound;

    pchar shape;
    pchar block_draw;

    pchar block_id;
} tc_map_block_definition_t;

typedef struct tc_map_block_definition_extension {
    pint extension_version;
    PList* block_definitions;
} tc_map_block_definition_extension_t;

typedef struct tc_map {
    pchar uuid[TC_THREADS_UUID_LEN];

    plong time_created;
    plong last_accessed;
    plong last_modified;

    pchar* name;

    pchar* created_by_service;
    pchar* created_by_username;

    pchar* map_gen_software;
    pchar* map_gen_name;

    pchar* block_array;

    tc_map_custom_blocks_extension_t* custom_blocks_extension;
    tc_map_env_colors_extension_t* env_colors_extension;
    tc_map_env_appearance_extension_t* env_appearance_extension;
    tc_map_env_weather_extension_t* env_weather_extension;
    tc_map_block_definition_extension_t* block_definition_extensions;

    pshort x_size;
    pshort y_size;
    pshort z_size;

    pshort spawn_x;
    pshort spawn_y;
    pshort spawn_z;

    pchar spawn_heading;
    pchar spawn_pitch;

    pchar format_version; 
} tc_map_t;

void tc_map_generate(tc_map_t *map, const pchar* name, pshort x_size, pshort y_size, pshort z_size, tc_map_generation_mode_t generation_mode);
void tc_map_finalize(tc_map_t *map);

void tc_map_load(tc_map_t *map, const pchar *path);
void tc_map_save(tc_map_t *map, const pchar *path);

#endif /* TELECLASSIC26_GAMEPLAY_MAP_H */