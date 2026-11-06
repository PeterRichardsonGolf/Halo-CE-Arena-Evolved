/*
AE_FIRETEAM_RULES.C

Pure rules for FIRETEAMS (ae_fireteam_rules.h). No engine includes.
*/

#include <math.h>
#include <stddef.h>

#include "ae_fireteam_rules.h"
#include "ae_wheel_rules.h"

static long order_serial_counter;

static void clear_order(struct ae_fireteam *fireteam)
{
	fireteam->order_serial = 0;
	fireteam->order_kind = _ae_order_none;
	fireteam->order_point[0] = fireteam->order_point[1] = fireteam->order_point[2] = 0.0f;
	fireteam->order_object = -1;
	fireteam->order_has_point = 0;
	fireteam->go_hold_start = -1.0f;
	fireteam->order_explicit = 0;
}

void ae_fireteam_clear(struct ae_fireteam_table *table)
{
	int index;

	table->count = 0;
	table->generation = 0;
	for (index = 0; index < AE_FIRETEAM_MAX; index++)
	{
		table->fireteams[index].role_slot = 0;
		table->fireteams[index].role = AE_ROLE_NONE;
		table->fireteams[index].team = -1;
		table->fireteams[index].leader = -1;
		table->fireteams[index].members = 0;
		clear_order(&table->fireteams[index]);
	}
	for (index = 0; index < AE_FIRETEAM_BRAINS; index++)
	{
		table->brain_fireteam[index] = -1;
		table->brain_arrived[index] = 0;
	}
}

static int used(struct ae_fireteam_table const *table, int fireteam)
{
	return fireteam >= 0 && fireteam < table->count && table->fireteams[fireteam].team != -1;
}

static void release(struct ae_fireteam_table *table, int fireteam)
{
	int brain;

	for (brain = 0; brain < AE_FIRETEAM_BRAINS; brain++)
		if (table->brain_fireteam[brain] == fireteam)
			table->brain_fireteam[brain] = -1;
	table->fireteams[fireteam].team = -1;
	table->fireteams[fireteam].leader = -1;
	table->fireteams[fireteam].members = 0;
	table->fireteams[fireteam].role_slot = 0;
	table->fireteams[fireteam].role = AE_ROLE_NONE;
	clear_order(&table->fireteams[fireteam]);
}

static int allocate(struct ae_fireteam_table *table, short team, long leader)
{
	int index;

	for (index = 0; index < table->count; index++)
		if (table->fireteams[index].team == -1)
			break;
	if (index == table->count)
	{
		if (table->count >= AE_FIRETEAM_MAX)
			return -1;
		table->count++;
	}
	table->fireteams[index].team = team;
	table->fireteams[index].leader = leader;
	table->fireteams[index].members = 0;
	table->fireteams[index].role_slot = 0;
	table->fireteams[index].role = AE_ROLE_NONE;
	clear_order(&table->fireteams[index]);
	return index;
}

static void assign(struct ae_fireteam_table *table, short brain, int fireteam)
{
	if (brain < 0 || brain >= AE_FIRETEAM_BRAINS || table->brain_fireteam[brain] != -1)
		return;
	table->brain_fireteam[brain] = (short)fireteam;
	table->fireteams[fireteam].members++;
}

static void unassign(struct ae_fireteam_table *table, short brain)
{
	int fireteam = table->brain_fireteam[brain];

	if (fireteam >= 0)
		table->fireteams[fireteam].members--;
	table->brain_fireteam[brain] = -1;
}

static int smallest_of_team(struct ae_fireteam_table const *table, short team, int skip, int only_bot_led)
{
	int index, best = -1;

	for (index = 0; index < table->count; index++)
	{
		if (index == skip || table->fireteams[index].team != team)
			continue;
		if (only_bot_led && table->fireteams[index].leader != -1)
			continue;
		if (best == -1 || table->fireteams[index].members < table->fireteams[best].members)
			best = index;
	}
	return best;
}

