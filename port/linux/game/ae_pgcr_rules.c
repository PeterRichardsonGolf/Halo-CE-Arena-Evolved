/* The post-game report's numbers (see the header). */
#include <string.h>

#include "ae_pgcr_rules.h"

static int slot_ok(int slot)
{
	return slot >= 0 && slot < AE_PGCR_PLAYERS;
}

void ae_pgcr_reset(struct ae_pgcr_game *game)
{
	memset(game, 0, sizeof(*game));
}

long ae_pgcr_shot(struct ae_pgcr_game *game, int slot)
{
	if (!slot_ok(slot))
		return 0;
	return ++game->shots_fired[slot];
}

int ae_pgcr_category_is_gun(int category)
{
	switch (category)
	{
	case AE_MEDAL_CATEGORY_BULLET:
	case AE_MEDAL_CATEGORY_SNIPER:
	case AE_MEDAL_CATEGORY_SHOTGUN:
	case AE_MEDAL_CATEGORY_NEEDLE:
	case AE_MEDAL_CATEGORY_PLASMA:
	case AE_MEDAL_CATEGORY_FLAME:
	case AE_MEDAL_CATEGORY_HIGH_EXPLOSIVE:
		return 1;
	}
	return 0;
}

int ae_pgcr_hit(struct ae_pgcr_game *game, int attacker, int victim, int category, long tick)
{
	if (!slot_ok(attacker) || !slot_ok(victim) || attacker == victim || !ae_pgcr_category_is_gun(category))
		return 0;
	/* (one hit a tick: a shell's pellets, a round and its splash) */
	if (game->shots_hit[attacker] > 0 && game->last_hit_tick[attacker] == tick)
		return 0;
	if (game->shots_hit[attacker] >= game->shots_fired[attacker])
		return 0;
	game->last_hit_tick[attacker] = tick;
	game->shots_hit[attacker]++;
	return 1;
}

void ae_pgcr_medal(struct ae_pgcr_game *game, int slot, int medal)
{
	if (slot_ok(slot) && medal >= 0 && medal < AE_MEDAL_COUNT && game->medals[slot][medal] < 255)
		game->medals[slot][medal]++;
}

void ae_pgcr_death(struct ae_pgcr_game *game, int victim)
{
	if (slot_ok(victim))
	{
		game->spree[victim] = 0;
		game->chain[victim] = 0;
	}
}

void ae_pgcr_kill(struct ae_pgcr_game *game, int killer, int victim, int headshot, long tag, long tick)
{
	int index;

	if (!slot_ok(killer) || !slot_ok(victim) || killer == victim)
		return;
	if (game->kills[killer][victim] < 255)
		game->kills[killer][victim]++;
	if (headshot && game->headshots[killer] < 65535)
		game->headshots[killer]++;
	if (game->spree[killer] < 65535)
		game->spree[killer]++;
	if (game->spree[killer] > game->best_spree[killer])
		game->best_spree[killer] = game->spree[killer];
	if (game->chain[killer] > 0 && tick - game->last_kill_tick[killer] <= AE_PGCR_MULTIKILL_TICKS)
		game->chain[killer]++;
	else
		game->chain[killer] = 1;
	game->last_kill_tick[killer] = tick;
	if (game->chain[killer] >= 2 && game->multikills[killer] < 65535)
		game->multikills[killer]++;
	if (tag == -1)
		return;
	for (index = 0; index < game->weapon_count[killer]; index++)
	{
		if (game->weapons[killer][index].tag == tag)
			break;
	}
	if (index == game->weapon_count[killer])
	{
		if (index == AE_PGCR_TRACKED_WEAPONS)
			return;
		game->weapons[killer][index].tag = tag;
		game->weapons[killer][index].kills = 0;
		game->weapon_count[killer]++;
	}
	if (game->weapons[killer][index].kills < 65535)
		game->weapons[killer][index].kills++;
}

