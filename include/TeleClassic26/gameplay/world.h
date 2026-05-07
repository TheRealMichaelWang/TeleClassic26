#ifndef TELECLASSIC26_GAMEPLAY_WORLD_H
#define TELECLASSIC26_GAMEPLAY_WORLD_H

#include <plibsys.h>

// forward declaration of tc_session_t; to avoid circular dependencies
typedef struct tc_session tc_session_t;

typedef struct tc_joinable_interface {
    // the name of the world; used to join world
    const pchar* name;

    // loads the world into the client
    // - return: a pointer to the joinable interface
    // NOTE: the custom return is to allow for routing (i.e. a lobby routes to multiple sub lobbies)
    void* (*attempt_join)(void* this_context, tc_session_t* session, const pchar* world_name);

    // leaves world
    // - call this to leave the world (must be called before the session is destroyed)
    void (*leave)(void* this_context, tc_session_t* session);

    // gets the number of players in the world
    // - return: the number of players in the world
    pint (*num_players)(void* this_context, tc_session_t* session);

    // handles a set block request
    // - see classic protocol docs on wiki.vg for details
    // - return: TRUE if the set block request was handled, FALSE otherwise
    pboolean (*handle_set_block)(void* this_context, tc_session_t* session, pshort x, pshort y, pshort z, pshort block);

    // handles a position and orientation update
    // - see classic protocol docs on wiki.vg for details
    // - return: TRUE if the position and orientation update was handled, FALSE otherwise
    pboolean (*handle_position_update)(void* this_context, tc_session_t* session, pshort x, pshort y, pshort z, pchar heading, pchar pitch);

    // handles a message from the player
    // - message: the message to handle
    // - message_length: the length of the message (not including the null terminator)
    // - return: TRUE if the message was handled, FALSE otherwise
    pboolean (*handle_message)(void* this_context, tc_session_t* session, const pchar* message, pint message_length);

    // handles a sudden stop of the server
    void (*handle_server_stop)(void* this_context);
} tc_joinable_interface_t;

typedef struct tc_join_router {
    PList* joinables;
    PMutex* lock;

    tc_joinable_interface_t* default_joinable;

    pboolean stopped;
} tc_join_router_t;

pboolean tc_join_router_init(tc_join_router_t* router, PList* joinables, const char* default_joinable_name);
void tc_join_router_finalize(tc_join_router_t* router);
void tc_join_router_stop_all(tc_join_router_t* router);

tc_joinable_interface_t* tc_find_joinable(tc_join_router_t* router, const pchar* address);

pboolean tc_session_join(tc_session_t* session, const pchar* address);

#endif /* TELECLASSIC26_GAMEPLAY_WORLD_H */