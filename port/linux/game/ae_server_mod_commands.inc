/*
AE_SERVER_MOD_COMMANDS.INC

The moderation depth's commands (server_commands.c includes this, after the keys' and backups'; ae_server_mod.c does the
work): sv_admins, sv_admin_add, sv_admin_remove, sv_access, sv_whitelist, sv_ban_range, sv_ban_name, sv_bans_export and
sv_bans_import. Part of server_commands.c: its statics are used as they are.
*/

static boolean command_admins(char const *line, long count, char **words)
{
	static char text[16 * 1024];

	ae_ops_admin_text(text, sizeof(text));
	say_lines(text);
	return TRUE;
}

/* sv_admin_add <name:login|cidr:list> <viewer|moderator|admin> [immunity] [days] [allow] */
static boolean command_admin_add(char const *line, long count, char **words)
{
	char secret[48], message[200];
	long immunity = 0, days = 0;
	int role;

	if (count < 3 || count > 6)
	{
		say("sv_admin_add <name:login|cidr:a.b.c.d/n[,...]> <viewer|moderator|admin> [immunity 0-99] [days] [allow a.b.c.d/n,...]");
		return FALSE;
	}
	role = ae_mod_role_parse(words[2]);
	if (count >= 4 && !parse_integer(words[3], 0, AE_IMMUNITY_MAXIMUM, &immunity))
	{
		say("sv_admin_add: immunity is 0 to %d", AE_IMMUNITY_MAXIMUM);
		return FALSE;
	}
	if (count >= 5 && !parse_integer(words[4], 0, 3650, &days))
	{
		say("sv_admin_add: days is 0 (never) or 1 to 3650");
		return FALSE;
	}
	if (!ae_ops_admin_add(words[1], role, (int)immunity, days, count == 6 ? words[5] : "",
		server_commands.origin == _server_origin_web ? "web" : "the console", secret, sizeof(secret), message, sizeof(message)))
	{
		say("sv_admin_add: %s", message);
		return FALSE;
	}
	if (secret[0])
	{
		say("admin %s made. The secret is shown this once; it is kept as a hash. Sign in with it as the name and secret of "
			"Basic auth, or the RCON's {\"type\":\"auth\",\"name\":...,\"secret\":...}:", words[1] + 5);
		say("%s", secret);
	}
	else
	{
		say("admin row %s made", words[1]);
	}
	memset(secret, 0, sizeof(secret));
	return TRUE;
}

static boolean command_admin_remove(char const *line, long count, char **words)
{
	if (count != 2)
	{
		say("sv_admin_remove <name:login|cidr:list>");
		return FALSE;
	}
	if (!ae_ops_admin_remove(words[1], server_commands.origin == _server_origin_web ? "web" : "the console"))
	{
		say("sv_admin_remove: no row %s (sv_admins lists them)", words[1]);
		return FALSE;
	}
	say("admin row %s taken out", words[1]);
	return TRUE;
}

static boolean command_access(char const *line, long count, char **words)
{
	static char text[8 * 1024];

	ae_server_mod_access_text(text, sizeof(text));
	say_lines(text);
	return TRUE;
}

static boolean command_whitelist(char const *line, long count, char **words)
{
	static char text[32 * 1024];
	char why[160];

	if (count < 2 || !csstrcasecmp(words[1], "list"))
	{
		say("the whitelist is %s", config_boolean("server.whitelist") ? "ON" : "off (server.whitelist)");
		ae_server_mod_whitelist_text(text, sizeof(text));
		say_lines(text);
		return TRUE;
	}
	if (!csstrcasecmp(words[1], "on") || !csstrcasecmp(words[1], "off"))
	{
		boolean on = !csstrcasecmp(words[1], "on");

		if (!config_write_boolean("server.whitelist", on))
		{
			say("sv_whitelist: config.toml cannot be written");
			return FALSE;
		}
		say("the whitelist is %s", on ? "ON: only listed machines and admin rows join" : "off");
		return TRUE;
	}
	if (count == 3 && !csstrcasecmp(words[1], "add"))
	{
		if (!ae_server_mod_whitelist_add(words[2], why, sizeof(why)))
		{
			say("sv_whitelist: %s", why);
			return FALSE;
		}
		say("%s listed", words[2]);
		return TRUE;
	}
	if (count == 3 && !csstrcasecmp(words[1], "remove"))
	{
		if (!ae_server_mod_whitelist_remove(words[2]))
		{
			say("sv_whitelist: %s is not listed", words[2]);
			return FALSE;
		}
		say("%s taken off", words[2]);
		return TRUE;
	}
	say("sv_whitelist [list] | add <ip:..|hwid:..|discord:..|name:..> | remove <entry> | on | off");
	return FALSE;
}

/* sv_ban_range <a.b.c.d/n> [30m|2h|7d|forever] [reason] and sv_ban_name <pattern> [duration] [reason] */
static boolean command_ban_by(char const *line, long count, char **words)
{
	char why[200];
	long seconds = 0, reason_words = 2;
	boolean range = !csstrcasecmp(words[0], "sv_ban_range");

	if (count < 2)
	{
		say("%s: give %s, then a duration (30m, 2h, 7d, forever) and a reason", words[0],
			range ? "a network (a.b.c.d/n)" : "a name pattern (* for any run, ? for one character)");
		return FALSE;
	}
	if (count >= 3)
	{
		seconds = parse_duration(words[2]);
		if (seconds >= 0)
			reason_words = 3;
		else
			seconds = 0;
	}
	if (!ae_server_mod_ban_write(range ? words[1] : "", range ? "" : words[1], seconds, rest_after_words(line, reason_words),
		server_commands.role, why, sizeof(why)))
	{
		say("%s: %s", words[0], why);
		return FALSE;
	}
	server_log(_server_log_info, "moderation", "%s %s (%s)", words[0], seconds > 0 ? "added, for a while" : "added",
		server_role_name(server_commands.role));
	moderation_event("ban", range ? "a network" : words[1], NULL, seconds > 0 ? posix_server_unix_seconds() + (unsigned long)seconds : 0);
	say("ban added%s; players it holds are dropped within a second", seconds > 0 ? " (for a while)" : "");
	return TRUE;
}

static boolean command_bans_export(char const *line, long count, char **words)
{
	char why[160];

	if (!ae_server_mod_bans_export_file(count >= 2 ? words[1] : "bans-export.json", why, sizeof(why)))
	{
		say("sv_bans_export: %s", why);
		return FALSE;
	}
	say("bans written to the data folder's %s", count >= 2 ? words[1] : "bans-export.json");
	return TRUE;
}

static boolean command_bans_import(char const *line, long count, char **words)
{
	char why[200];
	int added = 0, skipped = 0, refused = 0;

	if (count != 2)
	{
		say("sv_bans_import <file.json in the data folder>");
		return FALSE;
	}
	if (!ae_server_mod_bans_import_file(words[1], &added, &skipped, &refused, why, sizeof(why)))
	{
		say("sv_bans_import: %s", why);
		return FALSE;
	}
	say("bans imported: %d added, %d skipped (there already, or over), %d refused (not valid)", added, skipped, refused);
	server_log(_server_log_info, "moderation", "bans imported from %s: %d added (%s)", words[1], added,
		server_role_name(server_commands.role));
	return TRUE;
}