void ae_pgcr_damage(struct ae_pgcr_game *game, int attacker, int victim, float share, int friendly)
{
	float points = share * 100.0f;

	if (!slot_ok(victim) || !(points > 0.0f))
		return;
	game->damage_taken[victim] += points;
	if (slot_ok(attacker) && attacker != victim && !friendly)
		game->damage_dealt[attacker] += points;
}

/* (the rocket, sniper, fuel rod, sword and hammer: ae_medal_rules_role_is_power's list, kept here so the rules stand alone) */
static int role_is_power(int role)
{
	return role == AE_MEDAL_ROLE_SNIPER || role == AE_MEDAL_ROLE_ROCKET || role == AE_MEDAL_ROLE_FUEL_ROD ||
		role == AE_MEDAL_ROLE_SWORD || role == AE_MEDAL_ROLE_HAMMER;
}

int ae_pgcr_kill_kinds_of(struct ae_medal_kill const *kill)
{
	int kinds = 0, grenade, melee = kill->category == AE_MEDAL_CATEGORY_MELEE;

	grenade = kill->category == AE_MEDAL_CATEGORY_GRENADE || kill->weapon == AE_MEDAL_WEAPON_FRAG_GRENADE ||
		kill->weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE || kill->weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK;
	if (melee)
		kinds |= AE_PGCR_KIND_MELEE;
	if (melee && kill->from_behind)
		kinds |= AE_PGCR_KIND_ASSASSINATION;
	if (grenade)
		kinds |= AE_PGCR_KIND_GRENADE;
	if (kill->weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK)
		kinds |= AE_PGCR_KIND_STICK;
	if (kill->category == AE_MEDAL_CATEGORY_VEHICLE)
		kinds |= AE_PGCR_KIND_SPLATTER;
	/* (a power weapon's kill: not a bash with it, not a grenade or a splat; the sword's own blow is its kill) */
	if (role_is_power(kill->killer_role) && !grenade && kill->category != AE_MEDAL_CATEGORY_VEHICLE &&
		(!melee || kill->killer_role == AE_MEDAL_ROLE_SWORD || kill->killer_role == AE_MEDAL_ROLE_HAMMER))
		kinds |= AE_PGCR_KIND_POWER;
	return kinds;
}

static void bump(unsigned short *count)
{
	if (*count < 65535)
		++*count;
}

void ae_pgcr_kill_kinds(struct ae_pgcr_game *game, struct ae_medal_kill const *kill)
{
	int kinds, killer = kill->killer;

	if (!slot_ok(killer) || !slot_ok(kill->victim) || killer == kill->victim || kill->betrayal)
		return;
	kinds = ae_pgcr_kill_kinds_of(kill);
	if (kinds & AE_PGCR_KIND_MELEE)
		bump(&game->kind[killer][0]);
	if (kinds & AE_PGCR_KIND_POWER)
		bump(&game->kind[killer][1]);
	if (kinds & AE_PGCR_KIND_GRENADE)
		bump(&game->kind[killer][2]);
	if (kinds & AE_PGCR_KIND_ASSASSINATION)
		bump(&game->kind[killer][3]);
	if (kinds & AE_PGCR_KIND_SPLATTER)
		bump(&game->kind[killer][4]);
	if (kinds & AE_PGCR_KIND_STICK)
		bump(&game->kind[killer][5]);
	if (kill->killer_carried_flag)
		bump(&game->as_carrier[killer]);
}

void ae_pgcr_carrier_tick(struct ae_pgcr_game *game, int slot)
{
	if (slot_ok(slot) && game->carrier_ticks[slot] < 2000000000L)
		game->carrier_ticks[slot]++;
}

void ae_pgcr_bomb(struct ae_pgcr_game *game, int slot, int what)
{
	if (!slot_ok(slot))
		return;
	if (what == AE_PGCR_BOMB_PLANT && game->plants[slot] < 255)
		game->plants[slot]++;
	else if (what == AE_PGCR_BOMB_DEFUSE && game->defuses[slot] < 255)
		game->defuses[slot]++;
}

