/*
AE_MEDAL_RULES.C

Pure rules for MEDALS (ae_medal_rules.h). No engine includes.
*/

#include "ae_medal_rules.h"

#include <stddef.h>
#include <string.h>

/* a kill-time far enough back that no window reaches it */
#define LONG_AGO (-1000000L)

/* ---------- private code */

static int player_valid(int player)
{
	return player >= 0 && player < AE_MEDAL_RULES_PLAYERS;
}

static int team_of(struct ae_medal_kill const *kill, int player)
{
	return kill->team_of && player_valid(player) ? kill->team_of[player] : -1;
}

static void out_add(struct ae_medal_out *out, int player, int medal)
{
	if (out->count < AE_MEDAL_RULES_OUT)
	{
		out->items[out->count].player = (short)player;
		out->items[out->count].medal = (short)medal;
		out->count++;
	}
}

static int out_has_for(struct ae_medal_out const *out, int player)
{
	int index;

	for (index = 0; index < out->count; index++)
		if (out->items[index].player == player)
			return 1;
	return 0;
}

static void clear_player(struct ae_medal_player *player)
{
	memset(player, 0, sizeof(*player));
	player->last_kill_tick = LONG_AGO;
	player->strike_tick = LONG_AGO;
	player->last_killer = -1;
	player->last_victim = -1;
	player->last_grenade_tick = LONG_AGO;
	player->last_sniper_tick = LONG_AGO;
}

/* a death: the dead player's runs are over */
static void player_died(struct ae_medal_player *player)
{
	player->deaths++;
	player->deaths_since_kill++;
	player->spree = 0;
	player->multikill = 0;
	player->last_kill_tick = LONG_AGO;
	player->sniper_run = 0;
	player->shotgun_run = 0;
	player->splatter_run = 0;
	player->last_category = AE_MEDAL_CATEGORY_NONE;
	player->last_victim = -1;
	player->ball_kills = 0;
	player->juggernaut_kills = 0;
	player->hail_kills = 0;
	player->last_grenade_tick = LONG_AGO;
	player->last_sniper_tick = LONG_AGO;
}

/* the damage tag's path in lower case with backslashes (a path too long for the text is cut: it is none of ours) */
static void path_normalise(char const *path, char *text, size_t size)
{
	size_t length = 0;

	for (; *path && length + 1 < size; path++, length++)
	{
		char character = *path;

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		else if (character == '/')
			character = '\\';
		text[length] = character;
	}
	text[length] = 0;
}

static void spree_medal(struct ae_medal_out *out, int player, int spree)
{
	static int const medals[] = {
		AE_MEDAL_KILLING_SPREE, AE_MEDAL_KILLING_FRENZY, AE_MEDAL_RUNNING_RIOT, AE_MEDAL_RAMPAGE,
		AE_MEDAL_UNTOUCHABLE_ROW_WIT_UNTOUCHABLE, AE_MEDAL_INVINCIBLE, AE_MEDAL_INCONCEIVABLE,
		AE_MEDAL_UNFRIGGENBELIEVABLE_ROW_WIT_UNFRIGGENBELIEVABLE };

	if (spree >= 5 && spree <= 40 && spree % 5 == 0)
		out_add(out, player, medals[spree / 5 - 1]);
}

static void multikill_medal(struct ae_medal_out *out, int player, int count)
{
	static int const medals[] = {
		AE_MEDAL_DOUBLE_KILL, AE_MEDAL_TRIPLE_OF_EACH_KILL, AE_MEDAL_OVERKILL, AE_MEDAL_KILLTACULAR,
		AE_MEDAL_KILLTROCITY, AE_MEDAL_KILLIMANJARO, AE_MEDAL_KILLTASTROPHE, AE_MEDAL_KILLPOCALYPSE,
		AE_MEDAL_KILLIONAIRE };

	if (count >= 2 && count <= 10)
		out_add(out, player, medals[count - 2]);
}

/* the assists of a kill: attackers other than the killer, enemies of the victim, whose damage was recent and more than
a share of the best */
static void assists(struct ae_medal_game *game, struct ae_medal_kill const *kill, struct ae_medal_out *out)
{
	int count = kill->attacker_count;
	float best = 0.0f;
	int seen[AE_MEDAL_RULES_ATTACKERS];
	int seen_count = 0;
	int index, other;

	if (count > AE_MEDAL_RULES_ATTACKERS)
		count = AE_MEDAL_RULES_ATTACKERS;
	/* the best share: the killer's own, or if the killer has none the biggest of the others' */
	for (index = 0; index < count; index++)
	{
		struct ae_medal_attacker const *attacker = &kill->attackers[index];

		if (attacker->player == kill->killer && attacker->damage > best)
			best = attacker->damage;
	}
	if (best <= 0.0f)
	{
		for (index = 0; index < count; index++)
			if (kill->attackers[index].damage > best)
				best = kill->attackers[index].damage;
	}
	for (index = 0; index < count; index++)
	{
		struct ae_medal_attacker const *attacker = &kill->attackers[index];
		int duplicate = 0;

		if (!player_valid(attacker->player) || attacker->player == kill->killer || attacker->player == kill->victim)
			continue;
		if (team_of(kill, attacker->player) == team_of(kill, kill->victim))
			continue;
		if (attacker->tick <= kill->tick - AE_MEDAL_ASSIST_TICKS)
			continue;
		if (!(attacker->damage * 100.0f > best * (float)AE_MEDAL_ASSIST_PERCENT))
			continue;
		for (other = 0; other < seen_count; other++)
			duplicate |= seen[other] == attacker->player;
		if (duplicate)
			continue;
		seen[seen_count++] = attacker->player;
		game->players[attacker->player].assists++;
		out_add(out, attacker->player, AE_MEDAL_ASSIST);
		if (game->players[attacker->player].assists == AE_MEDAL_WINGMAN_ASSISTS)
			out_add(out, attacker->player, AE_MEDAL_WINGMAN);
	}
}

