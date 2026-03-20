#ifndef TELECLASSIC26_SERVER_H
#define TELECLASSIC26_SERVER_H

#include <plibsys.h>
#include "thread_pool.h"

#define TC_SERVER_MAX_SESSIONS 128
#define TC_PROTOCOL_MAX_STR_LEN 64

typedef struct {
    pint buffer[TC_SERVER_MAX_SESSIONS];
    psize head_index;
    psize tail_index;
} tc_server_id_buf_t;

typedef struct {
    pchar username[TC_PROTOCOL_MAX_STR_LEN];
    PSocket *client_socket;
    pint id;
} tc_server_session_t;

typedef struct {
    tc_server_session_t session_buffer[TC_SERVER_MAX_SESSIONS];

    tc_server_id_buf_t id_buffer;
    tc_thread_pool_t thread_pool;

    PSocket *listener_socket;
    PMutex *lock;
} tc_server_t;

void tc_server_init(tc_server_t *server, const char* , pint port, pint tcp_backlog);

#endif /* TELECLASSIC26_SERVER_H */