static unsigned long points_whole(float points)
{
	return points > 0.0f ? (unsigned long)(points + 0.5f) : 0;
}

void ae_pgcr_summarize(struct ae_pgcr_game const *game, int slot, struct ae_pgcr_summary *summary)
{
	int index, other, best, taken[AE_PGCR_TRACKED_WEAPONS];

	memset(summary, 0, sizeof(*summary));
	summary->killed_most = AE_PGCR_NO_PLAYER;
	summary->killed_by_most = AE_PGCR_NO_PLAYER;
	if (!slot_ok(slot))
		return;
	summary->valid = 1;
	summary->slot = slot;
	summary->best_spree = game->best_spree[slot];
	summary->multikills = game->multikills[slot];
	summary->headshots = game->headshots[slot];
	summary->extended = 1;
	summary->damage_dealt = points_whole(game->damage_dealt[slot]);
	summary->damage_taken = points_whole(game->damage_taken[slot]);
	summary->melee = game->kind[slot][0];
	summary->power = game->kind[slot][1];
	summary->grenade = game->kind[slot][2];
	summary->assassination = game->kind[slot][3];
	summary->splatter = game->kind[slot][4];
	summary->stick = game->kind[slot][5];
	summary->kills_as_flag_carrier = game->as_carrier[slot];
	summary->flag_carrier_seconds = (unsigned short)(game->carrier_ticks[slot] / AE_PGCR_TICKS_PER_SECOND > 65535 ? 65535 :
		game->carrier_ticks[slot] / AE_PGCR_TICKS_PER_SECOND);
	summary->bomb_plants = game->plants[slot];
	summary->bomb_defuses = game->defuses[slot];
	/* (the first of equals wins: the lowest slot) */
	for (other = 0; other < AE_PGCR_PLAYERS; other++)
	{
		if (game->kills[slot][other] > summary->killed_most_count)
		{
			summary->killed_most = (unsigned char)other;
			summary->killed_most_count = game->kills[slot][other];
		}
		if (game->kills[other][slot] > summary->killed_by_most_count)
		{
			summary->killed_by_most = (unsigned char)other;
			summary->killed_by_most_count = game->kills[other][slot];
		}
	}
	/* the players they traded the most kills with (kills of and by, together): most first, the lower slot of equals first */
	{
		int used[AE_PGCR_PLAYERS];

		memset(used, 0, sizeof(used));
		while (summary->versus_count < AE_PGCR_SUMMARY_VERSUS)
		{
			best = -1;
			for (other = 0; other < AE_PGCR_PLAYERS; other++)
			{
				int total = game->kills[slot][other] + game->kills[other][slot];

				if (other != slot && !used[other] && total > 0 &&
					(best < 0 || total > game->kills[slot][best] + game->kills[best][slot]))
				{
					best = other;
				}
			}
			if (best < 0)
				break;
			used[best] = 1;
			summary->versus[summary->versus_count].slot = (unsigned char)best;
			summary->versus[summary->versus_count].killed = game->kills[slot][best];
			summary->versus[summary->versus_count].killed_by = game->kills[best][slot];
			summary->versus_count++;
		}
	}
	/* the deadliest weapons: most kills first, the earlier of equals first */
	memset(taken, 0, sizeof(taken));
	while (summary->weapon_count < AE_PGCR_SUMMARY_WEAPONS)
	{
		best = -1;
		for (index = 0; index < game->weapon_count[slot]; index++)
		{
			if (!taken[index] && game->weapons[slot][index].kills > 0 &&
				(best < 0 || game->weapons[slot][index].kills > game->weapons[slot][best].kills))
			{
				best = index;
			}
		}
		if (best < 0)
			break;
		taken[best] = 1;
		summary->weapons[summary->weapon_count++] = game->weapons[slot][best];
	}
	/* the medals: most first, the lower id of equals first */
	{
		int used[AE_MEDAL_COUNT];

		memset(used, 0, sizeof(used));
		while (summary->medal_count < AE_PGCR_SUMMARY_MEDALS)
		{
			best = -1;
			for (index = 0; index < AE_MEDAL_COUNT; index++)
			{
				if (!used[index] && game->medals[slot][index] > 0 &&
					(best < 0 || game->medals[slot][index] > game->medals[slot][best]))
				{
					best = index;
				}
			}
			if (best < 0)
				break;
			used[best] = 1;
			summary->medals[summary->medal_count].id = (unsigned char)best;
			summary->medals[summary->medal_count].count = game->medals[slot][best];
			summary->medal_count++;
		}
	}
}