void ae_fireteam_split(struct ae_fireteam_table *table, short team, long const *leader_players, short leaders,
	short const *bot_brains, short bots)
{
	int index;
	short taken = 0, left, squads, size, extra, share, remainder;
	int last = -1;
	struct ae_fireteam kept[AE_FIRETEAM_MAX];
	int kept_count = 0;

	table->generation++;
	/* a person's fireteam keeps its order through the rebuild (found again by its leader) */
	for (index = 0; index < table->count; index++)
		if (table->fireteams[index].team == team)
		{
			if (table->fireteams[index].leader != -1 && table->fireteams[index].order_kind != _ae_order_none)
				kept[kept_count++] = table->fireteams[index];
			release(table, index);
		}
	for (index = 0; index < bots; index++)
		if (bot_brains[index] >= 0 && bot_brains[index] < AE_FIRETEAM_BRAINS)
			unassign(table, bot_brains[index]);
	if (leaders < 0 || !leader_players)
		leaders = 0;
	if (bots < 0 || !bot_brains)
		bots = 0;

	share = leaders > 0 ? (short)(bots / leaders) : 0;
	remainder = leaders > 0 ? (short)(bots % leaders) : 0;
	for (index = 0; index < leaders; index++)
	{
		int fireteam = allocate(table, team, leader_players[index]);
		short want = (short)(share + (index < remainder ? 1 : 0));

		if (fireteam == -1)
			break;
		{
			int k;

			for (k = 0; k < kept_count; k++)
				if (kept[k].leader == leader_players[index])
				{
					table->fireteams[fireteam].order_kind = kept[k].order_kind;
					table->fireteams[fireteam].order_explicit = kept[k].order_explicit;
					table->fireteams[fireteam].order_serial = kept[k].order_serial;
					table->fireteams[fireteam].order_object = kept[k].order_object;
					table->fireteams[fireteam].order_has_point = kept[k].order_has_point;
					table->fireteams[fireteam].order_point[0] = kept[k].order_point[0];
					table->fireteams[fireteam].order_point[1] = kept[k].order_point[1];
					table->fireteams[fireteam].order_point[2] = kept[k].order_point[2];
				}
		}
		last = fireteam;
		if (want > AE_FIRETEAM_CAP)
			want = AE_FIRETEAM_CAP;
		while (want-- > 0 && taken < bots)
			assign(table, bot_brains[taken++], fireteam);
	}

	left = (short)(bots - taken);
	if (left <= 0)
		return;
	if (left < AE_FIRETEAM_SQUAD_MIN && last != -1)
	{
		/* too few for a squad: they join the team's smallest fireteam */
		int into = smallest_of_team(table, team, -1, 0);

		while (taken < bots)
			assign(table, bot_brains[taken++], into);
		return;
	}
	squads = (short)((left + AE_FIRETEAM_CAP - 1) / AE_FIRETEAM_CAP);
	size = (short)(left / squads);
	extra = (short)(left % squads);
	for (index = 0; index < squads; index++)
	{
		int fireteam = allocate(table, team, -1);
		short want = (short)(size + (index < extra ? 1 : 0));

		if (fireteam == -1)
		{
			/* the table is full: the rest go to the last fireteam of the team */
			int into = smallest_of_team(table, team, -1, 0);

			while (into != -1 && taken < bots)
				assign(table, bot_brains[taken++], into);
			return;
		}
		while (want-- > 0 && taken < bots)
			assign(table, bot_brains[taken++], fireteam);
	}
}

int ae_fireteam_of_bot(struct ae_fireteam_table const *table, short brain)
{
	if (brain < 0 || brain >= AE_FIRETEAM_BRAINS)
		return -1;
	return table->brain_fireteam[brain];
}

int ae_fireteam_recruit(struct ae_fireteam_table *table, short brain, int to_fireteam)
{
	int from = ae_fireteam_of_bot(table, brain);

	if (from < 0 || !used(table, to_fireteam) || from == to_fireteam)
		return 0;
	if (table->fireteams[from].team != table->fireteams[to_fireteam].team)
		return 0;
	if (table->fireteams[from].members <= 1)
		return 0;
	table->fireteams[from].members--;
	table->fireteams[to_fireteam].members++;
	table->brain_fireteam[brain] = (short)to_fireteam;
	return 1;
}

int ae_fireteam_recruit_check(struct ae_fireteam_table const *table, short brain, int to_fireteam, int carrying_objective,
	int seated_driver)
{
	int from = brain >= 0 && brain < AE_FIRETEAM_BRAINS ? ae_fireteam_of_bot(table, brain) : -1;

	if (from < 0 || !used(table, to_fireteam) || table->fireteams[from].team != table->fireteams[to_fireteam].team)
		return AE_RECRUIT_NONE;
	if (from == to_fireteam)
		return AE_RECRUIT_SAME;
	if (carrying_objective)
		return AE_RECRUIT_OBJECTIVE;
	if (seated_driver)
		return AE_RECRUIT_SEAT;
	return table->fireteams[from].members <= 1 ? AE_RECRUIT_LAST : AE_RECRUIT_OK;
}

