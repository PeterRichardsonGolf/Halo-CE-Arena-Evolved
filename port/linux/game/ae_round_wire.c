/* The round modes' state on the wire (see the header) */
#include <string.h>

#include "ae_round_rules.h"
#include "ae_round_wire.h"
#include "ae_round_zone_rules.h"

static void put_long(unsigned char *at, long value)
{
	unsigned long bits = (unsigned long)value;

	at[0] = (unsigned char)(bits & 0xFF);
	at[1] = (unsigned char)((bits >> 8) & 0xFF);
	at[2] = (unsigned char)((bits >> 16) & 0xFF);
	at[3] = (unsigned char)((bits >> 24) & 0xFF);
}

static long get_long(unsigned char const *at)
{
	unsigned long bits = (unsigned long)at[0] | ((unsigned long)at[1] << 8) | ((unsigned long)at[2] << 16) |
		((unsigned long)at[3] << 24);

	/* (a 32-bit two's complement value, on a long of any width) */
	if (bits & 0x80000000UL)
		return -(long)((~bits & 0xFFFFFFFFUL) + 1);
	return (long)bits;
}

static void put_short(unsigned char *at, unsigned short value)
{
	at[0] = (unsigned char)(value & 0xFF);
	at[1] = (unsigned char)((value >> 8) & 0xFF);
}

static unsigned short get_short(unsigned char const *at)
{
	return (unsigned short)(at[0] | (at[1] << 8));
}

static short signed_of(unsigned short value)
{
	return value & 0x8000 ? (short)-(int)((~value & 0xFFFF) + 1) : (short)value;
}

static int result_ok(unsigned char result)
{
	return result < AE_ROUND_WIRE_PLAYERS || result == AE_ROUND_WIRE_RESULT_TIE || result == AE_ROUND_WIRE_RESULT_NONE;
}

static int state_ok(struct ae_round_wire const *state)
{
	unsigned seen = 0;
	int index;

	if (state->phase >= AE_ROUND_PHASE_COUNT || (state->flags & ~AE_ROUND_WIRE_FLAGS) ||
		state->zone_phase >= AE_ZONE_PHASE_COUNT || state->count > AE_ROUND_WIRE_PLAYERS ||
		!result_ok(state->last_result))
	{
		return 0;
	}
	for (index = 0; index < state->count; index++)
	{
		struct ae_round_wire_player const *player = &state->players[index];

		if (player->player >= AE_ROUND_WIRE_PLAYERS || (seen & (1u << player->player)) ||
			player->state >= AE_ROUND_PLAYER_STATE_COUNT ||
			(player->reviver >= AE_ROUND_WIRE_PLAYERS && player->reviver != AE_ROUND_WIRE_NO_PLAYER))
		{
			return 0;
		}
		seen |= 1u << player->player;
	}
	return 1;
}

int ae_round_wire_encode(struct ae_round_wire const *state, unsigned char *out, int size)
{
	int needed, index;
	unsigned char *at;

	if (!state || !out || !state_ok(state))
		return 0;
	needed = AE_ROUND_WIRE_HEAD + state->count * AE_ROUND_WIRE_ENTRY + ((state->flags & AE_ROUND_WIRE_EXTRA) ? AE_ROUND_WIRE_TAIL : 0);
	if (size < needed)
		return 0;
	/* (padded to an even length, as the stream carries 16-bit words; the decoder takes the padding byte or none) */
	needed += needed & 1;
	if (size < needed)
		return 0;
	memset(out, 0, (size_t)needed);
	out[0] = AE_ROUND_WIRE_VERSION;
	out[1] = state->phase;
	out[2] = state->round;
	out[3] = state->flags;
	out[4] = state->wins_red;
	out[5] = state->wins_blue;
	out[6] = state->last_result;
	out[7] = state->zone_phase;
	out[8] = state->max_rounds;
	out[9] = state->to_win;
	put_long(out + 10, state->round_start);
	put_long(out + 14, state->live_start);
	put_long(out + 18, state->post_end);
	put_short(out + 22, (unsigned short)state->zone_centre[0]);
	put_short(out + 24, (unsigned short)state->zone_centre[1]);
	put_short(out + 26, (unsigned short)state->zone_centre[2]);
	put_short(out + 28, state->zone_radius);
	out[30] = state->zone_wait_seconds;
	out[31] = state->zone_closing_seconds;
	out[32] = state->zone_closing_2_seconds;
	put_long(out + 33, state->zone_start);
	out[37] = state->count;
	for (index = 0; index < state->count; index++)
	{
		struct ae_round_wire_player const *player = &state->players[index];

		at = out + AE_ROUND_WIRE_HEAD + index * AE_ROUND_WIRE_ENTRY;
		at[0] = player->player;
		at[1] = player->state;
		at[2] = player->revives;
		at[3] = player->progress;
		at[4] = player->reviver;
		at[5] = player->wins;
		put_short(at + 6, (unsigned short)player->spot[0]);
		put_short(at + 8, (unsigned short)player->spot[1]);
		put_short(at + 10, (unsigned short)player->spot[2]);
		at[12] = player->bleed;
	}
	if (state->flags & AE_ROUND_WIRE_EXTRA)
	{
		at = out + AE_ROUND_WIRE_HEAD + state->count * AE_ROUND_WIRE_ENTRY;
		at[0] = state->pool_red;
		at[1] = state->pool_blue;
		at[2] = state->kills_red;
		at[3] = state->kills_blue;
		at[4] = state->zone_wait_2_seconds;
	}
	return needed;
}

