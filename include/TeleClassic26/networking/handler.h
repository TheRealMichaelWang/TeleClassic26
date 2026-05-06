#ifndef TELECLASSIC26_NETWORKING_HANDLER_H
#define TELECLASSIC26_NETWORKING_HANDLER_H

#include <plibsys.h>
#include <TeleClassic26/thread_pool.h>

#define TC_PACKET_HANDLERS_MAX_PACKETS 32

#define TC_PACKET_CPE_EXTINFO 0x10
#define TC_PACKET_CPE_EXTENTRY 0x11

#define TC_PACKET_CPE_CUSTOM_BLOCK_SUPPORT_LEVEL 0x13

#define TC_PACKET_PLAYER_IDENTIFICATION 0x00

// NOTE: all packet sizes must be greater than 0
extern const psize tc_packet_data_sizes[TC_PACKET_HANDLERS_MAX_PACKETS];
extern const tc_thread_pool_task_t tc_packet_handlers[TC_PACKET_HANDLERS_MAX_PACKETS];

#endif /* TELECLASSIC26_NETWORKING_HANDLER_H */