int ae_fireteam_smart_resolve(int on_friendly_bot, int bot_in_own_fireteam, int on_ground, float distance_to_caller)
{
	if (on_friendly_bot)
		return bot_in_own_fireteam ? AE_SMART_FOLLOW : AE_SMART_RECRUIT;
	return on_ground && distance_to_caller <= AE_FIRETEAM_SMART_GROUND_RADIUS ? AE_SMART_FOLLOW : AE_SMART_PLAIN;
}

short ae_fireteam_smart_order(int resolved)
{
	return resolved == AE_SMART_FOLLOW ? (short)_ae_order_follow : resolved == AE_SMART_PLAIN ? (short)_ae_order_go : (short)_ae_order_none;
}

short ae_fireteam_call_order(int fireteam, int wheel_entry)
{
	if (fireteam < 0)
		return _ae_order_none;
	switch (wheel_entry)
	{
	case AE_WHEEL_ATTACK: return _ae_order_attack;
	case AE_WHEEL_DEFEND: return _ae_order_defend;
	case AE_WHEEL_WATCH: return _ae_order_watch;
	case AE_WHEEL_GOING: return _ae_order_go;
	case AE_WHEEL_REGROUP: return _ae_order_regroup;
	}
	return _ae_order_none;
}

int ae_fireteam_call_smart(int fireteam, int wheel_entry, int mark_is_place)
{
	return fireteam >= 0 && wheel_entry == AE_WHEEL_SMART && mark_is_place;
}

int ae_fireteam_call_shares(int wheel_entry)
{
	return wheel_entry != AE_WHEEL_REGROUP;
}

int ae_fireteam_role_table(int mode, short *roles)
{
	static short const ctf[] = { AE_ROLE_DEFEND_OURS, AE_ROLE_PUSH_THEIRS, AE_ROLE_ROVE };
	static short const snd_defenders[] = { AE_ROLE_SITE_A, AE_ROLE_SITE_B, AE_ROLE_ROVE };
	static short const snd_attackers[] = { AE_ROLE_ENTRY, AE_ROLE_ROVE, AE_ROLE_ENTRY };
	static short const king[] = { AE_ROLE_HILL, AE_ROLE_APPROACH };
	static short const oddball[] = { AE_ROLE_PROTECT, AE_ROLE_HUNT };
	static short const slayer[] = { AE_ROLE_POWER, AE_ROLE_ROAM };
	short const *list = NULL;
	int count = 0, index;

	switch (mode)
	{
	case AE_ROLE_MODE_CTF: list = ctf; count = 3; break;
	case AE_ROLE_MODE_SND_DEFENDERS: list = snd_defenders; count = 3; break;
	case AE_ROLE_MODE_SND_ATTACKERS: list = snd_attackers; count = 3; break;
	case AE_ROLE_MODE_KING: list = king; count = 2; break;
	case AE_ROLE_MODE_ODDBALL: list = oddball; count = 2; break;
	case AE_ROLE_MODE_SLAYER: list = slayer; count = 2; break;
	}
	for (index = 0; roles && index < count; index++)
		roles[index] = list[index];
	return count;
}

short ae_fireteam_role_order_kind(int role)
{
	switch (role)
	{
	case AE_ROLE_DEFEND_OURS: case AE_ROLE_SITE_A: case AE_ROLE_HILL: case AE_ROLE_PROTECT: return _ae_order_defend;
	case AE_ROLE_PUSH_THEIRS: case AE_ROLE_ENTRY: case AE_ROLE_APPROACH: case AE_ROLE_HUNT: return _ae_order_attack;
	case AE_ROLE_SITE_B: return _ae_order_watch;
	}
	/* (rove, power items and roam: the bots' own goals, the escort and the item timers, already are these) */
	return _ae_order_none;
}

int ae_fireteam_role_mix(int mode, int squads, short *roles)
{
	short table_roles[AE_ROLE_TABLE_MAX];
	int count = ae_fireteam_role_table(mode, table_roles), index;

	if (count <= 0 || squads <= 0)
		return 0;
	if (squads > AE_FIRETEAM_MAX)
		squads = AE_FIRETEAM_MAX;
	for (index = 0; index < squads; index++)
		roles[index] = table_roles[index % count];
	return squads;
}

