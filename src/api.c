#include "TeleClassic26/gameplay/map.h"
#include "TeleClassic26/gameplay/map_cache.h"
#include "TeleClassic26/networking/protocol.h"
#include "TeleClassic26/task_backlog.h"
#include <TeleClassic26/networking/server.h>
#include <TeleClassic26/networking/api.h>
#include <TeleClassic26/log.h>
#include <TeleClassic26/thread_pool.h>
#include <TeleClassic26/gameplay/map_cache.h>
#include <TeleClassic26/gameplay/blocks.h>
#include <TeleClassic26/utils.h>
#include <plibsys.h>
#include <string.h>

pboolean tc_api_send_message(tc_session_t* session, tc_message_type_t message_type, const pchar message[]) {
    if (tc_session_get_extension_version(session, TC_CPE_MESSAGE_TYPES_EXTENSION_INDEX) > 0) {
        return tc_protocol_send_message(session->client_socket, (pchar)message_type, message);
    }
    return tc_protocol_send_message(session->client_socket, 0, message);
}

typedef struct tc_send_map_data {
    tc_session_t* session;
    const pchar* file_name;

    tc_map_t* map;
    tc_task_backlog_entry_t schedule_info;

    send_buffer_t block_array_buffer;
    send_buffer_t block_array2_buffer;
} tc_send_map_data_t;

typedef struct tc_send_buffer_task_data {
    tc_send_map_data_t* send_map_data;
    psize sent_count;

    pboolean send_main_buffer; //true=block array1, false=block array2
} tc_send_buffer_task_data_t;

static void free_send_map_data(tc_send_map_data_t* send_map_data) {
    if (send_map_data->map) {
        tc_map_unref(send_map_data->map);
    }
    if (send_map_data->block_array_buffer.data) {
        p_free(send_map_data->block_array_buffer.data);
    }
    if (send_map_data->block_array2_buffer.data) {
        p_free(send_map_data->block_array2_buffer.data);
    }
    p_free(send_map_data);
}

static void handle_success(tc_send_map_data_t* send_map_data, tc_thread_pool_task_priority_t current_priority, pint session_generation) {
    tc_session_release_action_lock(send_map_data->session);
    p_rwlock_reader_unlock(send_map_data->map->lock);

    tc_task_backlog_invoke_handler(
        &send_map_data->schedule_info, 
        &send_map_data->session->server->thread_pool, 
        NULL, // map already ref + 1 in schedule map load 
        (tc_task_backlog_release_handler_t)tc_map_unref,
        send_map_data->map, 
        current_priority
    );

    free_send_map_data(send_map_data);
}

static void handle_failure(tc_send_map_data_t* send_map_data, pint session_generation, pboolean release_lock) {
    if (release_lock) {
        tc_session_release_action_lock(send_map_data->session);
    }
    if (send_map_data->map) {
        p_rwlock_reader_unlock(send_map_data->map->lock);
    }

    tc_task_backlog_invoke_handler(
        &send_map_data->schedule_info, 
        &send_map_data->session->server->thread_pool, 
        NULL, 
        NULL,
        NULL,
        TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
    );
    free_send_map_data(send_map_data);
}

// task to schedule in case of failure for tc_send_buffer_task
static void tc_send_buffer_handle_failure(void* arg, tc_thread_pool_task_priority_t priority, pint session_generation) {
    tc_send_buffer_task_data_t* send_buffer_task_data = (tc_send_buffer_task_data_t*)arg;
    handle_failure(send_buffer_task_data->send_map_data, session_generation, TRUE);
    p_free(send_buffer_task_data);
    return;
}