/* the weapon's own medals, in the list's order */
static void weapon_medals(struct ae_medal_kill const *kill, struct ae_medal_out *out)
{
	int killer = kill->killer;

	if (kill->category == AE_MEDAL_CATEGORY_VEHICLE)
		out_add(out, killer, AE_MEDAL_SPLATTER_WITH_A__SPLATTER);
	if (kill->category == AE_MEDAL_CATEGORY_MELEE)
	{
		if (kill->from_behind && kill->killed_instantly && kill->side_effect == AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING)
		{
			/* a blow from behind that kills outright: on a charged shield it is ACHILLES SPINE, on a camouflaged player
			SNEAK KING; a ninja's leaves the plain melee kill, which the leap earns the medal for */
			if (kill->victim_overshield)
				out_add(out, killer, AE_MEDAL_ACHILLES_SPINE);
			else if (kill->victim_invisible)
				out_add(out, killer, AE_MEDAL_SNEAK_KING);
			else if (kill->ninja)
				out_add(out, killer, AE_MEDAL_MELEE_KILL);
			else
				out_add(out, killer, AE_MEDAL_BEAT_DOWN);
		}
		else
			out_add(out, killer, AE_MEDAL_MELEE_KILL);
	}
	if (kill->weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK)
		out_add(out, killer, AE_MEDAL_GRENADE_STICK);
	if (kill->weapon == AE_MEDAL_WEAPON_BALL)
		out_add(out, killer, AE_MEDAL_BALL_KILL);
	if (kill->weapon == AE_MEDAL_WEAPON_ROCKET)
		out_add(out, killer, AE_MEDAL_ROCKET_KILL);
	if (kill->weapon == AE_MEDAL_WEAPON_NEEDLER_SUPERCOMBINE)
		out_add(out, killer, AE_MEDAL_SUPERCOMBINE_USING_A_SUPERCOMBINE);
	if (kill->category == AE_MEDAL_CATEGORY_MOUNTED)
		out_add(out, killer, AE_MEDAL_VEHICLE_A_VEHIC_KILL);
	if (kill->weapon == AE_MEDAL_WEAPON_FRAG_GRENADE)
		out_add(out, killer, AE_MEDAL_GRENADE_KILL);
	if (kill->weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE)
		out_add(out, killer, AE_MEDAL_PLASMA_KILL);
	if (kill->weapon == AE_MEDAL_WEAPON_FLAG)
		out_add(out, killer, AE_MEDAL_FLAG_KILL);
	if (kill->weapon == AE_MEDAL_WEAPON_FUEL_ROD)
		out_add(out, killer, AE_MEDAL_FUEL_ROD_CANNON_KILL);
}


/* whether a kill was made with a gun: its damage is a bullet's, a plasma's, a needle's, a sniper's, a shotgun's or a flame's */
static int kill_by_gun(struct ae_medal_kill const *kill)
{
	switch (kill->category)
	{
	case AE_MEDAL_CATEGORY_BULLET:
	case AE_MEDAL_CATEGORY_SNIPER:
	case AE_MEDAL_CATEGORY_PLASMA:
	case AE_MEDAL_CATEGORY_NEEDLE:
	case AE_MEDAL_CATEGORY_SHOTGUN:
	case AE_MEDAL_CATEGORY_FLAME:
		return 1;
	default:
		return 0;
	}
}

static int weapon_is_grenade(int weapon)
{
	return weapon == AE_MEDAL_WEAPON_FRAG_GRENADE || weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE ||
		weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK;
}

/* a kind of kill counted: the medal comes at the count, once */
static void proficiency(struct ae_medal_out *out, int player, int *count, int medal)
{
	(*count)++;
	if (*count == AE_MEDAL_PROFICIENCY_KILLS)
		out_add(out, player, medal);
}

