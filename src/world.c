#ifndef TELECLASSIC26_WORLD_C
#define TELECLASSIC26_WORLD_C

#include "plibsys.h"
#include <string.h>
#include <TeleClassic26/gameplay/world.h>
#include <TeleClassic26/networking/server.h>
#include <TeleClassic26/networking/protocol.h>
#include <TeleClassic26/utils.h>
#include <TeleClassic26/log.h>

pboolean tc_join_router_init(tc_join_router_t* router, PList* joinables, const char* default_joinable_name) {
    router->joinables = joinables;
    router->lock = p_mutex_new();
    router->stopped = FALSE;
    router->default_joinable = NULL;
    if (router->lock == NULL) {
        return FALSE;
    }

    router->default_joinable = tc_find_joinable(router, default_joinable_name);

    return TRUE;
}

void tc_join_router_finalize(tc_join_router_t* router) {
    if (!router->stopped) {
        tc_join_router_stop_all(router);
    }
    p_list_free(router->joinables);
    p_mutex_free(router->lock);
}

static void stop_joinable(void* ppointer, void* userdata) {
    tc_joinable_interface_t* joinable_interface = (tc_joinable_interface_t*)ppointer;
    joinable_interface->handle_server_stop(joinable_interface);
}

void tc_join_router_stop_all(tc_join_router_t* router) {
    p_mutex_lock(router->lock);

    if (router->stopped) {
        return;
    }
    router->stopped = TRUE;

    p_list_foreach(router->joinables, stop_joinable, NULL);
    p_mutex_unlock(router->lock);
}

typedef struct find_joinable_userdata {
    const pchar* address;
    tc_joinable_interface_t* best_match;
} find_joinable_userdata_t;

static void find_joinable_callback(void* ppointer, void* userdata) {
    tc_joinable_interface_t* joinable_interface = (tc_joinable_interface_t*)ppointer;
    find_joinable_userdata_t* parameters = (find_joinable_userdata_t*)userdata;

    if (parameters->best_match) {
        return;
    }

    if (strcmp(joinable_interface->name, parameters->address) == 0) {
        parameters->best_match = joinable_interface;
        return;
    }
}

tc_joinable_interface_t* tc_find_joinable(tc_join_router_t* router, const pchar* address) {
    if (address == NULL || strcmp(address, "default") == 0 || strcmp(address, "") == 0) {
        return router->default_joinable;
    }

    p_mutex_lock(router->lock);
    if (router->stopped) {
        return NULL;
    }

    find_joinable_userdata_t userdata = {
        .address = address,
        .best_match = NULL
    };
    p_list_foreach(router->joinables, find_joinable_callback, &userdata);

    p_mutex_unlock(router->lock);
    return userdata.best_match;
}

pboolean tc_session_join(tc_session_t* session, const pchar* address) {
    tc_joinable_interface_t* joinable = tc_find_joinable(&session->server->join_router, address);
    if (!joinable) {
        TC_LOG_SESSION(log_error, session, "Failed to find %s that player can join. ", address);
        
        //send a msg indicating that the player can't join the world
        return TRUE;
    }

    if (session->current_joinable) { // leave old joinable if applicable
        session->current_joinable->leave(session->current_joinable, session);
    }

    if (!joinable->attempt_join(joinable, session, address)) {
        TC_LOG_SESSION(log_error, session, "Failed to join %s.", address);
        
        //send a msg indicating that the player can't join the world or may not be allowed to join the world
        
        tc_joinable_interface_t* default_joinable = session->server->join_router.default_joinable;
        if (!default_joinable->attempt_join(default_joinable, session, address)) {
            TC_LOG_SESSION(log_error, session, "Failed to reroute playerto default/lobby.", address);
            tc_server_kick_session(session, "Failed to reroute you to default/lobby after failing to join a world.");
            return FALSE;
        }
        session->current_joinable = default_joinable;
        return TRUE;
    }

    session->current_joinable = joinable;
    return TRUE;
}

#endif /* TELECLASSIC26_WORLD_C */