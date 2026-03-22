#ifndef TELECLASSIC26_SESSION_H
#define TELECLASSIC26_SESSION_H

#include <plibsys.h>
#include "thread_pool.h"

#define TC_PROTOCOL_MAX_STR_LEN 64
#define TC_PROTOCOL_TOTAL_PACKETS 32

// NOTE: all packet sizes must be greater than 0
extern const psize tc_protocol_packet_sizes[TC_PROTOCOL_TOTAL_PACKETS];
extern const tc_thread_pool_task_func_t tc_protocol_packet_handlers[TC_PROTOCOL_TOTAL_PACKETS];

pboolean tc_protocol_kick(PSocket* session, const char msg[]);

#endif /* TELECLASSIC26_SESSION_H */