/* the medals of a kill whose facts the glue read at the kill: the victim's and the killer's state, the vehicle, the records'
findings, and the kinds of kills counted for the game (AE_MEDAL_PROFICIENCY_KILLS each) */
static void batch2_medals(struct ae_medal_kill const *kill, struct ae_medal_player *killer, struct ae_medal_out *out)
{
	int who = kill->killer;
	int grenade = weapon_is_grenade(kill->weapon);
	int gun = kill_by_gun(kill);
	int sniper = kill->category == AE_MEDAL_CATEGORY_SNIPER;
	int role = kill->killer_role;

	if (kill->victim_power_weapon && kill->victim_inventory_full)
		out_add(out, who, AE_MEDAL_YARD_SALE);
	if (kill->killer_magazine_empty && kill->category == AE_MEDAL_CATEGORY_BULLET && !ae_medal_rules_role_is_power(role))
		out_add(out, who, AE_MEDAL_LAST_SHOT);
	if (grenade && kill->distance > AE_MEDAL_HAIL_MARY_DISTANCE)
		out_add(out, who, AE_MEDAL_HAIL_MARY);
	if (grenade)
	{
		if (killer->last_grenade_tick >= kill->tick - AE_MEDAL_PAIR_TICKS)
		{
			out_add(out, who, AE_MEDAL_CLUSTER_LUCK);
			killer->last_grenade_tick = LONG_AGO;
		}
		else
			killer->last_grenade_tick = kill->tick;
	}
	if (sniper)
	{
		if (killer->last_sniper_tick >= kill->tick - AE_MEDAL_PAIR_TICKS)
		{
			out_add(out, who, AE_MEDAL_QUIGLEY);
			killer->last_sniper_tick = LONG_AGO;
		}
		else
			killer->last_sniper_tick = kill->tick;
		if (kill->victim_driving && kill->victim_vehicle_speed > AE_MEDAL_AUTOPILOT_SPEED)
			out_add(out, who, AE_MEDAL_AUTOPILOT_ENGAGED);
	}
	if (kill->killer_seat == AE_MEDAL_SEAT_PASSENGER)
	{
		if (killer->multikill == 2)
			out_add(out, who, AE_MEDAL_DRIVEBY);
		if (grenade)
			out_add(out, who, AE_MEDAL_PINEAPPLE_EXPRESS);
	}
	/* the records' findings */
	if (kill->no_scope)
		out_add(out, who, AE_MEDAL_NO_SCOPE);
	if (kill->quick_draw)
		out_add(out, who, AE_MEDAL_QUICK_DRAW);
	if (kill->reversal)
		out_add(out, who, AE_MEDAL_REVERSAL);
	if (kill->nade_shot)
		out_add(out, who, AE_MEDAL_NADE_SHOT);
	if (kill->turned_360)
		out_add(out, who, AE_MEDAL_THREE_SIXTY);
	if (kill->ninja)
		out_add(out, who, AE_MEDAL_NINJA);
	if (kill->off_the_rack)
		out_add(out, who, AE_MEDAL_OFF_THE_RACK);

	/* the kinds of kills */
	if (gun && (role == AE_MEDAL_ROLE_PISTOL || role == AE_MEDAL_ROLE_PLASMA_PISTOL))
		proficiency(out, who, &killer->gunslinger_kills, AE_MEDAL_GUNSLINGER);
	if (gun && (role == AE_MEDAL_ROLE_ASSAULT_RIFLE || role == AE_MEDAL_ROLE_SMG || role == AE_MEDAL_ROLE_BATTLE_RIFLE ||
		role == AE_MEDAL_ROLE_EVO_RIFLE || role == AE_MEDAL_ROLE_DMR || role == AE_MEDAL_ROLE_CARBINE ||
		role == AE_MEDAL_ROLE_PLASMA_RIFLE))
	{
		proficiency(out, who, &killer->rifleman_kills, AE_MEDAL_RIFLEMAN);
	}
	if (kill->category == AE_MEDAL_CATEGORY_SHOTGUN)
		proficiency(out, who, &killer->scattergunner_kills, AE_MEDAL_SCATTERGUNNER);
	if (kill->weapon == AE_MEDAL_WEAPON_ROCKET || kill->weapon == AE_MEDAL_WEAPON_FUEL_ROD)
		proficiency(out, who, &killer->bomber_kills, AE_MEDAL_BOMBER);
	if (grenade)
		proficiency(out, who, &killer->grenadier_kills, AE_MEDAL_GRENADIER);
	if (kill->category == AE_MEDAL_CATEGORY_MELEE)
		proficiency(out, who, &killer->boxer_kills, AE_MEDAL_BOXER);
	/* kills from a vehicle's seat: a splatter or a gun of the vehicle's (a rider's own weapon is a passenger's, and a blow
	or a grenade is the rider's, not the vehicle's) */
	if (kill->category != AE_MEDAL_CATEGORY_NONE && kill->category != AE_MEDAL_CATEGORY_MELEE && !grenade)
	{
		if (kill->killer_seat == AE_MEDAL_SEAT_DRIVER && kill->killer_vehicle == AE_MEDAL_VEHICLE_GROUND)
			proficiency(out, who, &killer->driver_kills, AE_MEDAL_DRIVER);
		if (kill->killer_seat == AE_MEDAL_SEAT_DRIVER && kill->killer_vehicle == AE_MEDAL_VEHICLE_AIR)
			proficiency(out, who, &killer->pilot_kills, AE_MEDAL_PILOT);
		if (kill->killer_seat == AE_MEDAL_SEAT_GUNNER)
			proficiency(out, who, &killer->gunner_kills, AE_MEDAL_GUNNER);
	}
}