/* the team's bot-led squads, in table order; their number */
static int team_bot_squads(struct ae_fireteam_table const *table, short team, int *list)
{
	int index, count = 0;

	for (index = 0; index < table->count; index++)
		if (table->fireteams[index].team == team && table->fireteams[index].leader == -1)
			list[count++] = index;
	return count;
}

int ae_fireteam_bot_squads(struct ae_fireteam_table const *table, short team)
{
	int list[AE_FIRETEAM_MAX];

	return team_bot_squads(table, team, list);
}

/* the squads' places in the mix are kept only while they are a whole set: each 1..N once (a squad gone or added since
a wipe starts the places over, by rank) */
static int slots_whole(struct ae_fireteam_table const *table, int const *list, int count)
{
	char seen[AE_FIRETEAM_MAX + 1] = { 0 };
	int index;

	for (index = 0; index < count; index++)
	{
		int slot = table->fireteams[list[index]].role_slot;

		if (slot < 1 || slot > count || seen[slot])
			return 0;
		seen[slot] = 1;
	}
	return 1;
}

int ae_fireteam_role_of(struct ae_fireteam_table const *table, int fireteam, int mode)
{
	short roles[AE_FIRETEAM_MAX];
	int list[AE_FIRETEAM_MAX];
	int count, rank, slot;

	if (!used(table, fireteam))
		return AE_ROLE_NONE;
	/* a human leads this squad: it takes the leader's calls, not a role */
	if (table->fireteams[fireteam].leader != -1)
		return AE_ROLE_NONE;
	count = team_bot_squads(table, table->fireteams[fireteam].team, list);
	if (ae_fireteam_role_mix(mode, count, roles) <= 0)
		return AE_ROLE_NONE;
	for (rank = 0; rank < count && list[rank] != fireteam; rank++)
		;
	if (rank >= count)
		return AE_ROLE_NONE;
	slot = slots_whole(table, list, count) ? table->fireteams[fireteam].role_slot - 1 : rank;
	return roles[slot];
}

void ae_fireteam_role_wiped(struct ae_fireteam_table *table, int fireteam)
{
	int list[AE_FIRETEAM_MAX];
	int count, rank, other, index;
	short held;

	if (!used(table, fireteam) || table->fireteams[fireteam].leader != -1)
		return;
	count = team_bot_squads(table, table->fireteams[fireteam].team, list);
	if (count < 2)
		return;
	if (!slots_whole(table, list, count))
		for (index = 0; index < count; index++)
			table->fireteams[list[index]].role_slot = (short)(index + 1);
	for (rank = 0; list[rank] != fireteam; rank++)
		;
	other = list[(rank + 1) % count];
	held = table->fireteams[fireteam].role_slot;
	table->fireteams[fireteam].role_slot = table->fireteams[other].role_slot;
	table->fireteams[other].role_slot = held;
}

int ae_fireteam_role_wants_order(struct ae_fireteam_table *table, int fireteam, int leader_alive, int flipped)
{
	struct ae_fireteam *squad;

	if (!used(table, fireteam))
		return 0;
	squad = &table->fireteams[fireteam];
	/* a human-led squad never takes a role: its order is its leader's last call (or FOLLOW) whatever the objective does */
	if (squad->leader != -1)
		return 0;
	(void)leader_alive;
	(void)flipped;
	if (squad->order_kind == _ae_order_none)
		return 1;
	if (!squad->order_explicit)
		return 1;
	return 0;
}

void ae_fireteam_set_role_order(struct ae_fireteam_table *table, int fireteam, int role, float const *point)
{
	short kind = ae_fireteam_role_order_kind(role);

	if (!used(table, fireteam))
		return;
	table->fireteams[fireteam].role = (short)role;
	if (kind == _ae_order_none || !point)
	{
		clear_order(&table->fireteams[fireteam]);
		return;
	}
	ae_fireteam_set_order(table, fireteam, kind, point, -1);
	table->fireteams[fireteam].order_explicit = 0;
}

int ae_fireteam_order_is_explicit(struct ae_fireteam_table const *table, int fireteam)
{
	return used(table, fireteam) && table->fireteams[fireteam].order_explicit;
}

int ae_fireteam_role(struct ae_fireteam_table const *table, int fireteam)
{
	return used(table, fireteam) ? table->fireteams[fireteam].role : AE_ROLE_NONE;
}

long ae_fireteam_leader(struct ae_fireteam_table const *table, int fireteam)
{
	return used(table, fireteam) ? table->fireteams[fireteam].leader : -1;
}