// sends a chunk of the block array or block array2
static void tc_send_buffer_task(void* arg, tc_thread_pool_task_priority_t priority, pint session_generation) {
    tc_send_buffer_task_data_t* send_buffer_task_data = (tc_send_buffer_task_data_t*)arg;
    TC_ASSERT(priority == send_buffer_task_data->send_map_data->schedule_info.priority, "Buffer sending must be run with the requested priority.");

    send_buffer_t* selected_buffer = send_buffer_task_data->send_main_buffer 
        ? &send_buffer_task_data->send_map_data->block_array_buffer 
        : &send_buffer_task_data->send_map_data->block_array2_buffer;

    pchar current_chunk[1024];
    pint chunk_length = TC_MIN(1024, selected_buffer->size - send_buffer_task_data->sent_count);
    memcpy(current_chunk, &selected_buffer->data[send_buffer_task_data->sent_count], chunk_length);
    if (chunk_length < 1024) {
        memset(&current_chunk[chunk_length], 0, 1024 - chunk_length); //set the rest to all zeros
    }

    pchar percent_complete = tc_session_get_extension_version(send_buffer_task_data->send_map_data->session, TC_CPE_EXTENDED_BLOCKS_EXTENSION_INDEX) > 0 
        ? (pchar)(!send_buffer_task_data->send_main_buffer) 
        : (pchar)((float)send_buffer_task_data->sent_count * 100.0f / (float)selected_buffer->size);
    int send_result = tc_protocol_send_level_data_chunk(
        send_buffer_task_data->send_map_data->session->client_socket, 
        chunk_length, 
        current_chunk, 
        percent_complete
    );
    if (!send_result) {
        TC_LOG_SESSION(log_error, 
            send_buffer_task_data->send_map_data->session, 
            "Failed to send map %s: Could not send level data chunk packet (while sending bytes %zu - %zu of %zu).", 
            send_buffer_task_data->send_map_data->file_name,
            send_buffer_task_data->sent_count,
            send_buffer_task_data->sent_count + chunk_length,
            selected_buffer->size
        );
        handle_failure(send_buffer_task_data->send_map_data, session_generation, TRUE);
        p_free(send_buffer_task_data);
        return;
    }

    send_buffer_task_data->sent_count += chunk_length;
    if (send_buffer_task_data->sent_count >= selected_buffer->size) {
        if (send_buffer_task_data->send_main_buffer && send_buffer_task_data->send_map_data->map->block_array2) {
            send_buffer_task_data->send_main_buffer = FALSE;
            send_buffer_task_data->sent_count = 0;
        } else {
            int send_result = tc_protocol_send_level_finalize(
                send_buffer_task_data->send_map_data->session->client_socket, 
                send_buffer_task_data->send_map_data->map->x_size, 
                send_buffer_task_data->send_map_data->map->y_size, 
                send_buffer_task_data->send_map_data->map->z_size
            );
            if (!send_result) {
                TC_LOG_SESSION(log_error, send_buffer_task_data->send_map_data->session, "Failed to send map %s: Could not send level finalize packet.", send_buffer_task_data->send_map_data->file_name);
                handle_failure(send_buffer_task_data->send_map_data, session_generation, TRUE);
                p_free(send_buffer_task_data);
                return;
            }
            //world has been transmitted successfully
            handle_success(send_buffer_task_data->send_map_data, priority, session_generation);

            p_free(send_buffer_task_data);
            return;
        }
    }

    tc_thread_schedule_next(
        &send_buffer_task_data->send_map_data->session->server->thread_pool,
        tc_send_buffer_task, 
        tc_send_buffer_handle_failure, 
        send_buffer_task_data, 
        send_buffer_task_data->send_map_data->schedule_info.priority,
        session_generation
    );
}