/* the game type's medals of a kill */
static void mode_medals(struct ae_medal_game *game, struct ae_medal_kill const *kill, struct ae_medal_player *killer,
	struct ae_medal_out *out)
{
	switch (game->mode)
	{
	case AE_MEDAL_MODE_CTF:
		if (!game->assault && kill->victim_carried_flag)
		{
			out_add(out, kill->killer, AE_MEDAL_FLAG_CARRIER_KILL);
			if (kill->killer_carried_flag)
				out_add(out, kill->killer, AE_MEDAL_FLAG_JOUST);
			if (kill->victim_stand_distance >= 0.0f && kill->victim_stand_distance < AE_MEDAL_STAND_DISTANCE)
				out_add(out, kill->killer, AE_MEDAL_STOPPED_SHORT);
		}
		break;
	case AE_MEDAL_MODE_ODDBALL:
		if (kill->victim_carried_ball)
		{
			out_add(out, kill->killer,
				game->ball_type == AE_MEDAL_BALL_JUGGERNAUT ? AE_MEDAL_KILLED_JUGGERNAUT : AE_MEDAL_BALL_CARRIER_KILL);
			/* a carrier who was within a few percent of winning on the ball's time (the normal ball's) */
			if (game->ball_type == AE_MEDAL_BALL_NORMAL && kill->victim_score_to_win > 0 &&
				kill->victim_score * 100 >= kill->victim_score_to_win * AE_MEDAL_FUMBLE_PERCENT)
			{
				out_add(out, kill->killer, AE_MEDAL_FUMBLE);
			}
		}
		if (game->ball_type == AE_MEDAL_BALL_JUGGERNAUT)
		{
			if (kill->killer_carried_ball)
			{
				killer->juggernaut_kills++;
				if (killer->juggernaut_kills == AE_MEDAL_JUGGERNAUT_SPREE_KILLS)
					out_add(out, kill->killer, AE_MEDAL_JUGGERNAUT_SPREE);
				if (killer->juggernaut_kills == AE_MEDAL_IMMOVABLE_KILLS)
					out_add(out, kill->killer, AE_MEDAL_IMMOVABLE_OBJECT);
			}
			else
				killer->juggernaut_kills = 0;
		}
		else if (kill->killer_carried_ball)
		{
			if (kill->weapon == AE_MEDAL_WEAPON_BALL)
				killer->ball_kills++;
			if (game->ball_type == AE_MEDAL_BALL_NORMAL && killer->multikill == 3 && killer->ball_kills > 2)
				out_add(out, kill->killer, AE_MEDAL_SMOOTH_WITH_TH_MOVES);
		}
		break;
	case AE_MEDAL_MODE_KING:
		if (kill->killer_on_hill)
		{
			killer->hail_kills++;
			if (killer->hail_kills == AE_MEDAL_HAIL_KILLS)
				out_add(out, kill->killer, AE_MEDAL_HAIL_TO_THE_KING);
			out_add(out, kill->killer, AE_MEDAL_HILL_DEFENSE);
		}
		else
			killer->hail_kills = 0;
		if (kill->victim_on_hill)
			out_add(out, kill->killer, AE_MEDAL_HILL_OFFENSE);
		break;
	default:
		break;
	}
}

