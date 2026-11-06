/*
AE_ROUND_WIRE.H

The round modes' state on the wire: the body of distributed message 106 (_distributed_message_ae_round,
ae_round_net.c), which the host sends every client reliably when it changes and every two seconds. A build that does
not know the kind drops it. Little-endian bytes, written and read field by field; what another machine sends is
untrusted, so the reader takes only a body of exactly the size its count gives, with every field in range. Pure: no
engine includes; unit-tested (port/linux/tests/ae_round_wire_test.c).

The body (AE_ROUND_WIRE_HEAD bytes, then count entries of AE_ROUND_WIRE_ENTRY):

	 0 version (1)          1 phase (enum ae_round_phase)   2 round (1 up)        3 flags (AE_ROUND_WIRE_*)
	 4 wins red             5 wins blue                     6 last result (a side, 254 tie, 255 none)
	 7 zone phase (enum ae_zone_phase)                      8 max rounds          9 wins to win (0: all played)
	10 round start (tick, 4 bytes)   14 LIVE start (tick, 4)   18 POST end (tick, 4; 0 none)
	22 zone centre x, y, z (3 x 2 bytes, signed, sixteenths of a world unit)
	28 zone start radius (2 bytes, sixteenths)   30 zone wait s   31 first closing s   32 second closing s
	33 zone start (tick, 4 bytes: the LIVE start, or a dev command's)
	37 count (0-16)
	then per player: 0 player (absolute index, 0-15)   1 state (enum ae_round_player_state)   2 revives this round
	                 3 revive progress (0-255)   4 reviver (absolute index, 255 none)   5 wins (no teams)
	                 6 spot x, y, z (3 x 2 bytes, sixteenths)   12 bleed-out seconds left (0 none: off, or not down)
*/
#ifndef AE_ROUND_WIRE_H
#define AE_ROUND_WIRE_H

#include "ae_pure_types.h"

enum
{
	AE_ROUND_WIRE_VERSION = 1,
	AE_ROUND_WIRE_PLAYERS = 16,
	AE_ROUND_WIRE_HEAD = 38,
	AE_ROUND_WIRE_ENTRY = 13,
	AE_ROUND_WIRE_MAXIMUM = AE_ROUND_WIRE_HEAD + AE_ROUND_WIRE_PLAYERS * AE_ROUND_WIRE_ENTRY,
	AE_ROUND_WIRE_NO_PLAYER = 255,
	AE_ROUND_WIRE_RESULT_NONE = 255,
	AE_ROUND_WIRE_RESULT_TIE = 254,

	/* flags */
	AE_ROUND_WIRE_TEAMS = 0x01,
	AE_ROUND_WIRE_SWITCHED = 0x02,
	AE_ROUND_WIRE_ZONE = 0x04,
	AE_ROUND_WIRE_REVIVE = 0x08,
	AE_ROUND_WIRE_FLAGS = 0x0F,
};

struct ae_round_wire_player
{
	unsigned char player;   /* absolute index */
	unsigned char state;    /* enum ae_round_player_state */
	unsigned char revives;
	unsigned char progress; /* 0-255 of the revive */
	unsigned char reviver;  /* absolute index, AE_ROUND_WIRE_NO_PLAYER none */
	unsigned char wins;
	short spot[3];          /* sixteenths of a world unit */
	unsigned char bleed;    /* BLEEDOUT: seconds the downed player's marker has left, 0 none */
};

struct ae_round_wire
{
	unsigned char phase, round, flags, wins_red, wins_blue, last_result, zone_phase, max_rounds, to_win;
	long round_start, live_start, post_end, zone_start;
	short zone_centre[3];
	unsigned short zone_radius;
	unsigned char zone_wait_seconds, zone_closing_seconds, zone_closing_2_seconds;
	unsigned char count;
	struct ae_round_wire_player players[AE_ROUND_WIRE_PLAYERS];
};

/* the bytes of the body: their count, 0 when the state is out of range or size is too small */
int ae_round_wire_encode(struct ae_round_wire const *state, unsigned char *out, int size);
/* a received body read into *state: 1 when it is whole and every field in range, else 0 (*state then unchanged) */
int ae_round_wire_decode(unsigned char const *in, int size, struct ae_round_wire *state);

/* a world unit coordinate in sixteenths, held to a short's range; and back */
short ae_round_wire_coordinate(real value);
real ae_round_wire_real(short value);
/* a radius in sixteenths, held to an unsigned short's range */
unsigned short ae_round_wire_radius(real value);

#endif