// Task 2: Send the main block array level init packet and set up environment packets
static void tc_send_map_task2(void* arg, tc_thread_pool_task_priority_t priority, pint session_generation) {
    tc_send_map_data_t* send_map_data = (tc_send_map_data_t*)arg;
    TC_ASSERT(priority == send_map_data->schedule_info.priority, "Map sending must be run with the requested priority.");

    if (!tc_session_aquire_action_lock(send_map_data->session, session_generation)) {
        return;
    }

    #define HANDLE_FAILURE(ACTION, FAILURE_MESSAGE) { \
        if (!ACTION) { \
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map %s: " FAILURE_MESSAGE ".", send_map_data->file_name); \
            handle_failure(send_map_data, session_generation, TRUE); \
            return; \
        } \
    }

    if (tc_session_get_extension_version(send_map_data->session, TC_CPE_FASTMAP_EXTENSION_INDEX) > 0) {
        TC_ASSERT(send_map_data->block_array_buffer.size <= INT32_MAX, "Block array size must be less than or equal to INT32_MAX.");
        HANDLE_FAILURE(tc_protocol_send_level_initialize2(
            send_map_data->session->client_socket, 
            send_map_data->map->block_array_count
        ), "Could not send level initialize packet.");
    } else {
        HANDLE_FAILURE(tc_protocol_send_level_initialize(send_map_data->session->client_socket), "Could not send level initialize packet.");
    }

    if (send_map_data->map->env_aspect_extension) {
        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_ENV_MAP_ASPECT_EXTENSION_INDEX) <= 0) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): EnvMapAppearance extension is not supported but required.", send_map_data->map->name);
            handle_failure(send_map_data, session_generation, TRUE);
            return;
        }

        HANDLE_FAILURE(tc_cpe_send_set_texture_url(
            send_map_data->session->client_socket, 
            send_map_data->map->env_appearance_extension->texture_url
        ), "Could not send set map env url packet.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_SIDE_BLOCK, 
            send_map_data->map->env_aspect_extension->side_block
        ), "Could not send set map env packet for side block.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_EDGE_BLOCK, 
            send_map_data->map->env_aspect_extension->edge_block
        ), "Could not send set map env packet for edge block.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_EDGE_HEIGHT, 
            send_map_data->map->env_aspect_extension->edge_height
        ), "Could not send set map env packet for edge height.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_CLOUDS_HEIGHT, 
            send_map_data->map->env_aspect_extension->clouds_height
        ), "Could not send set map env packet for clouds height.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_CLOUDS_SPEED, 
            (pint32)(send_map_data->map->env_aspect_extension->clouds_speed * 256.0f)
        ), "Could not send set map env packet for clouds speed.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_WEATHER_SPEED, 
            (pint32)(send_map_data->map->env_aspect_extension->weather_speed * 256.0f)
        ), "Could not send set map env packet for weather speed.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_WEATHER_FADE, 
            (pint32)(send_map_data->map->env_aspect_extension->weather_fade * 128.0f)
        ), "Could not send set map env packet for weather fade.")   ;

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_USE_EXPONENTIAL_FOG, 
            send_map_data->map->env_aspect_extension->use_exponential_fog
        ), "Could not send set map env packet for use exponential fog.");

        HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
            send_map_data->session->client_socket, 
            TC_CPE_MAP_ENV_PROPERTY_SIDE_OFFSET, 
            send_map_data->map->env_aspect_extension->side_offset
        ), "Could not send set map env packet for side offset.");
    } else if (send_map_data->map->env_appearance_extension) {
        pint client_supported_version = tc_session_get_extension_version(send_map_data->session, TC_CPE_ENV_MAP_APPEARANCE_EXTENSION_INDEX);
        if (send_map_data->map->env_appearance_extension->extension_version > client_supported_version) {
            if (tc_session_get_extension_version(send_map_data->session, TC_CPE_ENV_MAP_ASPECT_EXTENSION_INDEX) <= 0) {
                TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): EnvMapAppearance extension is not supported but required.", send_map_data->map->name);
                handle_failure(send_map_data, session_generation, TRUE);
                return;
            }

            // fall back on env aspect extension if supported
            HANDLE_FAILURE(tc_cpe_send_set_texture_url(
                send_map_data->session->client_socket, 
                send_map_data->map->env_appearance_extension->texture_url
            ), "Could not send set map env url packet.");

            HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
                send_map_data->session->client_socket, 
                TC_CPE_MAP_ENV_PROPERTY_SIDE_BLOCK, 
                send_map_data->map->env_appearance_extension->side_block
            ), "Could not send set map env packet for side block.");

            HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
                send_map_data->session->client_socket, 
                TC_CPE_MAP_ENV_PROPERTY_EDGE_BLOCK, 
                send_map_data->map->env_appearance_extension->edge_block
            ), "Could not send set map env packet for edge block.");

            HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
                send_map_data->session->client_socket, 
                TC_CPE_MAP_ENV_PROPERTY_EDGE_HEIGHT, 
                send_map_data->map->env_appearance_extension->side_level
            ), "Could not send set map env packet for side level.");

            HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
                send_map_data->session->client_socket, 
                TC_CPE_MAP_ENV_PROPERTY_CLOUDS_HEIGHT, 
                send_map_data->map->env_appearance_extension->cloud_level
            ), "Could not send set map env packet for cloud level.");
            
            HANDLE_FAILURE(tc_cpe_send_set_map_env_property(
                send_map_data->session->client_socket, 
                TC_CPE_MAP_ENV_PROPERTY_MAX_VIEW_DISTANCE, 
                send_map_data->map->env_appearance_extension->maximum_view_distance
            ), "Could not send set map env packet for maximum view distance.");
        }

        if (client_supported_version == 2) { //ver2
            HANDLE_FAILURE(tc_cpe_send_env_map_appearance2(
                send_map_data->session->client_socket, 
                send_map_data->map->env_appearance_extension->texture_url, 
                send_map_data->map->env_appearance_extension->side_block, 
                send_map_data->map->env_appearance_extension->edge_block, 
                send_map_data->map->env_appearance_extension->side_level, 
                send_map_data->map->env_appearance_extension->cloud_level, 
                send_map_data->map->env_appearance_extension->maximum_view_distance
            ), "Could not send set map env packet for env map appearance v2.");
        } else {
            TC_ASSERT(client_supported_version == 1, "EnvMapAppearance extension must be either version 1 or 2.");

            HANDLE_FAILURE(tc_cpe_send_env_map_appearance1(
                send_map_data->session->client_socket, 
                send_map_data->map->env_appearance_extension->texture_url, 
                send_map_data->map->env_appearance_extension->side_block, 
                send_map_data->map->env_appearance_extension->edge_block, 
                send_map_data->map->env_appearance_extension->side_level
            ), "Could not send set map env packet for env map appearance v1.");
        }
    }

    if (send_map_data->map->env_weather_extension) {
        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_ENV_MAP_WEATHER_TYPE_EXTENSION_INDEX) <= 0) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): EnvMapWeatherType extension is not supported but required.", send_map_data->map->name);
            handle_failure(send_map_data, session_generation, TRUE);
            return;
        }

        HANDLE_FAILURE(tc_cpe_send_env_set_weather_type(
            send_map_data->session->client_socket, 
            send_map_data->map->env_weather_extension->weather_type
        ), "Could not send set map env packet for weather type.");
    }

    if (send_map_data->map->env_colors_extension) {
        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_ENV_MAP_COLORS_EXTENSION_INDEX) <= 0) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): EnvMapColors extension is not supported but required.", send_map_data->map->name);
            handle_failure(send_map_data, session_generation, TRUE);
            return;
        }
     
        HANDLE_FAILURE(tc_cpe_send_set_env_color(
            send_map_data->session->client_socket, 
            TC_CPE_ENV_COLOR_SKY, 
            send_map_data->map->env_colors_extension->sky.red, 
            send_map_data->map->env_colors_extension->sky.green, 
            send_map_data->map->env_colors_extension->sky.blue
        ), "Could not send set map env packet for sky color.");
        
        HANDLE_FAILURE(tc_cpe_send_set_env_color(
            send_map_data->session->client_socket, 
            TC_CPE_ENV_COLOR_CLOUD, 
            send_map_data->map->env_colors_extension->cloud.red, 
            send_map_data->map->env_colors_extension->cloud.green, 
            send_map_data->map->env_colors_extension->cloud.blue
        ), "Could not send set map env packet for cloud color.");
        
        HANDLE_FAILURE(tc_cpe_send_set_env_color(
            send_map_data->session->client_socket, 
            TC_CPE_ENV_COLOR_FOG, 
            send_map_data->map->env_colors_extension->fog.red, 
            send_map_data->map->env_colors_extension->fog.green, 
            send_map_data->map->env_colors_extension->fog.blue
        ), "Could not send set map env packet for fog color.");

        HANDLE_FAILURE(tc_cpe_send_set_env_color(
            send_map_data->session->client_socket, 
            TC_CPE_ENV_COLOR_AMBIENT, 
            send_map_data->map->env_colors_extension->ambient.red, 
            send_map_data->map->env_colors_extension->ambient.green, 
            send_map_data->map->env_colors_extension->ambient.blue
        ), "Could not send set map env packet for ambient color.");
        
        HANDLE_FAILURE(tc_cpe_send_set_env_color(
            send_map_data->session->client_socket, 
            TC_CPE_ENV_COLOR_SUNLIGHT, 
            send_map_data->map->env_colors_extension->sunlight.red, 
            send_map_data->map->env_colors_extension->sunlight.green, 
            send_map_data->map->env_colors_extension->sunlight.blue
        ), "Could not send set map env packet for sunlight color.");
    }

    if (send_map_data->map->block_definition_extensions) {
        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_BLOCK_DEFINITIONS_EXTENSION_INDEX) <= 0) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): BlockDefinitions extension is not supported but required.", send_map_data->map->name);
            handle_failure(send_map_data, session_generation, TRUE);
            return;
        }
        
    }

    tc_send_buffer_task_data_t* send_buffer_task_data = p_malloc0(sizeof(tc_send_buffer_task_data_t));
    if (!send_buffer_task_data) {
        TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): Out of memory.", send_map_data->map->name);
        handle_failure(send_map_data, session_generation, TRUE);
        return;
    }

    send_buffer_task_data->send_map_data = send_map_data;
    send_buffer_task_data->sent_count = 0;
    send_buffer_task_data->send_main_buffer = TRUE;

    tc_thread_schedule_next(
        &send_map_data->session->server->thread_pool,
        tc_send_buffer_task,
        tc_send_buffer_handle_failure,
        send_buffer_task_data,
        send_map_data->schedule_info.priority,
        session_generation
    );
}