static void put16(unsigned char *out, unsigned int value)
{
	out[0] = (unsigned char)(value & 255);
	out[1] = (unsigned char)((value >> 8) & 255);
}

static void put32(unsigned char *out, unsigned long value)
{
	put16(out, (unsigned int)(value & 65535));
	put16(out + 2, (unsigned int)((value >> 16) & 65535));
}

static unsigned int get16(unsigned char const *in)
{
	return (unsigned int)in[0] | ((unsigned int)in[1] << 8);
}

static unsigned long get32(unsigned char const *in)
{
	return (unsigned long)get16(in) | ((unsigned long)get16(in + 2) << 16);
}

/* version, slot, best spree (2), multikills (2), headshots (2), killed most + count, killed by + count, weapons (count,
then tag 4 and kills 2 each; at most 3 up to version 2, 6 in version 3), medals (count, then id and count each), versus
(count, then slot, killed, killed by each; version 2), then (version 3) damage dealt and taken (4 each), melee, power, grenade,
assassinations, splatters, sticks (2 each), kills as the carrier (2), carrier seconds (2), plants, defuses (1 each). A version
1 form ends after the medals, a version 2 form after the versus */
int ae_pgcr_encode_version(struct ae_pgcr_summary const *summary, unsigned char *out, int size, int version)
{
	int at = 0, index, weapons = summary->weapon_count;

	if (!summary->valid || size < AE_PGCR_WIRE_MAXIMUM || version < 1 || version > AE_PGCR_WIRE_VERSION)
		return 0;
	if (version < 3 && weapons > AE_PGCR_WIRE_V2_WEAPONS)
		weapons = AE_PGCR_WIRE_V2_WEAPONS;
	out[at++] = (unsigned char)version;
	out[at++] = (unsigned char)summary->slot;
	put16(out + at, summary->best_spree); at += 2;
	put16(out + at, summary->multikills); at += 2;
	put16(out + at, summary->headshots); at += 2;
	out[at++] = summary->killed_most;
	out[at++] = summary->killed_most_count;
	out[at++] = summary->killed_by_most;
	out[at++] = summary->killed_by_most_count;
	out[at++] = (unsigned char)weapons;
	for (index = 0; index < weapons; index++)
	{
		put32(out + at, (unsigned long)summary->weapons[index].tag); at += 4;
		put16(out + at, summary->weapons[index].kills); at += 2;
	}
	out[at++] = (unsigned char)summary->medal_count;
	for (index = 0; index < summary->medal_count; index++)
	{
		out[at++] = summary->medals[index].id;
		out[at++] = summary->medals[index].count;
	}
	if (version < 2)
		return at;
	out[at++] = (unsigned char)summary->versus_count;
	for (index = 0; index < summary->versus_count; index++)
	{
		out[at++] = summary->versus[index].slot;
		out[at++] = summary->versus[index].killed;
		out[at++] = summary->versus[index].killed_by;
	}
	if (version < 3)
		return at;
	put32(out + at, summary->damage_dealt); at += 4;
	put32(out + at, summary->damage_taken); at += 4;
	put16(out + at, summary->melee); at += 2;
	put16(out + at, summary->power); at += 2;
	put16(out + at, summary->grenade); at += 2;
	put16(out + at, summary->assassination); at += 2;
	put16(out + at, summary->splatter); at += 2;
	put16(out + at, summary->stick); at += 2;
	put16(out + at, summary->kills_as_flag_carrier); at += 2;
	put16(out + at, summary->flag_carrier_seconds); at += 2;
	out[at++] = summary->bomb_plants;
	out[at++] = summary->bomb_defuses;
	return at;
}

