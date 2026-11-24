/*
AE_BOTS_OBJLOG.INC

The bots' part of the objective log (ae_objective_log.h), in bots.c's unit (included last by ae_bots_lan.inc, so it
reads every other .inc's state): a bot's line each second at the "full" level (ev=ps, with its job, role, goal and why it
is doing what it does), ev=job_change when its behaviour changes (with the utilities that chose it and what forced the
choice), ev=obj_blocked when it works at its objective and does not come nearer, and the words appended to ev=stuck.
*/

#define AE_BOTS_OBJLOG_BLOCKED_TICKS (5 * TICKS_PER_SECOND)
#define AE_BOTS_OBJLOG_STEP_TICKS (TICKS_PER_SECOND / 2)
/* a bot that came this much nearer its objective moved */
#define AE_BOTS_OBJLOG_PROGRESS 1.5

/* what forced a choice of the objective's utilities (the carrier's own rules) */
enum { AE_BOTS_OBJLOG_FORCED_NONE, AE_BOTS_OBJLOG_FORCED_CARRIER_DROP, AE_BOTS_OBJLOG_FORCED_CARRIER_WAIT,
	AE_BOTS_OBJLOG_FORCED_CARRIER_RECOVER };

static struct
{
	long player_index;
	long next_ps_tick;
	long next_step_tick;
	double best;
	long best_tick;
	long logged_tick;
	short behaviour;
} ae_bots_objlog_bots[BOTS_BRAINS];

static char const *ae_bots_objlog_goal_name(short kind)
{
	switch (kind)
	{
	case _bots_goal_enemy:
		return "enemy";
	case _bots_goal_roam:
		return "roam";
	case _bots_goal_item:
		return "item";
	default:
		return "none";
	}
}

static char const *ae_bots_objlog_order_name(long fireteam)
{
	static char const *const names[] = { "none", "follow", "regroup", "attack", "defend", "watch", "go" };
	short kind = fireteam != NONE ? ae_fireteam_order_of(fireteam) : (short)_ae_order_none;

	return kind >= 0 && kind < (short)(sizeof(names) / sizeof(names[0])) ? names[kind] : "none";
}

/* the part the bot plays in the mode, from what it does (SEARCH AND DESTROY: this round's role) */
static char const *ae_bots_objlog_role(long bot_index, short mode, short behaviour)
{
	static char const *const snd_roles[] = { "none", "carrier", "entry", "support", "lurker", "anchor", "holder", "forward" };

	if (mode == AE_BOTS_MODE_BOMB && ae_bomb_snd_on())
	{
		short role = ae_bots_objective_bots[bot_index].snd_role;

		if (role > 0 && role < (short)(sizeof(snd_roles) / sizeof(snd_roles[0])))
			return snd_roles[role];
	}
	switch (behaviour)
	{
	case AE_BOTS_CTF_TAKE:
	case AE_BOTS_BALL_TAKE:
	case AE_BOTS_BOMB_TAKE:
		return "taker";
	case AE_BOTS_CTF_CARRY:
	case AE_BOTS_BOMB_CARRY:
		return "carrier";
	case AE_BOTS_CTF_RETURN:
		return "returner";
	case AE_BOTS_CTF_DEFEND:
	case AE_BOTS_BOMB_DEFEND:
	case AE_BOTS_BOMB_SAVE:
		return "defender";
	case AE_BOTS_ESCORT:
		return "escort";
	case AE_BOTS_BALL_HOLD:
	case AE_BOTS_HILL_HOLD:
		return "holder";
	case AE_BOTS_BALL_CHASE:
	case AE_BOTS_JUGG_HUNT:
		return "hunter";
	case AE_BOTS_HILL_GO:
		return "goer";
	case AE_BOTS_JUGG_FLEE:
		return "juggernaut";
	case AE_BOTS_RACE_NEXT:
		return "racer";
	default:
		return "rover";
	}
}

static boolean ae_bots_objlog_tethered(struct bots_bot *bot, long bot_index)
{
	long fireteam = ae_fireteam_of_brain(bot_index);

	return fireteam != NONE && ae_bots_fireteam_tethered(bot, bot_index, bot->player_index, fireteam);
}

/* the words of the bot's line (ev=ps) */
static void ae_bots_objlog_words(struct bots_bot *bot, long bot_index, boolean fighting, struct ae_obj_ps_words *words)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	short behaviour = ae_bots_objective_bots[bot_index].behaviour;

	words->job = ae_bots_behaviour_name(behaviour);
	words->role = snapshot ? ae_bots_objlog_role(bot_index, snapshot->mode, behaviour) : "rover";
	words->goal = ae_bots_objlog_goal_name(bot->goal_kind);
	words->fireteam = ae_fireteam_of_brain(bot_index);
	words->order = ae_bots_objlog_order_name(words->fireteam);
	words->state = ae_bots_objlog_tethered(bot, bot_index) ? "tethered" : fighting ? "fighting" : bot->stuck_count > 0 ? "stuck" : NULL;
	words->seat = "none";
}

