#ifndef TELECLASSIC26_GAMEPLAY_MAP_H
#define TELECLASSIC26_GAMEPLAY_MAP_H

#include <plibsys.h>
#include <stddef.h>
#include <string.h>
#include <TeleClassic26/utils.h>
#include <TeleClassic26/gameplay/positions.h>

// Infrastructure for a map/world
typedef enum tc_map_generation_mode {
    TC_MAP_GENERATION_MODE_FLAT = 0
} tc_map_generation_mode_t;

typedef struct tc_map_custom_blocks_extension {
    pchar* fallback_blocks;
    pint32 extension_version;
    pint16 support_level;
} tc_map_custom_blocks_extension_t;

typedef struct tc_map_color_config {
    pint16 red;
    pint16 green;
    pint16 blue;
} tc_map_color_config_t;

typedef struct tc_map_env_colors_extension {
    tc_map_color_config_t sky;
    tc_map_color_config_t cloud;
    tc_map_color_config_t fog;
    tc_map_color_config_t ambient;
    tc_map_color_config_t sunlight;
    pint32 extension_version;
} tc_map_env_colors_extension_t;

typedef struct tc_map_env_aspect_extension {
    pchar side_block;
    pchar edge_block;

    pint32 clouds_height;
    pfloat clouds_speed;
    pint32 edge_height;
    pchar use_exponential_fog;
    pint32 side_offset;
    pfloat weather_fade;
    pfloat weather_speed;
    
    pint32 extension_version;
} tc_map_env_aspect_extension_t;

typedef struct tc_map_env_appearance_extension {
    pchar* texture_url;
    pchar side_block;
    pchar edge_block;
    pint16 side_level;

    //optional (for ver. 2)
    pint16 cloud_level;
    pint16 maximum_view_distance;

    pint32 extension_version;
} tc_map_env_appearance_extension_t;

typedef struct tc_map_env_weather_extension {
    pint32 extension_version;
    pchar weather_type;
} tc_map_env_weather_extension_t;

typedef struct tc_map_block_definition {
    pchar textures[6];
    pchar coords[6];
    pchar fog[4];

    pchar *block_name;
    pfloat speed;

    pboolean allow_placement;
    pboolean allow_deletion;

    pchar transmits_light;
    pchar walk_sound;

    pchar shape;
    pchar block_draw;

    pchar block_id;
} tc_map_block_definition_t;

typedef struct tc_map_block_definition_extension {
    pint32 extension_version;
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

    // we support up to 10 bits per block, up to 1024 total blocks
    pchar* block_array; //store the 0th to 7th bit for block data (one byte per block)
    pchar* block_array2; //store the 8th and 9th bit for extended block data (two bits per block)
    psize block_array_count;
    psize block_array2_count;

    tc_map_custom_blocks_extension_t* custom_blocks_extension;
    tc_map_env_colors_extension_t* env_colors_extension;
    tc_map_env_appearance_extension_t* env_appearance_extension;
    tc_map_env_weather_extension_t* env_weather_extension;
    tc_map_env_aspect_extension_t* env_aspect_extension;
    tc_map_block_definition_extension_t* block_definition_extensions;

    pint16 x_size;
    pint16 y_size;
    pint16 z_size;

    spawn_position_t spawn_position;

    pchar format_version; 

    // stuff for teleclassic26
    PRWLock* lock;
    pboolean is_dirty;
} tc_map_t;

#define TELECLASSIC26_MAP_BLOCK_INDEX(map, x, y, z) (y * map->z_size + z) * map->x_size + x

static inline puint16 tc_map_get_block_index(tc_map_t *map, pshort x, pshort y, pshort z) {
    p_rwlock_reader_lock(map->lock);
    psize index = TELECLASSIC26_MAP_BLOCK_INDEX(map, x, y, z);
    puint16 block = map->block_array[index] & 0xFF;
    if (map->block_array2) {
        puint16 shift = (index % 4) * 2;
        block |= ((map->block_array2[index / 4] >> shift) & 0x3) << 8;
    }
    p_rwlock_reader_unlock(map->lock);
    return block;
}

static inline void tc_map_set_block_index(tc_map_t *map, pshort x, pshort y, pshort z, puint16 block) {
    p_rwlock_writer_lock(map->lock);
    psize index = TELECLASSIC26_MAP_BLOCK_INDEX(map, x, y, z);
    map->is_dirty = TRUE;
    if (map->block_array2) {
        puint16 shift = (index % 4) * 2;
        map->block_array2[index / 4] = (pchar)((map->block_array2[index / 4] & ~(0x3 << shift))
                                      | (((block >> 8) & 0x3) << shift));
    }
    map->block_array[index] = (pchar)(block & 0xFF);
    p_rwlock_writer_unlock(map->lock);
}

static inline psize tc_map_get_memory_usage(tc_map_t *map) {
    psize block_array_size = map->x_size * map->y_size * map->z_size;
    return sizeof(tc_map_t)
        + (map->block_array2 ? (block_array_size * 10) / 8: block_array_size) //blocks
        + (map->custom_blocks_extension ? sizeof(tc_map_custom_blocks_extension_t) : 0) //custom blocks extension
        + (map->env_colors_extension ? sizeof(tc_map_env_colors_extension_t) : 0) //env colors extension
        + (map->env_appearance_extension ? sizeof(tc_map_env_appearance_extension_t) : 0) //env appearance extension
        + (map->env_weather_extension ? sizeof(tc_map_env_weather_extension_t) : 0) //env weather extension
        + (map->block_definition_extensions ? sizeof(tc_map_block_definition_extension_t) : 0) //block definition extensions
        + (map->name ? strlen(map->name) : 0) //name
        + (map->created_by_service ? strlen(map->created_by_service) : 0) //created by service
        + (map->created_by_username ? strlen(map->created_by_username) : 0); //created by username
}

void tc_map_generate(tc_map_t *map, const pchar* name, pshort x_size, pshort y_size, pshort z_size, tc_map_generation_mode_t generation_mode);
void tc_map_finalize(tc_map_t *map);

pboolean tc_map_load(tc_map_t *map, const pchar *path);
pboolean tc_map_save(tc_map_t *map, const pchar *path);

#endif /* TELECLASSIC26_GAMEPLAY_MAP_H */