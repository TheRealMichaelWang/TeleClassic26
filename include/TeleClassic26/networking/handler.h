#ifndef TELECLASSIC26_NETWORKING_HANDLER_H
#define TELECLASSIC26_NETWORKING_HANDLER_H

#include <plibsys.h>
#include <TeleClassic26/gameplay/joinable.h>
#include <TeleClassic26/thread_pool.h>

#define TC_PACKET_HANDLERS_MAX_PACKETS 32

#define TC_PACKET_CPE_EXTINFO 0x10
#define TC_PACKET_CPE_EXTENTRY 0x11

#define TC_PACKET_CPE_CUSTOM_BLOCK_SUPPORT_LEVEL 0x13

#define TC_INBOUND_PACKET_PLAYER_IDENTIFICATION 0x00
#define TC_INBOUND_PACKET_PLAYER_SET_BLOCK 0x05
#define TC_INBOUND_PACKET_POSITION_AND_ORIENTATION_UPDATE 0x08
#define TC_INBOUND_PACKET_MESSAGE 0x0D

extern const tc_thread_pool_task_t tc_packet_handlers[TC_PACKET_HANDLERS_MAX_PACKETS];

// NOTE: packet data size does not include the opcode byte (so its one less than the wiki.vg docs)
psize tc_packet_data_size(pint opcode, tc_session_t* session);

#endif /* TELECLASSIC26_NETWORKING_HANDLER_H */