/* (ev=stuck's added words) the bot's team, the mode, its job, goal, role, the distances to its goal and objective, whether
it fights, whether it is tethered */
static void ae_bots_objlog_stuck_words(struct bots_bot *bot, long bot_index, struct bots_tick *tick, char *text, size_t size)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	short behaviour = ae_bots_objective_bots[bot_index].behaviour;
	char const *objective;
	double d_obj;
	double d_goal = bot->goal_kind != _bots_goal_none ? bots_distance3d(&bot->goal_point, &tick->position) : -1.0;

	ae_obj_objective_of(bot->player_index, &objective, &d_obj);
	snprintf(text, size, " team=%ld mode=%s job=%s goal_name=%s role=%s d_goal=%.1f d_obj=%.1f fighting=%d tether=%d",
		(long)tick->player->team_index, ae_obj_mode_word(), ae_bots_behaviour_name(behaviour),
		ae_bots_objlog_goal_name(bot->goal_kind), snapshot ? ae_bots_objlog_role(bot_index, snapshot->mode, behaviour) : "rover",
		d_goal, d_obj, tick->fighting ? 1 : 0, ae_bots_objlog_tethered(bot, bot_index) ? 1 : 0);
}

/* (ae_travel_stuck_line, each stuck report) the objective log's own line at the events level: where, how often, what the
bot was doing */
static void ae_bots_objlog_stuck_event(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	struct ae_bots_objective_snapshot const *snapshot;
	struct ae_obj_fields fields;
	char const *objective;
	double d_obj;
	short behaviour;

	if (ae_obj_level() < AE_OBJ_EVENTS || bot_index < 0 || bot_index >= BOTS_BRAINS || network_game_distributed_client())
		return;
	snapshot = ae_bots_objective_snapshot();
	behaviour = ae_bots_objective_bots[bot_index].behaviour;
	ae_obj_objective_of(bot->player_index, &objective, &d_obj);
	ae_obj_fields_begin(&fields);
	ae_obj_who(&fields, bot->player_index);
	ae_obj_fields_long(&fields, "count", (long)bot->stuck_count);
	ae_obj_fields_text(&fields, "job", ae_bots_behaviour_name(behaviour));
	ae_obj_fields_text(&fields, "goal_name", ae_bots_objlog_goal_name(bot->goal_kind));
	ae_obj_fields_text(&fields, "role", snapshot ? ae_bots_objlog_role(bot_index, snapshot->mode, behaviour) : "rover");
	ae_obj_fields_real(&fields, "d_goal", bot->goal_kind != _bots_goal_none ? bots_distance3d(&bot->goal_point, &tick->position) : -1.0);
	ae_obj_fields_real(&fields, "d_obj", d_obj);
	ae_obj_fields_long(&fields, "fighting", tick->fighting ? 1 : 0);
	ae_obj_fields_long(&fields, "tether", ae_bots_objlog_tethered(bot, bot_index) ? 1 : 0);
	ae_obj_emit("stuck", &fields);
}

