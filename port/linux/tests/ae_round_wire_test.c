#include <stdio.h>
#include <string.h>
#include "ae_round_rules.h"
#include "ae_round_wire.h"
#include "ae_round_zone_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void sample(struct ae_round_wire *state)
{
	memset(state, 0, sizeof(*state));
	state->phase = AE_ROUND_PHASE_LIVE;
	state->round = 5;
	state->flags = AE_ROUND_WIRE_TEAMS | AE_ROUND_WIRE_SWITCHED | AE_ROUND_WIRE_ZONE | AE_ROUND_WIRE_REVIVE;
	state->wins_red = 2;
	state->wins_blue = 2;
	state->last_result = AE_ROUND_WIRE_RESULT_TIE;
	state->zone_phase = AE_ZONE_PHASE_CLOSING;
	state->max_rounds = 9;
	state->to_win = 5;
	state->round_start = 123456;
	state->live_start = 123606;
	state->post_end = 0;
	state->zone_start = -7;
	state->zone_centre[0] = ae_round_wire_coordinate(-40.25f);
	state->zone_centre[1] = ae_round_wire_coordinate(100.0f);
	state->zone_centre[2] = ae_round_wire_coordinate(-2.5f);
	state->zone_radius = ae_round_wire_radius(55.5f);
	state->zone_wait_seconds = 60;
	state->zone_closing_seconds = 48;
	state->zone_closing_2_seconds = 30;
	state->count = 2;
	state->players[0].player = 0;
	state->players[0].state = AE_ROUND_PLAYER_ALIVE;
	state->players[0].reviver = AE_ROUND_WIRE_NO_PLAYER;
	state->players[1].player = 9;
	state->players[1].state = AE_ROUND_PLAYER_DOWN;
	state->players[1].revives = 1;
	state->players[1].progress = 128;
	state->players[1].reviver = 0;
	state->players[1].spot[0] = ae_round_wire_coordinate(12.0f);
	state->players[1].spot[1] = ae_round_wire_coordinate(-3.0625f);
	state->players[1].spot[2] = ae_round_wire_coordinate(0.5f);
	state->players[1].bleed = 17;
}

