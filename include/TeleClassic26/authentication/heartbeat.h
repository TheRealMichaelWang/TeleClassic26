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
    pboolean allow_web_play; //Optional, can be FALSE
} tc_heartbeat_info_t;

typedef struct heartbeat_service {
    const pchar* hostname;
    pint port;

    pchar* web_play_url;
} heartbeat_service_t;

// Sends the server info to the heartbeat server
// - hostname: the hostname of the heartbeat server
// - info: the information about the server to send
// - salt: the salt to use for the heartbeat
// - res: the pointer response from the heartbeat server to write to upon success
// NOTE: res is not valid if FALSE is returned
pboolean tc_heartbeat_send_info(
    heartbeat_service_t* service,
    const tc_heartbeat_info_t* info,
    const pchar salt[TC_HEARTBEAT_SALT_LENGTH]
);

// Manager for the heartbeat system
typedef struct heartbeat_manager {
    tc_heartbeat_info_t info;
    pchar current_salt[TC_HEARTBEAT_SALT_LENGTH];

    heartbeat_service_t* services;
    PUThread* heartbeat_thread;
    PMutex* lock;

    pint num_services;
    volatile pboolean shutdown;
} heartbeat_manager_t;

// Generates a random base-62 salt
// - salt: the buffer to write the salt to
void heartbeat_generate_salt(pchar salt[TC_HEARTBEAT_SALT_LENGTH]);

// Initializes the heartbeat manager
// - manager: the heartbeat manager to initialize
// - hostname: the hostname of the heartbeat server
// - host_port: the port of the heartbeat server
// - info: the information about the server to send
// - services: the services to send the heartbeat to
// - num_services: the number of services to send the heartbeat to
// - return: TRUE if the heartbeat manager was initialized, FALSE otherwise
pboolean heartbeat_manager_init(
    heartbeat_manager_t* manager, 
    tc_heartbeat_info_t info,
    heartbeat_service_t* services,
    pint num_services
);

// Finalizes the heartbeat manager
void heartbeat_manager_finalize(heartbeat_manager_t* manager);

// Validates the username with the given key
// - return: the service that the username and key are valid for, NULL if not valid
// - manager: the heartbeat manager to validate the username with
// - username: the username to validate
// - key: the key to validate
heartbeat_service_t* heartbeat_manager_validate(
    heartbeat_manager_t* manager, 
    const pchar* username, 
    const pchar* key
);

#endif