#ifndef TELECLASSIC26_AUTHENTICATION_HEARTBEAT_H
#define TELECLASSIC26_AUTHENTICATION_HEARTBEAT_H

#include <plibsys.h>

#define TC_HEARTBEAT_SALT_LENGTH 16

typedef struct tc_heartbeat_info {
    pint port;
    pint max_players;
    const pchar* server_name;
    pboolean is_public;
    pint protocol_version;

    const pchar* software_name; //Optional, can be NULL
    const pboolean allow_web_play; //Optional, can be FALSE
} tc_heartbeat_info_t;

// Response from the heartbeat server
typedef struct tc_heartbeat_res {
    pchar* web_play_url;
} tc_heartbeat_res_t;

// Sends the server info to the heartbeat server
// - hostname: the hostname of the heartbeat server
// - info: the information about the server to send
// - salt: the salt to use for the heartbeat
// - res: the pointer response from the heartbeat server to write to upon success
// NOTE: res is not valid if FALSE is returned
pboolean tc_heartbeat_send_info(
    const pchar* hostname, 
    const tc_heartbeat_info_t* info,
    const pchar salt[TC_HEARTBEAT_SALT_LENGTH],
    tc_heartbeat_res_t* res
);

// Manager for the heartbeat system
typedef struct heartbeat_manager {
    tc_heartbeat_info_t info;
    pchar current_salt[TC_HEARTBEAT_SALT_LENGTH];

    PUThread* heartbeat_thread;
    PTimeProfiler* heartbeat_profiler;
    PMutex* lock;
} heartbeat_manager_t;

// Initializes the heartbeat manager
void heartbeat_manager_init(heartbeat_manager_t* manager, const tc_heartbeat_info_t info);

// Finalizes the heartbeat manager
void heartbeat_manager_finalize(heartbeat_manager_t* manager);

// Starts the heartbeat manager
void heartbeat_manager_start(heartbeat_manager_t* manager);

pboolean heartbeat_manager_validate(const pchar* username, const pchar* key);

#endif