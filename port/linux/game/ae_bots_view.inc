/*
AE_BOTS_VIEW.INC

(0.7.9) DEV MODE's view bots, its data, in bots.c's unit (included last by ae_bots_lan.inc, so it reads every other .inc's
state): which system chose each bot's goal (set by bots_choose_goal's AE hooks), and the board of a team's bots for
ae_view_bots.c: its squads (letter, leader, order or role, a note) and each bot's row (number, name, source, the goal in
words and metres, its state, its last WHY). Host only: a client has no bot brains. Nothing here is sent anywhere.
*/

/* which system chose each bot's goal (AE_VB_SOURCE_*); the stock brain's kind is the goal's own */
static short ae_bots_goal_sources[BOTS_BRAINS];

void ae_bots_goal_from(struct bots_bot *bot, short source)
{
	long brain = (long)(bot - bots_globals.bots);

	if (brain >= 0 && brain < BOTS_BRAINS)
		ae_bots_goal_sources[brain] = source;
}

static short ae_bots_goal_source_of(long brain)
{
	return brain >= 0 && brain < BOTS_BRAINS ? ae_bots_goal_sources[brain] : (short)0;
}

#define AE_BOTS_VIEW_DOT " \xC2\xB7 "

/* a player's name as text (ASCII; 11 letters, the game's cut) */
static void ae_bots_view_name(struct player_datum const *player, char *text, int size)
{
	int at;

	for (at = 0; at < size - 1 && at < 11 && player->name[at]; at++)
		text[at] = player->name[at] >= 32 && player->name[at] < 127 ? (char)player->name[at] : '?';
	text[at] = 0;
}

static void ae_bots_view_upper(char *text)
{
	for (; *text; text++)
		if (*text >= 'a' && *text <= 'z')
			*text = (char)(*text - 'a' + 'A');
}

static void ae_bots_view_lower(char *text)
{
	for (; *text; text++)
		if (*text >= 'A' && *text <= 'Z')
			*text = (char)(*text - 'A' + 'a');
}

/* the goal in words: what, then its distance in metres */
static void ae_bots_view_detail(struct bots_bot *bot, long brain, struct unit_datum const *unit, char *text, int size)
{
	char what[32];
	short source = ae_bots_goal_sources[brain];
	long metres = bot->goal_kind != _bots_goal_none ? ae_vb_metres(bots_distance3d(&bot->goal_point, &unit->object.position)) : -1;

	what[0] = 0;
	switch (source)
	{
	case AE_VB_SOURCE_VEHICLE:
	{
		struct ae_bots_vehicle_bot const *vehicle = &ae_bots_vehicle_bots[brain];

		if (unit->object.parent_object_index != NONE)
		{
			/* (seated: its seat, no walk) */
			snprintf(text, (size_t)size, "%s", ae_bots_vehicle_seat_name(vehicle->seat));
			return;
		}
		snprintf(what, sizeof(what), "to %s", vehicle->target_vehicle_index != NONE ? ae_bots_vehicle_seat_name(vehicle->target_seat) :
			"a vehicle");
		break;
	}
	case AE_VB_SOURCE_ZONE:
		snprintf(what, sizeof(what), "zone");
		break;
	case AE_VB_SOURCE_SQUAD:
	{
		long fireteam = ae_fireteam_of_brain(brain);
		short order = fireteam != NONE ? ae_fireteam_order_of(fireteam) : (short)_ae_order_none;

		snprintf(what, sizeof(what), "%s", order != _ae_order_none ? ae_vb_order_name(order) : "squad");
		ae_bots_view_lower(what);
		break;
	}
	case AE_VB_SOURCE_MARK:
		snprintf(what, sizeof(what), "%s", ae_vb_mark_name(ae_bots_mark_orders[brain].kind));
		ae_bots_view_lower(what);
		break;
	case AE_VB_SOURCE_OBJECTIVE:
		snprintf(what, sizeof(what), "%s", ae_bots_behaviour_name(ae_bots_objective_bots[brain].behaviour));
		break;
	case AE_VB_SOURCE_POWER:
	{
		short entry = ae_bots_power_bots[brain].entry;

		if (entry != NONE && entry >= 0 && entry < item_timers_count())
		{
			wchar_t const *label = item_timers_get(entry)->label;
			int at;

			for (at = 0; at < 23 && label[at]; at++)
				what[at] = label[at] >= 32 && label[at] < 127 ? (char)label[at] : '?';
			what[at] = 0;
			ae_bots_view_lower(what);
		}
		else
			snprintf(what, sizeof(what), "power item");
		break;
	}
	case AE_VB_SOURCE_VANTAGE:
		if (ae_vantage_bots[brain].spot != NONE)
			snprintf(what, sizeof(what), "spot %d", (int)ae_vantage_bots[brain].spot + 1);
		else
			snprintf(what, sizeof(what), "spot");
		break;
	case AE_VB_SOURCE_ROUTE:
	{
		int kind = ae_squads.bots[brain].kind;

		snprintf(what, sizeof(what), "%s", ae_squads.bots[brain].active && kind >= 0 && kind < AE_BOTS_ROUTES ?
			ae_squads_kind_names[kind] : "route");
		break;
	}
	case AE_VB_SOURCE_REVENGE:
	case AE_VB_SOURCE_BASE:
		if (bot->goal_kind == _bots_goal_enemy || source == AE_VB_SOURCE_REVENGE)
		{
			struct player_datum const *target = bot->target_player_index != NONE ? player_try_and_get(bot->target_player_index) :
				NULL;

			if (target)
				ae_bots_view_name(target, what, sizeof(what));
			else
				snprintf(what, sizeof(what), source == AE_VB_SOURCE_REVENGE ? "revenge" : "enemy");
		}
		else
			snprintf(what, sizeof(what), "%s", bot->goal_kind == _bots_goal_item ? "weapon" : "about");
		break;
	case AE_VB_SOURCE_TEAM:
		snprintf(what, sizeof(what), "team");
		break;
	default:
		break;
	}
	if (metres < 0)
		snprintf(text, (size_t)size, "%s", what[0] ? what : "no goal");
	else if (what[0])
		snprintf(text, (size_t)size, "%s" AE_BOTS_VIEW_DOT "%ld m", what, metres);
	else
		snprintf(text, (size_t)size, "%ld m", metres);
}

