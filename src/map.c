#include <TeleClassic26/gameplay/map.h>

static void free_block_definition(ppointer data, ppointer user_data) {
    tc_map_block_definition_t *block_definition = (tc_map_block_definition_t *)data;
    p_free(block_definition->block_name);
    p_free(block_definition);
}

void tc_map_free(tc_map_t *map) {
    p_free(map->name);
    p_free(map->created_by_service);
    p_free(map->created_by_username);
    p_free(map->map_gen_software);
    p_free(map->map_gen_name);
    p_free(map->block_array);

    // free all the extensions
    if (map->custom_blocks_extension) {
        p_free(map->custom_blocks_extension->fallback_blocks);
        p_free(map->custom_blocks_extension);
    }
    if (map->env_colors_extension) {
        p_free(map->env_colors_extension);
    }
    if (map->env_appearance_extension) {
        p_free(map->env_appearance_extension->texture_url);
        p_free(map->env_appearance_extension);
    }
    if (map->env_weather_extension) {
        p_free(map->env_weather_extension);
    }
    if (map->block_definition_extensions) {
        p_list_foreach(map->block_definition_extensions->block_definitions, free_block_definition, NULL);
        p_list_free(map->block_definition_extensions->block_definitions);
        p_free(map->block_definition_extensions);
    }
}