int main(void)
{
	struct ae_round_wire state, read, kept;
	unsigned char bytes[AE_ROUND_WIRE_MAXIMUM + 8];
	int size, index;

	/* a whole state, out and back */
	sample(&state);
	size = ae_round_wire_encode(&state, bytes, sizeof(bytes));
	CHECK(size == AE_ROUND_WIRE_HEAD + 2 * AE_ROUND_WIRE_ENTRY);
	CHECK(ae_round_wire_decode(bytes, size, &read));
	CHECK(!memcmp(&read, &state, sizeof(state)));
	CHECK(read.zone_start == -7 && read.round_start == 123456);
	CHECK(read.players[1].bleed == 17 && read.players[0].bleed == 0);
	CHECK(ae_round_wire_real(read.zone_centre[0]) == -40.25f && ae_round_wire_real(read.players[1].spot[1]) == -3.0625f);
	CHECK(read.zone_radius == 888);

	/* the most players fit the most bytes */
	state.count = AE_ROUND_WIRE_PLAYERS;
	for (index = 0; index < AE_ROUND_WIRE_PLAYERS; index++)
	{
		state.players[index].player = (unsigned char)index;
		state.players[index].state = AE_ROUND_PLAYER_WAITING;
		state.players[index].reviver = AE_ROUND_WIRE_NO_PLAYER;
	}
	CHECK(ae_round_wire_encode(&state, bytes, sizeof(bytes)) == AE_ROUND_WIRE_MAXIMUM);
	CHECK(ae_round_wire_encode(&state, bytes, AE_ROUND_WIRE_MAXIMUM - 1) == 0);
	CHECK(ae_round_wire_decode(bytes, AE_ROUND_WIRE_MAXIMUM, &read) && read.count == AE_ROUND_WIRE_PLAYERS);

	/* a state out of range is not written */
	sample(&state);
	state.phase = AE_ROUND_PHASE_COUNT;
	CHECK(ae_round_wire_encode(&state, bytes, sizeof(bytes)) == 0);
	sample(&state);
	state.players[1].player = 0;
	CHECK(ae_round_wire_encode(&state, bytes, sizeof(bytes)) == 0);

	/* an odd count: padded to an even length, read with or without the padding byte; one entry more or fewer refused */
	state.count = 1;
	size = ae_round_wire_encode(&state, bytes, sizeof(bytes));
	CHECK(size == AE_ROUND_WIRE_HEAD + AE_ROUND_WIRE_ENTRY + 1 && size % 2 == 0);
	CHECK(ae_round_wire_decode(bytes, size, &read) && read.count == 1);
	CHECK(ae_round_wire_decode(bytes, size - 1, &read) && read.count == 1);
	CHECK(!ae_round_wire_decode(bytes, size + 1, &read));
	sample(&state);

	/* untrusted bytes: each fault refused, the state kept as it was */
	sample(&state);
	size = ae_round_wire_encode(&state, bytes, sizeof(bytes));
	memset(&kept, 0x5A, sizeof(kept));
	read = kept;
	CHECK(!ae_round_wire_decode(bytes, size - 1, &read) && !memcmp(&read, &kept, sizeof(kept)));
	CHECK(!ae_round_wire_decode(bytes, size + 2, &read));
	CHECK(!ae_round_wire_decode(bytes, AE_ROUND_WIRE_HEAD - 1, &read));
	CHECK(!ae_round_wire_decode(NULL, size, &read));
	{
		static const struct { int offset; unsigned char value; } faults[] =
		{
			{ 0, 2 },                                    /* another version */
			{ 1, AE_ROUND_PHASE_COUNT },                 /* phase */
			{ 3, 0x10 },                                 /* an unknown flag */
			{ 6, 16 },                                   /* a result of no player */
			{ 7, AE_ZONE_PHASE_COUNT },                  /* zone phase */
			{ 37, 3 },                                   /* a count the size does not hold */
			{ 37, 200 },                                 /* past the most players */
			{ AE_ROUND_WIRE_HEAD + 0, 16 },              /* a player past the table */
			{ AE_ROUND_WIRE_HEAD + 1, AE_ROUND_PLAYER_STATE_COUNT }, /* a state */
			{ AE_ROUND_WIRE_HEAD + 4, 40 },              /* a reviver of no player */
			{ AE_ROUND_WIRE_HEAD + AE_ROUND_WIRE_ENTRY, 0 }, /* a player twice */
		};
		unsigned char bad[sizeof(bytes)];
		size_t fault;

		for (fault = 0; fault < sizeof(faults) / sizeof(faults[0]); fault++)
		{
			memcpy(bad, bytes, (size_t)size);
			bad[faults[fault].offset] = faults[fault].value;
			read = kept;
			CHECK(!ae_round_wire_decode(bad, size, &read));
			CHECK(!memcmp(&read, &kept, sizeof(kept)));
		}
	}
	/* (garbage of every size: never read past the bytes given, never taken) */
	{
		unsigned char noise[AE_ROUND_WIRE_MAXIMUM];
		unsigned seed = 7;
		int length, taken = 0;

		for (length = 0; length <= AE_ROUND_WIRE_MAXIMUM; length++)
		{
			for (index = 0; index < length; index++)
			{
				seed = seed * 1103515245u + 12345u;
				noise[index] = (unsigned char)(seed >> 16);
			}
			taken += ae_round_wire_decode(noise, length, &read);
		}
		CHECK(taken == 0);
	}

	/* coordinates: sixteenths, held to a short's range; radii to an unsigned short's */
	CHECK(ae_round_wire_coordinate(1.0f) == 16 && ae_round_wire_coordinate(-1.0f) == -16);
	CHECK(ae_round_wire_coordinate(1e9f) == 32767 && ae_round_wire_coordinate(-1e9f) == -32768);
	CHECK(ae_round_wire_radius(-3.0f) == 0 && ae_round_wire_radius(1e9f) == 65535);


	/* the respawn modes' tail: pools, kills and the zone's second wait ride after the entries; ELIMINATION's body stays as it was */
	{
		struct ae_round_wire tail;
		unsigned char buffer[AE_ROUND_WIRE_BUFFER];
		int length;

		memset(&tail, 0, sizeof(tail));
		tail.phase = AE_ROUND_PHASE_LIVE;
		tail.round = 2;
		tail.flags = AE_ROUND_WIRE_TEAMS | AE_ROUND_WIRE_EXTRA;
		tail.count = 3;
		tail.players[0].player = 0, tail.players[1].player = 1, tail.players[2].player = 2;
		tail.players[0].state = tail.players[1].state = tail.players[2].state = AE_ROUND_PLAYER_ALIVE;
		tail.players[0].reviver = tail.players[1].reviver = tail.players[2].reviver = AE_ROUND_WIRE_NO_PLAYER;
		tail.pool_red = 5, tail.pool_blue = 8, tail.kills_red = 3, tail.kills_blue = 9, tail.zone_wait_2_seconds = 0;
		length = ae_round_wire_encode(&tail, buffer, sizeof(buffer));
		CHECK(length == ((AE_ROUND_WIRE_HEAD + 3 * AE_ROUND_WIRE_ENTRY + AE_ROUND_WIRE_TAIL + 1) & ~1));
		CHECK(ae_round_wire_decode(buffer, length, &read) && read.pool_red == 5 && read.pool_blue == 8 && read.kills_red == 3 &&
			read.kills_blue == 9 && read.zone_wait_2_seconds == 0 && read.count == 3);
		CHECK(!ae_round_wire_decode(buffer, AE_ROUND_WIRE_HEAD + 3 * AE_ROUND_WIRE_ENTRY, &read));
		tail.flags = AE_ROUND_WIRE_TEAMS;
		length = ae_round_wire_encode(&tail, buffer, sizeof(buffer));
		CHECK(length == ((AE_ROUND_WIRE_HEAD + 3 * AE_ROUND_WIRE_ENTRY + 1) & ~1));
	}

	if (failures)
		return 1;
	printf("ae_round_wire_test: ok\n");
	return 0;
}