/* an enemy's kill of a player: what the killer and the others earn (the death itself is the caller's) */
static void award_kill(struct ae_medal_game *game, struct ae_medal_kill const *kill, struct ae_medal_out *out)
{
	struct ae_medal_player *killer = &game->players[kill->killer];
	struct ae_medal_player *victim = &game->players[kill->victim];
	int killer_index = kill->killer;
	int category = kill->category;

	assists(game, kill, out);
	if (victim->spree >= AE_MEDAL_KILLJOY_SPREE)
		out_add(out, killer_index, AE_MEDAL_KILLJOY);

	killer->kills++;
	if (kill->headshot)
		killer->headshots++;
	killer->spree++;
	spree_medal(out, killer_index, killer->spree);

	if (killer->last_kill_tick >= kill->tick - AE_MEDAL_MULTIKILL_TICKS)
		killer->multikill++;
	else
		killer->multikill = 1;
	killer->last_kill_tick = kill->tick;
	multikill_medal(out, killer_index, killer->multikill);

	/* the weapon runs: a kill of another category ends them all */
	if (category != killer->last_category)
	{
		killer->sniper_run = 0;
		killer->shotgun_run = 0;
		killer->splatter_run = 0;
		killer->last_category = category;
	}
	if (category == AE_MEDAL_CATEGORY_SNIPER)
	{
		killer->sniper_run++;
		if (killer->sniper_run == 5)
			out_add(out, killer_index, AE_MEDAL_SNIPER_RIFLE_W_SPREE);
		if (killer->sniper_run == 10)
			out_add(out, killer_index, AE_MEDAL_SHARPSHOOTER_RIFLE_W_SHARPSHOOTER);
	}
	if (category == AE_MEDAL_CATEGORY_SHOTGUN)
	{
		killer->shotgun_run++;
		if (killer->shotgun_run == 5)
			out_add(out, killer_index, AE_MEDAL_SHOTGUN_SHOTGUN_SPREE);
		if (killer->shotgun_run == 10)
			out_add(out, killer_index, AE_MEDAL_OPEN_SEASON);
	}
	if (category == AE_MEDAL_CATEGORY_VEHICLE)
	{
		killer->splatter_run++;
		if (killer->splatter_run == 5)
			out_add(out, killer_index, AE_MEDAL_SPLATTER_ROW_WIT_SPREE);
		if (killer->splatter_run == 10)
			out_add(out, killer_index, AE_MEDAL_VEHICLE_ROW_WIT_MANSLAUGHTER);
	}

	if (killer->deaths_since_kill >= AE_MEDAL_COMEBACK_DEATHS)
		out_add(out, killer_index, AE_MEDAL_COMEBACK_KILL);
	killer->deaths_since_kill = 0;

	if (kill->killer_dead)
		out_add(out, killer_index, AE_MEDAL_KILL_FROM_THE_GRAVE);
	if (kill->victim_invisible)
		out_add(out, killer_index, AE_MEDAL_I_SEE_YOU);
	if (kill->victim_reloading)
		out_add(out, killer_index, AE_MEDAL_RELOAD_THIS);
	/* (NO SCOPE says it of a sniper kill: not this as well) */
	if (category == AE_MEDAL_CATEGORY_SNIPER && kill->killer_snapshot && !kill->no_scope)
		out_add(out, killer_index, AE_MEDAL_SNAPSHOT);

	if (killer->last_killer == kill->victim)
	{
		out_add(out, killer_index, AE_MEDAL_REVENGE);
		killer->last_killer = -1;
	}
	if (game->teams && player_valid(victim->last_victim) && victim->last_victim != killer_index &&
		team_of(kill, victim->last_victim) == team_of(kill, killer_index))
	{
		out_add(out, killer_index, AE_MEDAL_AVENGER);
	}

	if (kill->headshot)
	{
		/* a perfect kill is told in the headshot's place */
		if (kill->perfect)
			out_add(out, killer_index, AE_MEDAL_PERFECT);
		else
			out_add(out, killer_index, kill->weapon == AE_MEDAL_WEAPON_SNIPER ? AE_MEDAL_SNIPER_KILL : AE_MEDAL_HEADSHOT);
	}
	weapon_medals(kill, out);
	batch2_medals(kill, killer, out);

	if (game->teams && player_valid(kill->driver) && kill->driver != killer_index &&
		team_of(kill, kill->driver) == team_of(kill, killer_index))
	{
		out_add(out, kill->driver, AE_MEDAL_WHEELMAN_KILL_AN_WHEELMAN);
	}
	if (game->teams && killer->multikill >= AE_MEDAL_EXTERMINATION_MULTIKILL && kill->victim_team_survivors == 0)
		out_add(out, killer_index, AE_MEDAL_EXTERMINATION);
	if (kill->killer_close_call)
		out_add(out, killer_index, AE_MEDAL_CLOSE_CALL);
	if (game->teams && kill->protected_teammate && !kill->killer_dead)
	{
		out_add(out, killer_index, AE_MEDAL_PROTECTOR);
		if (kill->distance >= AE_MEDAL_GUARDIAN_ANGEL_DISTANCE)
			out_add(out, killer_index, AE_MEDAL_GUARDIAN_ANGEL);
		proficiency(out, killer_index, &killer->protector_kills, AE_MEDAL_BODYGUARD);
	}
	if (!game->first_strike_done)
		out_add(out, killer_index, AE_MEDAL_FIRST_STRIKE);
	game->first_strike_done = 1;

	mode_medals(game, kill, killer, out);
	if (!out_has_for(out, killer_index))
		out_add(out, killer_index, AE_MEDAL_KILL);

	killer->last_victim = kill->victim;
	victim->last_killer = killer_index;
	killer->strike_tick = ++game->strikes;
}

/* the hill's tracked first scorer held it to here */
static void hill_finish(struct ae_medal_game *game, struct ae_medal_out *out)
{
	if (game->hill_first >= 0 && game->hill_valid)
	{
		if (game->hill_on_foot && !game->hill_in_vehicle)
			out_add(out, game->hill_first, AE_MEDAL_HILL_DOMINANCE);
		else if (game->hill_in_vehicle && !game->hill_on_foot)
			out_add(out, game->hill_first, AE_MEDAL_VEHICLE_DOMINANCE);
	}
	game->hill_valid = 0;
}

/* ---------- public code */

void ae_medal_rules_reset(struct ae_medal_game *game, int mode, int teams, int assault, int ball_type)
{
	int index;

	memset(game, 0, sizeof(*game));
	game->mode = mode;
	game->teams = teams;
	game->assault = assault;
	game->ball_type = ball_type;
	game->hill_first = -1;
	for (index = 0; index < AE_MEDAL_RULES_PLAYERS; index++)
		clear_player(&game->players[index]);
}