// Task 1: Load the map from the file system and gzip the block array and block array2
static void on_map_load_success(tc_task_backlog_args_t* args, tc_thread_pool_task_priority_t priority, pint session_generation) {
    tc_send_map_data_t* send_map_data = (tc_send_map_data_t*)args->context;
    send_map_data->map = (tc_map_t*)args->result; // set loaded map

    TC_ASSERT(priority == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING, "Map compression must be run with blocking priority");
    
    pboolean result; //aquire reader lock asap (will not be released b/c map is already refed via loading)
    p_rwlock_reader_lock(send_map_data->map->lock);
    if (tc_session_get_extension_version(send_map_data->session, TC_CPE_FASTMAP_EXTENSION_INDEX) > 0) {
        result = tc_deflate_byte_array(
            (puint8*)send_map_data->map->block_array,
            send_map_data->map->block_array_count,
            &send_map_data->block_array_buffer
        );
    } else {
        result = tc_gzip_byte_array(
            (puint8*)send_map_data->map->block_array,
            send_map_data->map->block_array_count,
            &send_map_data->block_array_buffer
        );
    }
    if (!result) {
        TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): Could not compress block array.", send_map_data->map->name);
        handle_failure(send_map_data, session_generation, FALSE);
        p_free(args); // do not forget to free backlog args
        return;
    }

    if (send_map_data->map->block_array2) {
        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_EXTENDED_BLOCKS_EXTENSION_INDEX) <= 0) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): ExtendedBlocks extension is not supported but required.", send_map_data->map->name);
            handle_failure(send_map_data, session_generation, FALSE);
            p_free(args); // do not forget to free backlog args
            return;
        }

        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_FASTMAP_EXTENSION_INDEX) > 0) {
            result = tc_deflate_byte_array(
                (puint8*)send_map_data->map->block_array2,
                send_map_data->map->block_array2_count,
                &send_map_data->block_array2_buffer
            );
        } else {
            result = tc_gzip_byte_array(
                (puint8*)send_map_data->map->block_array2,
                send_map_data->map->block_array2_count,
                &send_map_data->block_array2_buffer
            );
        }
        if (!result) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): Could not compress block array2.", send_map_data->map->name);
            handle_failure(send_map_data, session_generation, FALSE);
            p_free(args); // do not forget to free backlog args
            return;
        }
    }

    result = tc_thread_schedule_new(
        &send_map_data->session->server->thread_pool,
        tc_send_map_task2,
        send_map_data,
        send_map_data->schedule_info.priority,
        session_generation
    );
    if (!result) {
        TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): Could not schedule task 2.", send_map_data->map->name);
        handle_failure(send_map_data, session_generation, FALSE);
    }

    p_free(args); // do not forget to free backlog args
    return;
}

