#ifndef AE_SERVER_RESTART_H
#define AE_SERVER_RESTART_H

/*
AE_SERVER_RESTART.H

A dedicated server's warned stop and restart (0.7.7): sv_stop or sv_restart with a delay, and the daily restart
(server.restart_daily). The players read a chat notice at the start and at 60, 30, 10 and 5 seconds; the stop itself
is the director's own (server_director_drain, "now"). ae_server_admin_rules.c has the schedule.
*/

/* a countdown of that many seconds begun (replacing one running); the reason is said in each notice */
void ae_restart_start(int restart, long seconds, char const *reason);
/* the countdown cancelled (the players told); 0 if none runs */
int ae_restart_cancel(void);
int ae_restart_active(void);
/* the countdown's seconds left, 0 if none */
long ae_restart_seconds_left(void);
/* the status's "restart_warning": null, or {seconds (left), total, restart, reason} (the web page's live countdown) */
struct server_json;
void ae_restart_status_json(struct server_json *json);
/* every frame, from the director */
void ae_restart_update(void);

#endif
