#ifndef TELECLASSIC26_SESSION_H
#define TELECLASSIC26_SESSION_H

#include <plibsys.h>
#include <TeleClassic26/thread_pool.h>

#define TC_PROTOCOL_MAX_STR_LEN 64
#define TC_PROTOCOL_TOTAL_PACKETS 32
#define TC_PROTOCOL_VERSION 0x07

// NOTE: all packet sizes must be greater than 0
extern const psize tc_protocol_packet_sizes[TC_PROTOCOL_TOTAL_PACKETS];
extern const tc_thread_pool_task_t tc_protocol_packet_handlers[TC_PROTOCOL_TOTAL_PACKETS];

pboolean tc_protocol_server_identification(PSocket* session, const pchar server_name[], const pchar motd[], pchar user_type);
pboolean tc_protocol_kick(PSocket* session, const pchar msg[]);

#endif /* TELECLASSIC26_SESSION_H */