static void handle_map_load_failure(void* context, pint session_generation) {
    tc_send_map_data_t* send_map_data = (tc_send_map_data_t*)context;
    TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): Could not load map.", send_map_data->file_name);
    
    tc_task_backlog_invoke_handler(
        &send_map_data->schedule_info, 
        &send_map_data->session->server->thread_pool, 
        NULL, // NULL since we already reffed map in schedule map load
        (tc_task_backlog_release_handler_t)tc_map_unref, // unref map
        NULL,
        TC_THREAD_POOL_TASK_PRIORITY_INVALID //no prio needed cause invoking failure doesnt need scheduler
    );
    free_send_map_data(send_map_data);
}

pboolean tc_api_schedule_send_map(
    tc_session_t* session,
    const pchar* file_name, 
    tc_map_t* pre_loaded_map,
    tc_task_backlog_entry_t schedule_info,
    tc_thread_pool_task_priority_t current_priority
) {
    TC_LOG_SESSION(log_info, session, "Begin sending map %s", file_name);

    tc_send_map_data_t* send_map_data = p_malloc0(sizeof(tc_send_map_data_t));
    if (!send_map_data) {
        TC_LOG_SESSION(log_error, session, "Failed to send map %s: Out of Memory", file_name);
        return FALSE; // out of memory
    }

    send_map_data->session = session;
    send_map_data->file_name = file_name;
    send_map_data->map = NULL;
    send_map_data->schedule_info = schedule_info;

    tc_task_backlog_entry_t load_map_schedule_info = {
        .success_handler = on_map_load_success,
        .failure_handler = handle_map_load_failure,
        .context = send_map_data,
        .priority = TC_THREAD_POOL_TASK_PRIORITY_BLOCKING,
        .session_generation = schedule_info.session_generation
    };

    if (pre_loaded_map) {
        tc_task_backlog_invoke_handler(
            &load_map_schedule_info, 
            &session->server->thread_pool, 
            (tc_task_backlog_aquire_handler_t)tc_map_ref,
            (tc_task_backlog_release_handler_t)tc_map_unref,
            pre_loaded_map,
            current_priority
        );
    } else {
        tc_map_cache_schedule_open(
            &session->server->map_cache,
            file_name,
            &session->server->thread_pool,
            load_map_schedule_info,
            current_priority
        );
    }

    return TRUE;
}