void ae_medal_rules_clear_player(struct ae_medal_game *game, int player)
{
	if (player_valid(player))
		clear_player(&game->players[player]);
}

int ae_medal_rules_weapon(char const *path)
{
	char text[256];

	if (!path)
		return AE_MEDAL_WEAPON_NONE;
	path_normalise(path, text, sizeof(text));
	if (strstr(text, "sniper"))
		return AE_MEDAL_WEAPON_SNIPER;
	if (strstr(text, "plasma grenade"))
		return strstr(text, "attached") ? AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK : AE_MEDAL_WEAPON_PLASMA_GRENADE;
	if (strstr(text, "frag grenade"))
		return AE_MEDAL_WEAPON_FRAG_GRENADE;
	if (strstr(text, "rocket"))
		return AE_MEDAL_WEAPON_ROCKET;
	if (strstr(text, "needler"))
		return strstr(text, "explosion") || strstr(text, "shock wave") ? AE_MEDAL_WEAPON_NEEDLER_SUPERCOMBINE :
			AE_MEDAL_WEAPON_NEEDLER;
	if (strstr(text, "fuel rod") || strstr(text, "fuel_rod") || strstr(text, "plasma cannon") ||
		strstr(text, "plasma_cannon"))
	{
		return AE_MEDAL_WEAPON_FUEL_ROD;
	}
	if (strstr(text, "\\flag\\"))
		return AE_MEDAL_WEAPON_FLAG;
	if (strstr(text, "\\ball\\"))
		return AE_MEDAL_WEAPON_BALL;
	return AE_MEDAL_WEAPON_OTHER;
}

int ae_medal_rules_role(char const *path)
{
	char text[256];

	if (!path)
		return AE_MEDAL_ROLE_NONE;
	path_normalise(path, text, sizeof(text));
	if (strstr(text, "plasma pistol") || strstr(text, "plasma_pistol"))
		return AE_MEDAL_ROLE_PLASMA_PISTOL;
	if (strstr(text, "plasma rifle") || strstr(text, "plasma_rifle"))
		return AE_MEDAL_ROLE_PLASMA_RIFLE;
	if (strstr(text, "pistol") || strstr(text, "magnum"))
		return AE_MEDAL_ROLE_PISTOL;
	if (strstr(text, "assault rifle") || strstr(text, "assault_rifle"))
		return AE_MEDAL_ROLE_ASSAULT_RIFLE;
	if (strstr(text, "smg"))
		return AE_MEDAL_ROLE_SMG;
	if (strstr(text, "battle rifle") || strstr(text, "battle_rifle"))
		return AE_MEDAL_ROLE_BATTLE_RIFLE;
	if (strstr(text, "evo rifle") || strstr(text, "evo_rifle"))
		return AE_MEDAL_ROLE_EVO_RIFLE;
	if (strstr(text, "dmr"))
		return AE_MEDAL_ROLE_DMR;
	if (strstr(text, "carbine"))
		return AE_MEDAL_ROLE_CARBINE;
	if (strstr(text, "shotgun"))
		return AE_MEDAL_ROLE_SHOTGUN;
	if (strstr(text, "sniper"))
		return AE_MEDAL_ROLE_SNIPER;
	if (strstr(text, "rocket"))
		return AE_MEDAL_ROLE_ROCKET;
	if (strstr(text, "fuel rod") || strstr(text, "fuel_rod") || strstr(text, "plasma cannon") ||
		strstr(text, "plasma_cannon"))
	{
		return AE_MEDAL_ROLE_FUEL_ROD;
	}
	if (strstr(text, "sword"))
		return AE_MEDAL_ROLE_SWORD;
	if (strstr(text, "hammer"))
		return AE_MEDAL_ROLE_HAMMER;
	if (strstr(text, "needler"))
		return AE_MEDAL_ROLE_NEEDLER;
	return AE_MEDAL_ROLE_OTHER;
}

int ae_medal_rules_role_of_word(char const *word)
{
	static struct
	{
		char const *word;
		int role;
	} const words[] =
	{
		{ "pistol", AE_MEDAL_ROLE_PISTOL }, { "plasma_pistol", AE_MEDAL_ROLE_PLASMA_PISTOL },
		{ "assault_rifle", AE_MEDAL_ROLE_ASSAULT_RIFLE }, { "smg", AE_MEDAL_ROLE_SMG },
		{ "battle_rifle", AE_MEDAL_ROLE_BATTLE_RIFLE }, { "evo_rifle", AE_MEDAL_ROLE_EVO_RIFLE }, { "dmr", AE_MEDAL_ROLE_DMR },
		{ "carbine", AE_MEDAL_ROLE_CARBINE }, { "plasma_rifle", AE_MEDAL_ROLE_PLASMA_RIFLE },
		{ "shotgun", AE_MEDAL_ROLE_SHOTGUN }, { "sniper", AE_MEDAL_ROLE_SNIPER }, { "rocket", AE_MEDAL_ROLE_ROCKET },
		{ "fuel_rod", AE_MEDAL_ROLE_FUEL_ROD }, { "sword", AE_MEDAL_ROLE_SWORD }, { "hammer", AE_MEDAL_ROLE_HAMMER },
		{ "needler", AE_MEDAL_ROLE_NEEDLER }, { "other", AE_MEDAL_ROLE_OTHER }, { "melee", AE_MEDAL_ROLE_OTHER }
	};
	unsigned int index;

	for (index = 0; word && index < sizeof(words) / sizeof(words[0]); index++)
	{
		char const *a = word, *b = words[index].word;

		for (; *a && *b && ((*a >= 'A' && *a <= 'Z') ? *a + ('a' - 'A') : *a) == *b; a++, b++)
			;
		if (!*a && !*b)
			return words[index].role;
	}
	return AE_MEDAL_ROLE_NONE;
}

