/*
AE_COMMUNITY_RULES.H

Arena Evolved: the community services' opt-in (0.3.5), its rules. The ChupathingyCE team runs a game list and match
stats at network.browser_url (halo.milenko.org): System Link's list of internet games, LIST HOSTED GAMES, SHARE GAME
STATS and SHARE HOSTED STATS. network.community_services says whether this copy uses them: "yes", "no", or "ask"
(the default: a new install, and an upgrade from 0.3.4, whose config.toml has no such line). Nothing goes to that
server unless it is "yes". "ask" is asked once, by the COMMUNITY SERVICES screen in front of the PC menus' first main
menu (port/linux/game/ae_community_screen.c); the answer is written at once.

Plain C: no SDL, no game headers (tools/test_ae_units.py builds it with port/linux/tests/ae_community_rules_test.c).
ae_community.c reads the settings and calls these.
*/

#ifndef __AE_COMMUNITY_RULES_H
#define __AE_COMMUNITY_RULES_H

enum ae_community_state
{
	AE_COMMUNITY_ASK,
	AE_COMMUNITY_YES,
	AE_COMMUNITY_NO
};

/* the input guard: a press in the screen's first half second is not an answer (the press that closed the screen
before it, a child mashing A) */
#define AE_COMMUNITY_INPUT_GUARD_MS 500UL

/* what the screen and the network settings say when the services are off and a player asks for one of them (Link
Profile, the profile page) */
#define AE_COMMUNITY_OFF_TEXT "Community services are off: Settings > Network Setup."

/* network.community_services's value as a state: "yes" and "no" (any case), and anything else, an empty or missing
value too (a 0.3.4 config.toml has no line: the setting's default), "ask" */
int ae_community_state_of(char const *value);

/* whether the services may be used: only when the answer is yes ("ask" is off until it is answered) */
int ae_community_on(int state);

/* where the game is when its main menu shows */
struct ae_community_context
{
	/* network.community_services's state */
	int state;
	/* the PC menus are the menus (display.menus = "pc" and they built); the Xbox's menus never ask: their players
	keep "ask", which is off, and change it in config.toml or the PC menus' Network Setup */
	int pc_menus;
	/* AE's renderer can draw (a window, GL, the game browser built in) */
	int renderer;
	/* nobody to ask: a dedicated server, a hidden window, a run with debug.exit_after, a network test, a screen
	opened by debug.menu_open */
	int headless;
};

/* whether the COMMUNITY SERVICES screen opens in front of the main menu */
int ae_community_should_ask(struct ae_community_context const *context);

/* what an answer writes */
struct ae_community_writes
{
	/* network.community_services: "yes" or "no" */
	char const *services;
	/* network.list_hosted_games, network.report_joined_games, network.report_events */
	int list_hosted_games;
	int report_joined_games;
	int report_events;
};

/* the answer: use (USE COMMUNITY SERVICES) or not (NOT NOW, B, Esc), with the screen's LIST HOSTED GAMES row's value
(list_row_on). USE turns the services and both stats on, and listing as the row says (it is off unless the player
turned it on); NOT NOW turns everything off, the row's value whatever it is (listing needs the list's server) */
void ae_community_answer(int use, int list_row_on, struct ae_community_writes *writes);

/* whether a press at now (milliseconds) is taken, on a screen opened at opened_at (the same clock): not before
AE_COMMUNITY_INPUT_GUARD_MS (the clock wrapping is allowed for) */
int ae_community_input_open(unsigned long opened_at, unsigned long now);

#endif
