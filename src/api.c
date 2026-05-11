#include <TeleClassic26/networking/server.h>
#include <TeleClassic26/networking/api.h>
#include <TeleClassic26/log.h>
#include <TeleClassic26/thread_pool.h>
#include <TeleClassic26/gameplay/map.h>
#include <TeleClassic26/utils.h>
#include <plibsys.h>

pboolean tc_api_send_message(tc_session_t* session, tc_message_type_t message_type, const pchar message[]) {
    if (tc_session_get_extension_version(session, TC_CPE_MESSAGE_TYPES_EXTENSION_INDEX) >= 0) {
        return tc_send_message(session->client_socket, (pchar)message_type, message);
    }
    return tc_send_message(session->client_socket, 0, message);
}

typedef struct tc_send_map_data {
    tc_session_t* session;
    const pchar* file_name;
    tc_thread_pool_task_priority_t priority;

    tc_map_t* map;
    send_buffer_t block_array_buffer;
    send_buffer_t block_array2_buffer;
} tc_send_map_data_t;

static void handle_failure(tc_send_map_data_t* send_map_data) {
    TC_ASSERT(send_map_data->priority != TC_THREAD_POOL_TASK_PRIORITY_BLOCKING, "Failure handler should not be of priority blocking");

    // handle the failure with the joinable if it exists
    if (send_map_data->session->current_joinable && send_map_data->session->current_joinable->handle_map_send_failure) {
        send_map_data->session->current_joinable->handle_map_send_failure(send_map_data->session->current_joinable, send_map_data->session, send_map_data->priority);
    }

    if (send_map_data->map) {
        tc_map_cache_unref(&send_map_data->session->server->map_cache, send_map_data->map);
    }
    p_free(send_map_data);
    
    p_atomic_int_set(&send_map_data->session->is_sending_map, 0); //reset the flag

    // unhandled failure, kick the session
    if (!send_map_data->session->current_joinable || !send_map_data->session->current_joinable->handle_map_send_failure) {
        tc_server_kick_session(send_map_data->session, "Unhandled map loading failure.");
    }
}

// Task 2: Send the main block array level init packet and set up environment packets
static void tc_send_map_task2(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_send_map_data_t* send_map_data = (tc_send_map_data_t*)arg;
    
}

// Task 1: Load the map from the file system and gzip the block array and block array2
static void tc_send_map_task1(void* arg, tc_thread_pool_task_priority_t priority) {
    tc_send_map_data_t* send_map_data = (tc_send_map_data_t*)arg;
    TC_ASSERT(priority == TC_THREAD_POOL_TASK_PRIORITY_BLOCKING, "Map loading/compression must be run with blocking priority");
    
    if (!send_map_data->map) {
        tc_map_t* map = tc_map_cache_open(&send_map_data->session->server->map_cache, send_map_data->file_name);
        if (!map) {
            TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map %s: Could not load map.", send_map_data->file_name);
            handle_failure(send_map_data);
            return;
        }
        send_map_data->map = map;
    }
    
    pboolean result;
    if (tc_session_get_extension_version(send_map_data->session, TC_CPE_FASTMAP_EXTENSION_INDEX) >= 0) {
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
        handle_failure(send_map_data);
        return;
    }

    if (send_map_data->map->block_array2) {
        if (tc_session_get_extension_version(send_map_data->session, TC_CPE_FASTMAP_EXTENSION_INDEX) >= 0) {
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
            handle_failure(send_map_data);
            return;
        }
    }

    result = tc_thread_schedule_new(
        &send_map_data->session->server->thread_pool,
        tc_send_map_task2,
        send_map_data,
        TC_THREAD_POOL_TASK_PRIORITY_MEDIUM
    );
    if (!result) {
        TC_LOG_SESSION(log_error, send_map_data->session, "Failed to send map (name %s): Could not schedule task 2.", send_map_data->map->name);
        handle_failure(send_map_data);
        return;
    }

    return;
}

pboolean tc_api_schedule_send_map(
    tc_session_t* session,
    const pchar* file_name, 
    tc_map_t* pre_loaded_map,
    tc_thread_pool_task_priority_t priority
) {
    TC_LOG_SESSION(log_info, session, "Begin sending map %s", file_name);
    if (!p_atomic_int_compare_and_exchange(&session->is_sending_map, 0, 1)) {
        TC_LOG_SESSION(log_error, session, "Failed to send map %s: another map is currently being sent.", file_name);
        return FALSE; // someone is already sending a map
    }

    tc_send_map_data_t* send_map_data = p_malloc0(sizeof(tc_send_map_data_t));
    if (!send_map_data) {
        TC_LOG_SESSION(log_error, session, "Failed to send map %s: Out of Memory", file_name);
        p_atomic_int_set(&session->is_sending_map, 0); // reset the flag
        return FALSE; // out of memory
    }

    send_map_data->session = session;
    send_map_data->file_name = file_name;
    send_map_data->priority = priority;
    send_map_data->map = pre_loaded_map;
    if (pre_loaded_map) {
        tc_map_cache_ref(&session->server->map_cache, pre_loaded_map);
    }

    int schedule_success = tc_thread_schedule_new(
        &session->server->thread_pool,
        tc_send_map_task1,
        send_map_data,
        priority
    );
    if (!schedule_success) {
        TC_LOG_SESSION(log_error, session, "Failed to send map %s: Failed to schedule task 1", file_name);
        p_free(send_map_data);
        p_atomic_int_set(&session->is_sending_map, 0); // reset the flag
        return FALSE; // failed to schedule task 1
    }

    return TRUE;
}