/*
AE_BOTS_RUNNER.INC

Included by ae_bots_lan.inc, at the end of bots.c. debug.bot_runner (HALO_BOT_RUNNER, off): in a local game with bots, the
host's first local player is played by a bot brain at the game's bot skill, so a headless run has a player who fights (the gate's
scenes leave the headless player standing). The brain is the last one, BOTS_BRAINS - 1, one no real bot (machines
BOTS_FIRST_MACHINE up, BOTS_MAXIMUM of them) can hold; if a bot holds it all the same the runner does nothing and logs once.
The runner's player stays a person to the rest of the game (its machine is no bot machine): only its action is a bot's.
For tests only; nothing in the menus.
*/

/* (bots.c's bots_update_actions, after its loop over the bots) the bots' actions of the tick put in the update the tick ran
(a local game's bots are driven here, after the update was built from their queues: the action history, which the
recording and the relay of players' input read, would hold nothing of what they did), then the runner's tick */
static void ae_bots_runner_tick(struct player_action *actions, long count)
{
	static boolean refused_logged = FALSE;
	struct data_iterator iterator;
	struct player_datum *player;
	struct bots_bot *bot;
	long player_index, absolute_index;
	long bot_index = BOTS_BRAINS - 1;

	if (count > 0 && !ae_lan_bots_build_actions())
	{
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			long driven_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);

			if (player->local_player_index == NONE && bots_machine_is_bot(player->network_player_data.machine_index) &&
				driven_index >= 0 && driven_index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			{
				update_server_note_driven_action(driven_index, &actions[driven_index]);
			}
		}
	}
	if (count <= 0 || !config_boolean("debug.bot_runner"))
		return;
	player_index = local_player_get_player_index(0);
	if (player_index == NONE)
		return;
	absolute_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	if (absolute_index < 0 || absolute_index >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return;
	bot = &bots_globals.bots[bot_index];
	if (bot->player_index != player_index)
	{
		/* (a bot's player on this brain: leave it) */
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (bots_machine_is_bot(player->network_player_data.machine_index) &&
				player->network_player_data.machine_index - BOTS_FIRST_MACHINE == bot_index)
			{
				if (!refused_logged)
					platform_log("bots: the runner (debug.bot_runner) finds a bot on brain %ld and does nothing", bot_index);
				refused_logged = TRUE;
				return;
			}
		}
		bots_bot_reset(bot, player_index, bot_index);
	}
	bots_bot_update(bot, bot_index, &actions[absolute_index]);
}