int ae_round_wire_decode(unsigned char const *in, int size, struct ae_round_wire *state)
{
	struct ae_round_wire read;
	int index;

	if (!in || !state || size < AE_ROUND_WIRE_HEAD || in[0] != AE_ROUND_WIRE_VERSION)
		return 0;
	{
		int base = AE_ROUND_WIRE_HEAD + in[37] * AE_ROUND_WIRE_ENTRY + ((in[3] & AE_ROUND_WIRE_EXTRA) ? AE_ROUND_WIRE_TAIL : 0);

		if (in[37] > AE_ROUND_WIRE_PLAYERS || (size != base && size != base + 1))
			return 0;
	}
	memset(&read, 0, sizeof(read));
	read.phase = in[1];
	read.round = in[2];
	read.flags = in[3];
	read.wins_red = in[4];
	read.wins_blue = in[5];
	read.last_result = in[6];
	read.zone_phase = in[7];
	read.max_rounds = in[8];
	read.to_win = in[9];
	read.round_start = get_long(in + 10);
	read.live_start = get_long(in + 14);
	read.post_end = get_long(in + 18);
	read.zone_centre[0] = signed_of(get_short(in + 22));
	read.zone_centre[1] = signed_of(get_short(in + 24));
	read.zone_centre[2] = signed_of(get_short(in + 26));
	read.zone_radius = get_short(in + 28);
	read.zone_wait_seconds = in[30];
	read.zone_closing_seconds = in[31];
	read.zone_closing_2_seconds = in[32];
	read.zone_start = get_long(in + 33);
	read.count = in[37];
	for (index = 0; index < read.count; index++)
	{
		unsigned char const *at = in + AE_ROUND_WIRE_HEAD + index * AE_ROUND_WIRE_ENTRY;
		struct ae_round_wire_player *player = &read.players[index];

		player->player = at[0];
		player->state = at[1];
		player->revives = at[2];
		player->progress = at[3];
		player->reviver = at[4];
		player->wins = at[5];
		player->spot[0] = signed_of(get_short(at + 6));
		player->spot[1] = signed_of(get_short(at + 8));
		player->spot[2] = signed_of(get_short(at + 10));
		player->bleed = at[12];
	}
	if (read.flags & AE_ROUND_WIRE_EXTRA)
	{
		unsigned char const *at = in + AE_ROUND_WIRE_HEAD + read.count * AE_ROUND_WIRE_ENTRY;

		read.pool_red = at[0];
		read.pool_blue = at[1];
		read.kills_red = at[2];
		read.kills_blue = at[3];
		read.zone_wait_2_seconds = at[4];
	}
	if (!state_ok(&read))
		return 0;
	*state = read;
	return 1;
}

short ae_round_wire_coordinate(real value)
{
	real scaled = value * 16.0f;

	if (!(scaled == scaled))
		return 0;
	if (scaled > 32767.0f)
		return 32767;
	if (scaled < -32768.0f)
		return -32768;
	return (short)(scaled < 0.0f ? scaled - 0.5f : scaled + 0.5f);
}

real ae_round_wire_real(short value)
{
	return (real)value / 16.0f;
}

unsigned short ae_round_wire_radius(real value)
{
	real scaled = value * 16.0f;

	if (!(scaled > 0.0f))
		return 0;
	if (scaled > 65535.0f)
		return 65535;
	return (unsigned short)(scaled + 0.5f);
}
