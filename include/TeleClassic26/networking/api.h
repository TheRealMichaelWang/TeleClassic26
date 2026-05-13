#ifndef TELECLASSIC26_LEVEL_H
#define TELECLASSIC26_LEVEL_H

#include <plibsys.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/thread_pool.h>
#include <TeleClassic26/networking/server.h>
#include <TeleClassic26/gameplay/map.h>

// message types for the server to send to the client
// see MessageTypes CPE extension on wiki.vg for details
typedef enum tc_message_type {
    TC_MESSAGE_TYPE_CHAT = 0,
    TC_MESSAGE_TYPE_STATUS1 = 1,
    TC_MESSAGE_TYPE_STATUS2 = 2,
    TC_MESSAGE_TYPE_STATUS3 = 3,
    TC_MESSAGE_TYPE_BOTTOM_RIGHT1 = 11,
    TC_MESSAGE_TYPE_BOTTOM_RIGHT2 = 12,
    TC_MESSAGE_TYPE_BOTTOM_RIGHT3 = 13,
    TC_MESSAGE_TYPE_ANNOUNCEMENT = 100,
} tc_message_type_t;

// sends a message to the session
// - session: the session to send the message to
// - message: the message to send (can be longer than 64 characters)
// - message_type: the type of message to send
// - return: TRUE if the message was sent, FALSE otherwise
pboolean tc_api_send_message(tc_session_t* session, tc_message_type_t message_type, const pchar message[]);

// schedules a world to be sent to the session
// - session: the session to schedule the world to be sent to
// - file_name: the name of the world file to send
// - priority: the priority of the task
// - on_success: the task to schedule on successful transmission of the map
// - on_failure: the task to schedule on failure to transmit the map
// - return: TRUE if the world was scheduled, FALSE otherwise
pboolean tc_api_schedule_send_map(
    tc_session_t* session,
    const pchar* file_name, 
    tc_map_t* pre_loaded_map,
    tc_joinable_interface_t* joinable,
    tc_thread_pool_task_priority_t priority,
    pint session_generation
);

#endif /* TELECLASSIC26_LEVEL_H */