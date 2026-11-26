/*
AE_PVE_GLUE.H

What Content Evolved's campaign score, campaign medals, service rank and Firefight stats (campaign_score.c,
campaign_medals.c, service_rank.c, firefight_stats.c) ask of this build and that it has no copy of (ae_pve_glue.c):
the multiplayer score panel's numbers, the social service and the community server. None of those runs here.
None of these systems plays in a multiplayer game (ae_pve_rules.h), and nothing leaves the machine without the
community services' opt-in (ae_stats_upload_allowed).
*/

#ifndef __AE_PVE_GLUE_H
#define __AE_PVE_GLUE_H

/* what the multiplayer score's HUD panel shows of a player (campaign_score.c draws it; no screen of this build
reaches it): this build's game engine has none to give */
enum
{
	_game_engine_hud_no_team = 0,
	_game_engine_hud_winning,
	_game_engine_hud_losing,
	_game_engine_hud_tied,
};

struct game_engine_hud_score
{
	wchar_t kind[48];
	wchar_t score[64];
	long place;
	boolean place_tied;
	short team_standing;
	boolean time_left;
	wchar_t time[32];
};

boolean game_engine_hud_score_get(long player_index, struct game_engine_hud_score *hud_score);

/* whether this game's medals, score, rank and stats may be written: a campaign level or a Firefight game, never a
multiplayer game's */
int ae_pve_records_allowed(void);

/* whether the stats may leave this machine, or a socket listen for the stats' web page: the community services'
opt-in is given (network.community_services), and the game is a Firefight or campaign game, never a multiplayer
game's. The pre-release security review reads every use of it */
int ae_stats_upload_allowed(void);

/* whether the stats' loopback socket may listen: the community services' opt-in alone (no game is running when
it starts) */
int ae_stats_socket_allowed(void);

/* the stats' web request (the community server's client of Content Evolved's build): this build has none, so
nothing is ever sent; 0 for a request that failed */
int update_request(const char *method, const char *url, const char *headers, const char *body, char *response,
	int response_size, char *error, int error_size);

#endif /* __AE_PVE_GLUE_H */
