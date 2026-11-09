/*
AE_SERVER_BOTS_RULES.H

(0.7.7) Bots on a dedicated server as pure rules (no engine or config calls; port/linux/tests/ae_server_bots_rules_test.c
runs them). The glue is ae_server_bots.c; the seats, names and brain are ae_lan_bots.c's (the server's machine has no
person: its bots sit on virtual machines).

- server.bots_fill: OFF (no bots), MINIMUM (humans plus bots reach server.min_players) or EMPTY_SLOTS (humans plus bots
  fill server.max_players). The default is AUTO: MINIMUM on an entry with an AE gametype, OFF on the others.
- Humans 0: the server idles (no bots, so no game) unless server.bots_alone is on: then bot-only games keep running.
- A human who joins takes a bot's place at that bot's NEXT DEATH (never mid-fight): the surplus waits, a dead bot of the
  right team leaves.
- A human who drops mid-game is held: an unrated game's bot takes the slot (the fill rule asks for it even when the
  humans are above the minimum), a rated game's slot stays empty. The human who comes back (same name and machine
  address) gets the team and the statistics back.
- Bots are never rated: a rated result (a later change) is built from the humans' rows only.
*/

#ifndef AE_SERVER_BOTS_RULES_H
#define AE_SERVER_BOTS_RULES_H

enum
{
	AE_SERVER_BOTS_FILL_OFF = 0,
	AE_SERVER_BOTS_FILL_MINIMUM,
	AE_SERVER_BOTS_FILL_EMPTY_SLOTS,
	AE_SERVER_BOTS_FILL_AUTO
};

/* a rated game's result is built from humans only (a later change sets this per entry); a held slot of a rated game
stays empty, an unrated game's gets a bot */
#define AE_SERVER_BOTS_RATED_DEFAULT 0

/* the fill setting's name: "off", "minimum", "empty_slots", "auto"; -1 for any other */
int ae_server_bots_fill_parse(char const *name);
/* the skill names the bots know today (recruit, easy, normal, heroic, legendary, demon): 1 if it is one. TODO: "elite"
joins once the bot skill table has it (a separate task adds the tier between heroic and legendary) */
int ae_server_bots_skill_valid(char const *name);
/* the fill actually used: AUTO becomes MINIMUM for an AE entry, OFF for a stock one */
int ae_server_bots_fill_resolved(int fill, int entry_is_ae);

/* the bots the server wants.
	fill: the resolved fill; minimum: server.min_players; humans: the people in the game; held: humans dropped from the
	game in progress who may come back; hold_with_bot: 1 unrated (a bot holds the slot), 0 rated (the slot stays empty);
	max_players: the game's most players; alone: server.bots_alone; played: whether bots play the gametype.
 0 when fill is OFF, the gametype is not played, or there is no human and no bots_alone. MINIMUM: the total of humans
plus bots is the larger of the minimum and (humans plus held slots a bot holds), and with bots_alone and no human at
least two bots; EMPTY_SLOTS: as many as fill the game. A rated game's held slots are taken out of the room. Never more
than the game has room for. */
int ae_server_bots_wanted(int fill, int minimum, int humans, int held, int hold_with_bot, int max_players, int alone,
	int played);

/* the bots that leave once they die: bots there are minus the target (0 for none) */
int ae_server_bots_surplus(int bots, int target);
/* whether a bot that just died leaves: a surplus is waiting and, in a team game, the bot is on the team
ae_lan_bots_leaving_team named (leaving_team -1: none; a game without teams: any dead bot) */
int ae_server_bots_death_leaves(int surplus, int teams, int bot_team, int leaving_team);

/* a human who asks to join a game whose seats are all taken: it is let in while a bot holds a seat (free_seats the
seats no one holds, bots the bots seated); a game full of people stays full. Bots fill to even teams and to the
minimum, so a server of eight seats can hold eight bots: they never keep a person out. */
int ae_server_bots_human_fits(int free_seats, int bots);
/* whether a bot has to give its seat up for the human who joins (no seat free, a bot seated) */
int ae_server_bots_human_needs_bot_gone(int free_seats, int bots);

/* ---------- the humans who dropped and may come back */

#define AE_SERVER_REJOIN_MAXIMUM 32
#define AE_SERVER_REJOIN_NAME 12

struct ae_server_rejoin_entry
{
	int used;
	char name[AE_SERVER_REJOIN_NAME];
	unsigned long address;
	int team;
	/* 1 a bot holds the slot (unrated), 0 the slot is held empty (rated) */
	int bot_holds;
};

struct ae_server_rejoin_table
{
	struct ae_server_rejoin_entry entries[AE_SERVER_REJOIN_MAXIMUM];
};

void ae_server_rejoin_clear(struct ae_server_rejoin_table *table);
/* a human dropped: held under its name and address (the same human dropping again replaces the entry). Returns the
entry's index, -1 when the table is full */
int ae_server_rejoin_drop(struct ae_server_rejoin_table *table, char const *name, unsigned long address, int team,
	int bot_holds);
/* the entry of a human who joins the game in progress; -1 for none */
int ae_server_rejoin_find(struct ae_server_rejoin_table const *table, char const *name, unsigned long address);
/* the human is back: the entry is let go */
void ae_server_rejoin_take(struct ae_server_rejoin_table *table, int index);
/* the entries held (with_bot: 1 those a bot holds, 0 those held empty) */
int ae_server_rejoin_held(struct ae_server_rejoin_table const *table, int with_bot);

#endif
