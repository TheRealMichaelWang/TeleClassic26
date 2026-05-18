#ifndef TELECLASSIC26_GAMEPLAY_POSITIONS_H
#define TELECLASSIC26_GAMEPLAY_POSITIONS_H

#include <plibsys.h>

typedef struct player_position { 
    pint32 x;
    pint32 y;
    pint32 z;
    pchar heading;
    pchar pitch;
} player_position_t;

typedef struct block_position {
    pint16 x;
    pint16 y;
    pint16 z;
} block_position_t;

typedef struct spawn_position {
    block_position_t block_position;
    pchar heading;
    pchar pitch;
} spawn_position_t;

static inline player_position_t tc_spawn_position_to_player_position(spawn_position_t spawn_position) {
    player_position_t player_position;
    player_position.x = (spawn_position.block_position.x << 5) + 16;
    player_position.y = (spawn_position.block_position.y << 5) + 51;
    player_position.z = (spawn_position.block_position.z << 5) + 16;
    player_position.heading = spawn_position.heading;
    player_position.pitch = spawn_position.pitch;
    return player_position;
}

#endif /* TELECLASSIC26_GAMEPLAY_POSITIONS_H */