/* a squad's header and note */
static void ae_bots_view_squad(long fireteam, char letter, struct ae_vb_squad *squad)
{
	long leader = ae_fireteam_leader_player(fireteam);
	struct player_datum const *person = leader != NONE ? player_try_and_get(leader) : NULL;
	short order = ae_fireteam_order_of(fireteam);
	boolean explicit_order = ae_fireteam_order_explicit_of(fireteam);
	int role = ae_fireteam_role_taken(fireteam);
	char lead[16], said[24];

	if (person)
	{
		ae_bots_view_name(person, lead, sizeof(lead));
		ae_bots_view_upper(lead);
	}
	else
		snprintf(lead, sizeof(lead), "BOT-LED");
	if (!explicit_order && role != AE_ROLE_NONE && ae_vb_role_name(role)[0])
		snprintf(said, sizeof(said), "ROLE %s", ae_vb_role_name(role));
	else
		snprintf(said, sizeof(said), "%s", ae_vb_order_name(order));
	snprintf(squad->letter, sizeof(squad->letter), "%c", letter);
	snprintf(squad->head, sizeof(squad->head), "SQUAD %c" AE_BOTS_VIEW_DOT "%s" AE_BOTS_VIEW_DOT "%s", letter, lead, said);
	squad->note[0] = 0;
	if (order == _ae_order_follow)
		snprintf(squad->note, sizeof(squad->note), "leash %ld m", ae_vb_metres(ae_bots_fireteam_radius(fireteam)));
	else if (order == _ae_order_regroup)
	{
		long brain, members = 0, there = 0;

		for (brain = 0; brain < BOTS_BRAINS; brain++)
			if (ae_fireteam_of_brain(brain) == fireteam)
			{
				members++;
				there += ae_fireteam_brain_arrived_of(brain) ? 1 : 0;
			}
		snprintf(squad->note, sizeof(squad->note), "%ld of %ld there", there, members);
	}
	else if (order == _ae_order_none || !explicit_order)
	{
		if (order == _ae_order_none && ae_fireteam_arena_rules() && person)
			snprintf(squad->note, sizeof(squad->note), "loose leash %ld m", ae_vb_metres(AE_FIRETEAM_LOOSE_RADIUS));
		else if (order == _ae_order_none)
			snprintf(squad->note, sizeof(squad->note), "no order");
		else
		{
			snprintf(squad->note, sizeof(squad->note), "%s", ae_vb_order_name(order));
			ae_bots_view_lower(squad->note);
		}
	}
}