short ae_fireteam_lead_brain(struct ae_fireteam_table const *table, int fireteam)
{
	int brain;

	for (brain = 0; brain < AE_FIRETEAM_BRAINS; brain++)
		if (fireteam >= 0 && table->brain_fireteam[brain] == fireteam)
			return (short)brain;
	return -1;
}

void ae_fireteam_leader_left(struct ae_fireteam_table *table, long player)
{
	int index;

	if (player < 0)
		return;
	for (index = 0; index < table->count; index++)
	{
		int into, brain;

		if (table->fireteams[index].team == -1 || table->fireteams[index].leader != player)
			continue;
		table->fireteams[index].leader = -1;
		into = smallest_of_team(table, table->fireteams[index].team, index, 1);
		if (into == -1)
			into = smallest_of_team(table, table->fireteams[index].team, index, 0);
		if (into == -1)
			continue;
		for (brain = 0; brain < AE_FIRETEAM_BRAINS; brain++)
		{
			if (table->brain_fireteam[brain] == index)
			{
				table->brain_fireteam[brain] = (short)into;
				table->fireteams[into].members++;
			}
		}
		table->fireteams[index].team = -1;
		table->fireteams[index].members = 0;
	}
}

int ae_fireteam_check(struct ae_fireteam_table const *table)
{
	short counts[AE_FIRETEAM_MAX] = { 0 };
	int index;

	if (table->count < 0 || table->count > AE_FIRETEAM_MAX)
		return 0;
	for (index = 0; index < AE_FIRETEAM_BRAINS; index++)
	{
		int fireteam = table->brain_fireteam[index];

		if (fireteam == -1)
			continue;
		if (!used(table, fireteam))
			return 0;
		counts[fireteam]++;
	}
	for (index = 0; index < table->count; index++)
		if (counts[index] != table->fireteams[index].members)
			return 0;
	return 1;
}

void ae_fireteam_set_order(struct ae_fireteam_table *table, int fireteam, short kind, float const *point, long object)
{
	int brain;

	if (!used(table, fireteam) || kind < _ae_order_none || kind >= AE_ORDER_COUNT)
		return;
	clear_order(&table->fireteams[fireteam]);
	table->fireteams[fireteam].order_serial = ++order_serial_counter;
	table->fireteams[fireteam].order_kind = kind;
	table->fireteams[fireteam].order_explicit = kind != _ae_order_none;
	table->fireteams[fireteam].order_object = object;
	if (point)
	{
		table->fireteams[fireteam].order_point[0] = point[0];
		table->fireteams[fireteam].order_point[1] = point[1];
		table->fireteams[fireteam].order_point[2] = point[2];
		table->fireteams[fireteam].order_has_point = 1;
	}
	for (brain = 0; brain < AE_FIRETEAM_BRAINS; brain++)
		if (table->brain_fireteam[brain] == fireteam)
			table->brain_arrived[brain] = 0;
}

short ae_fireteam_order_kind(struct ae_fireteam_table const *table, int fireteam)
{
	return used(table, fireteam) ? table->fireteams[fireteam].order_kind : _ae_order_none;
}

int ae_fireteam_order_point(struct ae_fireteam_table const *table, int fireteam, float *point)
{
	if (!used(table, fireteam) || !table->fireteams[fireteam].order_has_point)
		return 0;
	point[0] = table->fireteams[fireteam].order_point[0];
	point[1] = table->fireteams[fireteam].order_point[1];
	point[2] = table->fireteams[fireteam].order_point[2];
	return 1;
}

long ae_fireteam_order_serial(struct ae_fireteam_table const *table, int fireteam)
{
	return used(table, fireteam) ? table->fireteams[fireteam].order_serial : 0;
}

long ae_fireteam_order_object(struct ae_fireteam_table const *table, int fireteam)
{
	return used(table, fireteam) ? table->fireteams[fireteam].order_object : -1;
}

int ae_fireteam_follow_wanted(float distance_to_leader)
{
	return distance_to_leader > AE_FIRETEAM_LEASH;
}

int ae_fireteam_regroup_note(struct ae_fireteam_table *table, int fireteam, short brain, float distance)
{
	int index, all = 1;

	if (!used(table, fireteam) || table->fireteams[fireteam].order_kind != _ae_order_regroup)
		return 0;
	if (brain >= 0 && brain < AE_FIRETEAM_BRAINS && table->brain_fireteam[brain] == fireteam &&
		distance < AE_FIRETEAM_REFORM_RADIUS)
		table->brain_arrived[brain] = 1;
	for (index = 0; index < AE_FIRETEAM_BRAINS; index++)
		if (table->brain_fireteam[index] == fireteam && !table->brain_arrived[index])
			all = 0;
	if (!all)
		return 0;
	clear_order(&table->fireteams[fireteam]);
	return 1;
}