int ae_pgcr_encode(struct ae_pgcr_summary const *summary, unsigned char *out, int size)
{
	return ae_pgcr_encode_version(summary, out, size, AE_PGCR_WIRE_VERSION);
}

int ae_pgcr_decode(unsigned char const *data, int length, struct ae_pgcr_summary *summary)
{
	int at = 0, index;

	memset(summary, 0, sizeof(*summary));
	if (length < 14 || data[0] < 1 || data[0] > AE_PGCR_WIRE_VERSION || data[1] >= AE_PGCR_PLAYERS)
		return 0;
	summary->slot = data[1];
	summary->best_spree = (unsigned short)get16(data + 2);
	summary->multikills = (unsigned short)get16(data + 4);
	summary->headshots = (unsigned short)get16(data + 6);
	summary->killed_most = data[8];
	summary->killed_most_count = data[9];
	summary->killed_by_most = data[10];
	summary->killed_by_most_count = data[11];
	if ((summary->killed_most != AE_PGCR_NO_PLAYER && summary->killed_most >= AE_PGCR_PLAYERS) ||
		(summary->killed_by_most != AE_PGCR_NO_PLAYER && summary->killed_by_most >= AE_PGCR_PLAYERS))
		goto bad;
	summary->weapon_count = data[12];
	at = 13;
	if (summary->weapon_count > (data[0] >= 3 ? AE_PGCR_SUMMARY_WEAPONS : AE_PGCR_WIRE_V2_WEAPONS) || length < at + summary->weapon_count * 6 + 1)
		goto bad;
	for (index = 0; index < summary->weapon_count; index++)
	{
		summary->weapons[index].tag = (long)get32(data + at); at += 4;
		summary->weapons[index].kills = (unsigned short)get16(data + at); at += 2;
	}
	summary->medal_count = data[at++];
	if (summary->medal_count > AE_PGCR_SUMMARY_MEDALS || length < at + summary->medal_count * 2)
		goto bad;
	for (index = 0; index < summary->medal_count; index++)
	{
		if (data[at] >= AE_MEDAL_COUNT)
			goto bad;
		summary->medals[index].id = data[at++];
		summary->medals[index].count = data[at++];
	}
	if (data[0] >= 2)
	{
		if (length < at + 1)
			goto bad;
		summary->versus_count = data[at++];
		if (summary->versus_count > AE_PGCR_SUMMARY_VERSUS || length < at + summary->versus_count * 3)
			goto bad;
		for (index = 0; index < summary->versus_count; index++)
		{
			if (data[at] >= AE_PGCR_PLAYERS)
				goto bad;
			summary->versus[index].slot = data[at++];
			summary->versus[index].killed = data[at++];
			summary->versus[index].killed_by = data[at++];
		}
	}
	if (data[0] >= 3)
	{
		if (length < at + AE_PGCR_WIRE_EXTENSION)
			goto bad;
		summary->damage_dealt = get32(data + at); at += 4;
		summary->damage_taken = get32(data + at); at += 4;
		summary->melee = (unsigned short)get16(data + at); at += 2;
		summary->power = (unsigned short)get16(data + at); at += 2;
		summary->grenade = (unsigned short)get16(data + at); at += 2;
		summary->assassination = (unsigned short)get16(data + at); at += 2;
		summary->splatter = (unsigned short)get16(data + at); at += 2;
		summary->stick = (unsigned short)get16(data + at); at += 2;
		summary->kills_as_flag_carrier = (unsigned short)get16(data + at); at += 2;
		summary->flag_carrier_seconds = (unsigned short)get16(data + at); at += 2;
		summary->bomb_plants = data[at++];
		summary->bomb_defuses = data[at++];
		summary->extended = 1;
	}
	summary->valid = 1;
	return 1;
bad:
	memset(summary, 0, sizeof(*summary));
	return 0;
}