/* one bot's row */
static void ae_bots_view_row(long brain, struct player_datum const *player, long now, struct ae_vb_row *row)
{
	struct bots_bot *bot = &bots_globals.bots[brain];
	struct unit_datum const *unit = player->unit_index != NONE ?
		(struct unit_datum const *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;

	memset(row, 0, sizeof(*row));
	row->brain = (short)brain;
	row->player_index = bot->player_index;
	ae_bots_view_name(player, row->name, sizeof(row->name));
	row->why = ae_bots_why_last_text[brain];
	row->why_tick = ae_bots_why_last_tick[brain];
	if (row->why && (now < row->why_tick))
		row->why = NULL; /* (a WHY of an earlier game) */
	if (!unit)
	{
		long seconds = player->respawn_timer > 0 ? (player->respawn_timer + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND : 0;

		row->dead = 1;
		row->back_in = (short)(seconds > 999 ? 999 : seconds);
		return;
	}
	row->source = ae_bots_goal_sources[brain];
	row->goal_kind = bot->goal_kind;
	ae_bots_view_detail(bot, brain, unit, row->detail, sizeof(row->detail));
	if (bot->goal_kind != _bots_goal_none)
	{
		row->has_goal = 1;
		row->goal[0] = bot->goal_point.x;
		row->goal[1] = bot->goal_point.y;
		row->goal[2] = bot->goal_point.z;
	}
	{
		long fireteam = ae_fireteam_of_brain(brain);

		row->state = (short)ae_vb_state_pick(ae_vb_fighting(now, bot->combat.last_fight_tick), bot->stuck_count > 0,
			bot->goal_kind != _bots_goal_none && !bot->path.valid && bot->path_failures > 0,
			fireteam != NONE && unit->object.parent_object_index == NONE &&
			ae_bots_fireteam_tethered(bot, brain, bot->player_index, fireteam));
	}
}

boolean ae_bots_view_board(long team_index, struct ae_vb_board *board)
{
	long now = game_time_get();
	long brain, slot, slots = ae_fireteam_slot_count();
	char letter = 'A';
	short group, squad_of[BOTS_BRAINS];
	long group_slot[AE_VB_SQUADS];
	short groups = 0;

	memset(board, 0, sizeof(*board));
	board->team = (short)team_index;
	board->now = now;
	board->newest_brain = -1;
	if (network_game_distributed_client())
		return FALSE;
	if (team_index < 0)
		snprintf(board->team_name, sizeof(board->team_name), "ALL");
	else if (team_index == 0)
		snprintf(board->team_name, sizeof(board->team_name), "RED");
	else if (team_index == 1)
		snprintf(board->team_name, sizeof(board->team_name), "BLUE");
	else
		snprintf(board->team_name, sizeof(board->team_name), "TEAM %ld", team_index + 1);
	/* (the team's squads, in slot order: letters A, B, .. by rank among the team's slots, as the squads' own count) */
	for (slot = 0; slot < slots && groups < AE_VB_SQUADS - 1; slot++)
	{
		boolean any = FALSE;

		if (team_index < 0 || ae_fireteam_team_of(slot) != team_index)
			continue;
		for (brain = 0; brain < BOTS_BRAINS && !any; brain++)
			any = bots_globals.bots[brain].player_index != NONE && ae_fireteam_of_brain(brain) == slot;
		if (any)
		{
			ae_bots_view_squad(slot, letter, &board->squad[groups]);
			group_slot[groups++] = slot;
		}
		letter = (char)(letter < 'Z' ? letter + 1 : letter);
	}
	/* (each bot to its squad's group; the bots in none to a last group) */
	for (brain = 0; brain < BOTS_BRAINS; brain++)
	{
		struct player_datum const *player = bots_globals.bots[brain].player_index != NONE ?
			player_try_and_get(bots_globals.bots[brain].player_index) : NULL;
		long fireteam;

		squad_of[brain] = -1;
		if (!player || (team_index >= 0 && player->team_index != team_index))
			continue;
		board->count++;
		fireteam = ae_fireteam_of_brain(brain);
		for (group = 0; group < groups; group++)
			if (group_slot[group] == fireteam)
				squad_of[brain] = group;
		if (squad_of[brain] < 0)
			squad_of[brain] = groups; /* (the bots in no squad) */
	}
	{
		boolean loose = FALSE;

		for (brain = 0; brain < BOTS_BRAINS; brain++)
			loose |= squad_of[brain] == groups;
		if (loose)
		{
			snprintf(board->squad[groups].head, sizeof(board->squad[groups].head), groups ? "NO SQUAD" : "BOTS");
			groups++;
		}
	}
	board->squads = groups;
	/* (rows by group, then by number) */
	for (group = 0; group < groups; group++)
		for (brain = 0; brain < BOTS_BRAINS && board->rows < AE_VB_ROWS; brain++)
		{
			if (squad_of[brain] != group)
				continue;
			ae_bots_view_row(brain, player_get(bots_globals.bots[brain].player_index), now, &board->row[board->rows]);
			board->row[board->rows].squad = group;
			board->rows++;
			board->squad[group].rows++;
		}
	if (team_index >= 0 && team_index < AE_BOTS_WHY_TEAMS && ae_bots_why_newest[team_index].text &&
		now >= ae_bots_why_newest[team_index].tick)
	{
		board->newest_why = ae_bots_why_newest[team_index].text;
		board->newest_tick = ae_bots_why_newest[team_index].tick;
		board->newest_brain = (short)ae_bots_why_newest[team_index].brain;
	}
	else if (team_index < 0)
	{
		/* (no teams: the newest of any bot) */
		for (brain = 0; brain < BOTS_BRAINS; brain++)
			if (squad_of[brain] >= 0 && ae_bots_why_last_text[brain] && now >= ae_bots_why_last_tick[brain] &&
				(!board->newest_why || ae_bots_why_last_tick[brain] > board->newest_tick))
			{
				board->newest_why = ae_bots_why_last_text[brain];
				board->newest_tick = ae_bots_why_last_tick[brain];
				board->newest_brain = (short)brain;
			}
	}
	return TRUE;
}