int ae_fireteam_brain_arrived(struct ae_fireteam_table const *table, short brain)
{
	return brain >= 0 && brain < AE_FIRETEAM_BRAINS && table->brain_arrived[brain];
}

int ae_fireteam_gave_up(short kind, int able_members, float stalled_seconds)
{
	if (kind != _ae_order_regroup && kind != _ae_order_go)
		return 0;
	return able_members <= 0 && stalled_seconds >= AE_FIRETEAM_STALL_SECONDS;
}

int ae_fireteam_order_holds(short kind)
{
	return kind == _ae_order_defend || kind == _ae_order_watch;
}

int ae_fireteam_press_wanted(short kind, float position_distance, float goal_distance)
{
	switch (kind)
	{
	case _ae_order_attack:
		return position_distance > AE_FIRETEAM_LEASH * AE_FIRETEAM_ATTACK_DRIFT;
	case _ae_order_defend:
	case _ae_order_watch:
		return position_distance > AE_FIRETEAM_ANCHOR_RADIUS || goal_distance > AE_FIRETEAM_ANCHOR_RADIUS;
	case _ae_order_go:
		return position_distance > AE_FIRETEAM_REFORM_RADIUS || goal_distance > AE_FIRETEAM_REFORM_RADIUS;
	default:
		return 0;
	}
}

int ae_fireteam_go_note(struct ae_fireteam_table *table, int fireteam, short brain, float distance, float now_seconds)
{
	struct ae_fireteam *team;

	if (!used(table, fireteam) || table->fireteams[fireteam].order_kind != _ae_order_go)
		return 0;
	team = &table->fireteams[fireteam];
	if (brain >= 0 && brain < AE_FIRETEAM_BRAINS && table->brain_fireteam[brain] == fireteam &&
		distance < AE_FIRETEAM_REFORM_RADIUS && team->go_hold_start < 0.0f)
		team->go_hold_start = now_seconds;
	if (team->go_hold_start < 0.0f || now_seconds - team->go_hold_start < AE_FIRETEAM_GO_HOLD_SECONDS)
		return 0;
	clear_order(team);
	return 1;
}

void ae_fireteam_formation_slot(int index, float facing, float *dx, float *dy)
{
	int rank = index / 2 + 1;
	float side = (index & 1) ? 1.0f : -1.0f;
	float lateral = side * AE_FIRETEAM_FORMATION_SIDE * (float)rank, back = -AE_FIRETEAM_FORMATION_BACK * (float)rank;
	float forward_x = cosf(facing), forward_y = sinf(facing);

	if (index < 0)
		lateral = back = 0.0f;
	/* (forward is the facing; left of it is +90 degrees) */
	*dx = forward_x * back - forward_y * lateral;
	*dy = forward_y * back + forward_x * lateral;
}

float ae_fireteam_formation_facing(float held, float now_facing, int first)
{
	float turn = now_facing - held;

	while (turn > 3.14159265f)
		turn -= 6.2831853f;
	while (turn < -3.14159265f)
		turn += 6.2831853f;
	return first || fabsf(turn) > AE_FIRETEAM_RESLOT_TURN ? now_facing : held;
}

int ae_fireteam_tether(float leader_distance, float slot_distance, int fighting, float outward_cosine, float tolerance)
{
	if (leader_distance > AE_FIRETEAM_FOLLOW_RADIUS)
		return AE_TETHER_GO;
	if (fighting)
		return leader_distance > AE_FIRETEAM_FOLLOW_RADIUS - AE_FIRETEAM_FOLLOW_EDGE && outward_cosine < 1.5f &&
			outward_cosine > 0.2f ? AE_TETHER_STOP : AE_TETHER_FREE;
	return slot_distance > tolerance ? AE_TETHER_GO : AE_TETHER_HOLD;
}

int ae_fireteam_settled(short kind, float position_distance, int mates_near)
{
	if (kind != _ae_order_defend && kind != _ae_order_watch)
		return 0;
	return position_distance <= AE_FIRETEAM_SETTLE_RADIUS ||
		(position_distance <= AE_FIRETEAM_ANCHOR_RADIUS && mates_near >= AE_FIRETEAM_SETTLE_CROWD);
}