/* (ae_bots_events_end, each living bot's tick) the line each second, and the objective it does not come nearer */
static void ae_bots_objlog_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	int level = ae_obj_level();
	long now = tick->now;
	short behaviour;

	if (level < AE_OBJ_EVENTS || bot_index < 0 || bot_index >= BOTS_BRAINS || network_game_distributed_client() ||
		!game_engine_running() || game_engine_game_over())
	{
		return;
	}
	if (ae_bots_objlog_bots[bot_index].player_index != bot->player_index)
	{
		memset(&ae_bots_objlog_bots[bot_index], 0, sizeof(ae_bots_objlog_bots[bot_index]));
		ae_bots_objlog_bots[bot_index].player_index = bot->player_index;
		ae_bots_objlog_bots[bot_index].best = -1.0;
		ae_bots_objlog_bots[bot_index].best_tick = now;
		ae_bots_objlog_bots[bot_index].logged_tick = now;
	}
	if (level >= AE_OBJ_FULL && now >= ae_bots_objlog_bots[bot_index].next_ps_tick)
	{
		struct ae_obj_ps_words words;

		ae_bots_objlog_bots[bot_index].next_ps_tick = now + TICKS_PER_SECOND;
		ae_bots_objlog_words(bot, bot_index, tick->fighting, &words);
		if (tick->unit && tick->unit->object.parent_object_index != NONE)
			words.seat = ae_bots_vehicle_seat_name(ae_bots_vehicle_bots[bot_index].seat);
		ae_obj_ps(bot->player_index, &words);
	}
	if (now < ae_bots_objlog_bots[bot_index].next_step_tick)
		return;
	ae_bots_objlog_bots[bot_index].next_step_tick = now + AE_BOTS_OBJLOG_STEP_TICKS;
	behaviour = ae_bots_objective_bots[bot_index].behaviour;
	if (behaviour == AE_BOTS_FIGHT)
	{
		ae_bots_objlog_bots[bot_index].best = -1.0;
		ae_bots_objlog_bots[bot_index].best_tick = now;
		return;
	}
	{
		char const *objective;
		double d_obj;

		ae_obj_objective_of(bot->player_index, &objective, &d_obj);
		if (d_obj < 0.0)
			return;
		if (behaviour != ae_bots_objlog_bots[bot_index].behaviour || ae_bots_objlog_bots[bot_index].best < 0.0 ||
			d_obj < ae_bots_objlog_bots[bot_index].best - AE_BOTS_OBJLOG_PROGRESS)
		{
			ae_bots_objlog_bots[bot_index].behaviour = behaviour;
			ae_bots_objlog_bots[bot_index].best = d_obj;
			ae_bots_objlog_bots[bot_index].best_tick = now;
			return;
		}
		/* (standing at the objective holding it is not blocked: the hill, a defended stand) */
		if (d_obj < 4.0 || now - ae_bots_objlog_bots[bot_index].best_tick < AE_BOTS_OBJLOG_BLOCKED_TICKS ||
			now - ae_bots_objlog_bots[bot_index].logged_tick < AE_BOTS_OBJLOG_BLOCKED_TICKS)
		{
			return;
		}
		{
			struct ae_obj_fields fields;
			char const *reason = ae_bots_objlog_tethered(bot, bot_index) ? "tethered" : tick->fighting ? "fighting" :
				tick->unit && tick->unit->object.parent_object_index != NONE ? "seated" : bot->stuck_count > 0 ? "stuck" :
				!bot->path.valid ? "no_path" : "walking";

			ae_bots_objlog_bots[bot_index].logged_tick = now;
			ae_obj_fields_begin(&fields);
			ae_obj_fields_long(&fields, "p", (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index));
			ae_obj_fields_long(&fields, "b", 1);
			ae_obj_fields_long(&fields, "team", (long)tick->player->team_index);
			ae_obj_fields_text(&fields, "job", ae_bots_behaviour_name(behaviour));
			ae_obj_fields_text(&fields, "objective", objective);
			ae_obj_fields_real(&fields, "d_obj", d_obj);
			ae_obj_fields_text(&fields, "reason", reason);
			ae_obj_fields_real(&fields, "since_s", (now - ae_bots_objlog_bots[bot_index].best_tick) / (double)TICKS_PER_SECOND);
			ae_obj_emit("obj_blocked", &fields);
		}
	}
}

/* (ae_bots_objective_think, a change of behaviour) from, to, the utilities of both, and what forced it */
static void ae_bots_objlog_job_change(struct bots_bot *bot, long bot_index, short from, short to, float const *utility, int forced,
	real_point3d const *position)
{
	struct ae_obj_fields fields;
	struct player_datum *player = player_try_and_get(bot->player_index);
	char const *objective;
	double d_obj;
	static char const *const forced_names[] = { "none", "carrier_drop", "carrier_wait", "carrier_recover" };

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client() || !player || from < 0 || from >= AE_BOTS_BEHAVIOURS ||
		to < 0 || to >= AE_BOTS_BEHAVIOURS)
	{
		return;
	}
	(void)position;
	ae_obj_objective_of(bot->player_index, &objective, &d_obj);
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "p", (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index));
	ae_obj_fields_long(&fields, "b", 1);
	ae_obj_fields_long(&fields, "team", (long)player->team_index);
	ae_obj_fields_text(&fields, "from", ae_bots_behaviour_name(from));
	ae_obj_fields_text(&fields, "to", ae_bots_behaviour_name(to));
	ae_obj_fields_long(&fields, "util_from", (long)(utility[from] * 100.0f + 0.5f));
	ae_obj_fields_long(&fields, "util_to", (long)(utility[to] * 100.0f + 0.5f));
	ae_obj_fields_text(&fields, "forced", forced_names[forced >= 0 && forced < 4 ? forced : 0]);
	ae_obj_fields_real(&fields, "d_obj", d_obj);
	ae_obj_emit("job_change", &fields);
	(void)bot_index;
}