int ae_medal_rules_role_is_power(int role)
{
	return role == AE_MEDAL_ROLE_SNIPER || role == AE_MEDAL_ROLE_ROCKET || role == AE_MEDAL_ROLE_FUEL_ROD ||
		role == AE_MEDAL_ROLE_SWORD || role == AE_MEDAL_ROLE_HAMMER;
}

void ae_medal_rules_kill(struct ae_medal_game *game, struct ae_medal_kill const *kill, struct ae_medal_out *out)
{
	int enemy_kill;

	out->count = 0;
	if (!player_valid(kill->victim))
		return;
	enemy_kill = player_valid(kill->killer) && kill->killer != kill->victim && !kill->betrayal;
	if (enemy_kill)
		award_kill(game, kill, out);
	else if (player_valid(kill->killer) && kill->killer != kill->victim)
		/* a betrayal: the killer has no one to avenge on its account */
		game->players[kill->killer].last_victim = -1;
	player_died(&game->players[kill->victim]);
}

void ae_medal_rules_flag_at(struct ae_medal_game *game, int player, int what, float stand_distance,
	struct ae_medal_out *out)
{
	struct ae_medal_player *state;

	out->count = 0;
	if (!player_valid(player) || game->assault)
		return;
	state = &game->players[player];
	switch (what)
	{
	case AE_MEDAL_FLAG_GRAB:
		out_add(out, player, AE_MEDAL_FLAG_TAKEN);
		break;
	case AE_MEDAL_FLAG_RETURN:
		out_add(out, player, AE_MEDAL_FLAG_RETURNED);
		/* a flag taken back where the enemy would have scored with it */
		if (stand_distance >= 0.0f && stand_distance < AE_MEDAL_STAND_DISTANCE)
			out_add(out, player, AE_MEDAL_GOAL_LINE_STAND);
		break;
	case AE_MEDAL_FLAG_CAPTURE:
		state->captures++;
		out_add(out, player, AE_MEDAL_FLAG_CAPTURED);
		if (state->captures == 2)
			out_add(out, player, AE_MEDAL_FLAG_RUNNER);
		if (state->captures == 3)
			out_add(out, player, AE_MEDAL_FLAG_CHAMPION);
		break;
	default:
		break;
	}
}

void ae_medal_rules_flag(struct ae_medal_game *game, int player, int what, struct ae_medal_out *out)
{
	ae_medal_rules_flag_at(game, player, what, -1.0f, out);
}

void ae_medal_rules_ball_tick(struct ae_medal_game *game, int player, int holding, struct ae_medal_out *out)
{
	struct ae_medal_player *state;

	out->count = 0;
	if (!player_valid(player))
		return;
	state = &game->players[player];
	if (!holding)
	{
		state->ball_ticks = 0;
		return;
	}
	if (state->ball_ticks == 0 && !game->first_touch_done)
	{
		game->first_touch_done = 1;
		if (game->ball_type == AE_MEDAL_BALL_NORMAL)
			out_add(out, player, AE_MEDAL_FIRST_TOUCH);
	}
	state->ball_ticks++;
	if (state->ball_ticks == AE_MEDAL_STRAIGHT_BALLING_TICKS)
		out_add(out, player, AE_MEDAL_STRAIGHT_BALLING);
	if (game->ball_type == AE_MEDAL_BALL_JUGGERNAUT)
		return;
	switch (state->ball_ticks)
	{
	case AE_MEDAL_BALL_HOLDER_TICKS: out_add(out, player, AE_MEDAL_BALL_HOLDER); break;
	case AE_MEDAL_BALL_KEEPER_TICKS: out_add(out, player, AE_MEDAL_BALL_KEEPER); break;
	case AE_MEDAL_BALL_MASTER_TICKS: out_add(out, player, AE_MEDAL_BALL_MASTER); break;
	case AE_MEDAL_BALL_HOG_TICKS: out_add(out, player, AE_MEDAL_BALL_HOG); break;
	case AE_MEDAL_MAGIC_HANDS_TICKS: out_add(out, player, AE_MEDAL_MAGIC_HANDS); break;
	default: break;
	}
}

