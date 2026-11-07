/*
AE_SERVER_RESTART.C

The warned stop and restart of a dedicated server (ae_server_restart.h). One hook, in the director's frame.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"

#include "ae_server_restart.h"
#include "ae_server_admin_rules.h"
#include "network_distributed.h"
#include "server_director.h"
#ifdef HALO_PORT_CHAT
#include "chat.h"
#endif
#include "../src/port_config.h"
#include "../src/posix_server.h"
#include "../src/server_json.h"
#include "../src/server_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void platform_log(char const *format, ...);

static struct
{
	int active;
	int restart;
	long total;
	unsigned long deadline;
	long announced;
	char reason[160];
	/* the daily restart's moment already begun (or cancelled): not begun again */
	unsigned long daily_target;
} countdown;

static void say_to_players(char const *text)
{
	server_log(_server_log_info, "chat", "server: %s", text);
#ifdef HALO_PORT_CHAT
	chat_server_say(text);
#endif
	/* (a client without the chat sees the game's notice, in a game) */
	if (game_in_progress() && game_engine_running())
		distributed_send_notice(text);
}

static void say_left(long seconds)
{
	char text[256];

	snprintf(text, sizeof(text), "Server %s in %ld second%s%s%s.", countdown.restart ? "restarting" : "stopping", seconds,
		seconds == 1 ? "" : "s", countdown.reason[0] ? ": " : "", countdown.reason);
	say_to_players(text);
}

void ae_restart_start(int restart, long seconds, char const *reason)
{
	if (seconds < 1)
		return;
	if (seconds > 3600)
		seconds = 3600;
	memset(&countdown.active, 0, sizeof(countdown) - sizeof(countdown.daily_target));
	countdown.active = 1;
	countdown.restart = restart;
	countdown.total = seconds;
	countdown.deadline = posix_server_unix_seconds() + (unsigned long)seconds;
	snprintf(countdown.reason, sizeof(countdown.reason), "%s", reason ? reason : "");
	server_log(_server_log_info, "server", "%s in %ld seconds%s%s (exit code %d); sv_%s cancel cancels it",
		restart ? "restart" : "stop", seconds, reason && reason[0] ? ": " : "", reason ? reason : "",
		restart ? SERVER_EXIT_RESTART : SERVER_EXIT_STOPPED, restart ? "restart" : "stop");
	countdown.announced = seconds;
	say_left(seconds);
}

int ae_restart_cancel(void)
{
	if (!countdown.active)
		return 0;
	countdown.active = 0;
	server_log(_server_log_info, "server", "the %s is cancelled", countdown.restart ? "restart" : "stop");
	say_to_players(countdown.restart ? "The server restart is cancelled." : "The server stop is cancelled.");
	return 1;
}

int ae_restart_active(void)
{
	return countdown.active;
}

long ae_restart_seconds_left(void)
{
	long left;

	if (!countdown.active)
		return 0;
	left = (long)countdown.deadline - (long)posix_server_unix_seconds();
	return left > 0 ? left : 0;
}

/* the daily restart: begins its countdown so that it ends at the time of day */
static void daily_update(void)
{
	char const *text = config_string("server.restart_daily");
	int minute;
	time_t now;
	struct tm local;
	long second_of_day, until, warn;

	if (!text[0])
		return;
	minute = ae_daily_parse(text);
	if (minute < 0)
		return;
	/* (a server that has just started does not restart at once) */
	if (server_mode_uptime() < 30.0)
		return;
	now = (time_t)posix_server_unix_seconds();
	{ struct tm *moment = localtime(&now); if (!moment) return; local = *moment; }
	second_of_day = local.tm_hour * 3600L + local.tm_min * 60L + local.tm_sec;
	until = ae_daily_seconds_until(minute, second_of_day);
	warn = config_integer("server.restart_warning_seconds");
	if (warn < 1)
		warn = 1;
	if (warn > 3600)
		warn = 3600;
	if (until <= warn && countdown.daily_target != (unsigned long)now + (unsigned long)until)
	{
		/* (the moment's own second, kept: a cancel is not undone by the next frame) */
		countdown.daily_target = (unsigned long)now + (unsigned long)until;
		ae_restart_start(1, until, "daily restart");
	}
}

void ae_restart_update(void)
{
	long left, due;

	if (!countdown.active)
	{
		daily_update();
		return;
	}
	left = (long)countdown.deadline - (long)posix_server_unix_seconds();
	if (left <= 0)
	{
		countdown.active = 0;
		server_director_drain(_server_drain_now, countdown.restart, countdown.reason);
		return;
	}
	due = ae_warning_due(countdown.total, left, countdown.announced);
	if (due)
	{
		countdown.announced = due;
		say_left(left < due ? left : due);
	}
}

void ae_restart_status_json(struct server_json *json)
{
	if (!countdown.active)
	{
		server_json_null(json, "restart_warning");
		return;
	}
	server_json_object_begin(json, "restart_warning");
	server_json_integer(json, "seconds", ae_restart_seconds_left());
	server_json_integer(json, "total", countdown.total);
	server_json_boolean(json, "restart", countdown.restart != 0);
	server_json_string(json, "reason", countdown.reason);
	server_json_object_end(json);
}