void ae_medal_rules_hill_tick(struct ae_medal_game *game, int player, int alive, int on_hill, int in_vehicle,
	struct ae_medal_out *out)
{
	out->count = 0;
	if (!player_valid(player))
		return;
	if (game->hill_first < 0 && alive && on_hill)
	{
		game->hill_first = player;
		game->hill_valid = 1;
		game->hill_on_foot = 0;
		game->hill_in_vehicle = 0;
		out_add(out, player, AE_MEDAL_FIRST_POIINT);
	}
	if (game->hill_first == player && game->hill_valid)
	{
		if (!alive || !on_hill)
			game->hill_valid = 0;
		else if (in_vehicle)
			game->hill_in_vehicle = 1;
		else
			game->hill_on_foot = 1;
	}
}

void ae_medal_rules_hill_moved(struct ae_medal_game *game, struct ae_medal_out *out)
{
	out->count = 0;
	hill_finish(game, out);
}

void ae_medal_rules_hill_guardian_tick(struct ae_medal_game *game, int player, int alive, int on_hill, int contested,
	struct ae_medal_out *out)
{
	struct ae_medal_player *state;

	out->count = 0;
	if (!player_valid(player) || game->mode != AE_MEDAL_MODE_KING)
		return;
	state = &game->players[player];
	/* off the hill, or dead: the time is gone. A contest holds it still */
	if (!alive || !on_hill)
	{
		state->hill_ticks = 0;
		return;
	}
	if (contested)
		return;
	state->hill_ticks++;
	if (state->hill_ticks >= AE_MEDAL_HILL_GUARDIAN_TICKS)
	{
		state->hill_ticks -= AE_MEDAL_HILL_GUARDIAN_TICKS;
		out_add(out, player, AE_MEDAL_HILL_GUARDIAN);
	}
}

void ae_medal_rules_checkpoint(struct ae_medal_game *game, int player, long tick, struct ae_medal_out *out)
{
	struct ae_medal_player *state;

	out->count = 0;
	if (!player_valid(player))
		return;
	state = &game->players[player];
	if (!game->pole_position_done)
	{
		game->pole_position_done = 1;
		out_add(out, player, AE_MEDAL_POLE_POSITION);
	}
	state->checkpoints++;
	if (state->checkpoints == AE_MEDAL_REDLINED_CHECKPOINTS && tick < AE_MEDAL_REDLINED_TICKS)
		out_add(out, player, AE_MEDAL_REDLINED_IT);
}

void ae_medal_rules_game_end(struct ae_medal_game *game, struct ae_medal_end const *end, struct ae_medal_out *out)
{
	int player;

	out->count = 0;
	/* (the last kill of those still in the game) */
	{
		int striker = -1;

		for (player = 0; player < AE_MEDAL_RULES_PLAYERS; player++)
		{
			if (end->used[player] && game->players[player].strike_tick != LONG_AGO &&
				(striker < 0 || game->players[player].strike_tick > game->players[striker].strike_tick))
			{
				striker = player;
			}
		}
		if (striker >= 0)
			out_add(out, striker, AE_MEDAL_LAST_STRIKE);
	}
	if (game->mode == AE_MEDAL_MODE_KING)
		hill_finish(game, out);
	for (player = 0; player < AE_MEDAL_RULES_PLAYERS; player++)
	{
		struct ae_medal_player const *state = &game->players[player];

		if (!end->used[player])
			continue;
		if (state->kills >= AE_MEDAL_TRIPLE_DOUBLE_EACH && state->assists >= AE_MEDAL_TRIPLE_DOUBLE_EACH &&
			state->headshots >= AE_MEDAL_TRIPLE_DOUBLE_EACH)
		{
			out_add(out, player, AE_MEDAL_TRIPLE_DOUBLE);
		}
		if (game->mode == AE_MEDAL_MODE_SLAYER && end->winner[player] && state->kills >= AE_MEDAL_PERFECTION_KILLS &&
			state->deaths == 0)
		{
			out_add(out, player, AE_MEDAL_PERFECTION);
		}
		if (game->mode == AE_MEDAL_MODE_RACE && end->winner[player])
			out_add(out, player, AE_MEDAL_FIRST_PLACE);
	}
	if (game->mode == AE_MEDAL_MODE_SLAYER)
	{
		if (game->teams)
		{
			if (end->team_lead >= AE_MEDAL_STEAKTACULAR_LEAD)
			{
				for (player = 0; player < AE_MEDAL_RULES_PLAYERS; player++)
					if (end->used[player] && end->team[player] == end->leading_team)
						out_add(out, player, AE_MEDAL_STEAKTACULAR);
			}
		}
		else
		{
			int top = -1, top_kills = 0, second_kills = 0;

			for (player = 0; player < AE_MEDAL_RULES_PLAYERS; player++)
			{
				int kills = game->players[player].kills;

				if (!end->used[player])
					continue;
				if (top < 0 || kills > top_kills)
				{
					if (top >= 0)
						second_kills = top_kills;
					top = player;
					top_kills = kills;
				}
				else if (kills > second_kills)
					second_kills = kills;
			}
			if (top >= 0 && top_kills - second_kills >= AE_MEDAL_STEAKTACULAR_LEAD)
				out_add(out, top, AE_MEDAL_STEAKTACULAR);
		}
	}
}
