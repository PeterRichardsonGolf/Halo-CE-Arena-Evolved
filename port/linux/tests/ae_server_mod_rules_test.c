#include <stdio.h>
#include <string.h>
#include "ae_server_mod_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_roles_and_immunity(void)
{
	CHECK(ae_mod_role_parse("viewer") == 0 && ae_mod_role_parse("owner") == 3 && ae_mod_role_parse("root") == -1);
	CHECK(ae_mod_role_parse(NULL) == -1 && !strcmp(ae_mod_role_text(2), "admin") && !strcmp(ae_mod_role_text(9), "none"));
	/* the owner acts on anyone; a higher role on a lower; an equal role only with more immunity; a tie is refused */
	CHECK(ae_mod_may_act(3, 0, 2, 99) && ae_mod_may_act(3, 0, 3, 99));
	CHECK(ae_mod_may_act(2, 0, 1, 99) && ae_mod_may_act(1, 0, 0, 99));
	CHECK(!ae_mod_may_act(1, 0, 2, 0) && !ae_mod_may_act(1, 99, 2, 0));
	CHECK(ae_mod_may_act(2, 50, 2, 10) && !ae_mod_may_act(2, 10, 2, 10) && !ae_mod_may_act(2, 10, 2, 50));
	/* a target with no admin row is below every actor with a role */
	CHECK(ae_mod_may_act(0, 0, -1, 0) && ae_mod_may_act(1, 0, -1, 0) && !ae_mod_may_act(-1, 0, -1, 0));
	/* a target's standing is the stronger of its address row and its key row (a signed-in owner is not "nobody") */
	{
		int role = -1, immunity = 0;

		ae_mod_standing_merge(&role, &immunity, -1, 0);
		CHECK(role == -1);
		ae_mod_standing_merge(&role, &immunity, 3, 99);
		CHECK(role == 3 && immunity == 99 && !ae_mod_may_act(1, 0, role, immunity) && !ae_mod_may_act(2, 50, role, immunity));
		ae_mod_standing_merge(&role, &immunity, 2, 99);
		CHECK(role == 3);
		role = 2;
		immunity = 10;
		ae_mod_standing_merge(&role, &immunity, 2, 40);
		CHECK(role == 2 && immunity == 40);
		ae_mod_standing_merge(&role, &immunity, 2, 5);
		CHECK(immunity == 40);
	}
}

static void test_admin_rows(void)
{
	struct ae_admin_record record, back;
	char line[AE_ADMIN_LINE_SIZE];
	char hash[65];

	memset(hash, 'a', 64);
	hash[64] = 0;
	memset(&record, 0, sizeof(record));
	record.kind = 'n';
	strcpy(record.name, "dash-bot");
	record.role = 1;
	record.immunity = 7;
	record.expires = 1234;
	strcpy(record.hash, hash);
	strcpy(record.allow, "10.0.0.0/8");
	CHECK(ae_admin_line_format(&record, line, sizeof(line)));
	CHECK(ae_admin_line_parse(line, &back) && back.kind == 'n' && !strcmp(back.name, "dash-bot") && back.role == 1 &&
		back.immunity == 7 && back.expires == 1234 && !strcmp(back.hash, hash) && !strcmp(back.allow, "10.0.0.0/8"));
	memset(&record, 0, sizeof(record));
	record.kind = 'c';
	strcpy(record.cidr, "192.168.7.0/24,10.1.1.1");
	record.role = 2;
	record.immunity = 50;
	CHECK(ae_admin_line_format(&record, line, sizeof(line)));
	CHECK(ae_admin_line_parse(line, &back) && back.kind == 'c' && !strcmp(back.cidr, "192.168.7.0/24,10.1.1.1") &&
		back.role == 2 && back.immunity == 50 && !back.hash[0]);
	CHECK(!ae_admin_expired(&back, 99999) && (back.expires = 10, ae_admin_expired(&back, 10)) && !ae_admin_expired(&back, 9));
	/* bad rows: an owner row, a bad name, a name without a secret, a bad network, an immunity too high, junk, comments */
	CHECK(!ae_admin_line_parse("cidr:10.0.0.0/8\towner\t0\t0\t\t", &back));
	CHECK(!ae_admin_line_parse("name:Bad Name\tadmin\t0\t0\taaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\t", &back));
	CHECK(!ae_admin_line_parse("name:ok\tadmin\t0\t0\t\t", &back));
	CHECK(!ae_admin_line_parse("cidr:300.0.0.0/8\tadmin\t0\t0\t\t", &back));
	CHECK(!ae_admin_line_parse("cidr:10.0.0.0/8\tadmin\t100\t0\t\t", &back));
	CHECK(!ae_admin_line_parse("junk", &back) && !ae_admin_line_parse("# a comment", &back) && !ae_admin_line_parse("", &back));
	CHECK(!ae_admin_line_parse("cidr:\tadmin\t0\t0\t\t", &back));
	/* (0.7.7 key rows: a moderator key, any case kept lower; never owner; written back as read) */
	CHECK(ae_admin_line_parse("key:00112233445566778899AABBCCDDEEFF00112233445566778899aabbccddeeff\tmoderator\t5\t0\t\t",
		&back) && back.kind == 'k' && back.role == 1 && back.immunity == 5 &&
		!strcmp(back.key, "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"));
	CHECK(ae_admin_line_format(&back, line, sizeof(line)) &&
		!strcmp(line, "key:00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\tmoderator\t5\t0\t\t\n"));
	CHECK(!ae_admin_line_parse("key:0011\tadmin\t0\t0\t\t", &back));
	CHECK(!ae_admin_line_parse("key:zz112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\tadmin\t0\t0\t\t", &back));
	CHECK(!ae_admin_line_parse("key:00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\towner\t0\t0\t\t", &back));
}

static void test_access(void)
{
	char command[AE_COMMAND_NAME_MAXIMUM + 1];
	int role = -1;

	CHECK(ae_access_line_parse("sv_ban admin", command, sizeof(command), &role) && !strcmp(command, "sv_ban") && role == 2);
	CHECK(ae_access_line_parse("  KICK   moderator  # comment", command, sizeof(command), &role) && !strcmp(command, "kick") && role == 1);
	CHECK(ae_access_line_parse("sv_map viewer", command, sizeof(command), &role) && role == 0);
	CHECK(ae_access_line_parse("quit owner", command, sizeof(command), &role) && role == 3);
	CHECK(!ae_access_line_parse("sv_ban", command, sizeof(command), &role) && !ae_access_line_parse("sv_ban god", command, sizeof(command), &role));
	CHECK(!ae_access_line_parse("# only a comment", command, sizeof(command), &role) && !ae_access_line_parse("bad-name admin", command, sizeof(command), &role));
	CHECK(!ae_access_line_parse("", command, sizeof(command), &role) && !ae_access_line_parse(NULL, command, sizeof(command), &role));
}

static void test_whitelist(void)
{
	struct ae_whitelist_entry entries[8];
	char line[96];
	int count = 0;

	CHECK(ae_whitelist_line_parse("ip:192.168.7.0/24", &entries[count]) && entries[count].kind == AE_WHITELIST_IP);
	count++;
	CHECK(ae_whitelist_line_parse("ip:10.0.0.9  # my friend", &entries[count]) && !strcmp(entries[count].value, "10.0.0.9"));
	count++;
	CHECK(ae_whitelist_line_parse("hwid:ABCDEF0123", &entries[count]) && !strcmp(entries[count].value, "abcdef0123"));
	count++;
	CHECK(ae_whitelist_line_parse("discord:123456789012345678", &entries[count]));
	count++;
	CHECK(ae_whitelist_line_parse("name:Chief Bob", &entries[count]) && entries[count].kind == AE_WHITELIST_NAME);
	count++;
	CHECK(!ae_whitelist_line_parse("ip:1.2.3", &entries[7]) && !ae_whitelist_line_parse("ip:0.0.0.0/0", &entries[7]));
	CHECK(!ae_whitelist_line_parse("hwid:xyz", &entries[7]) && !ae_whitelist_line_parse("discord:abc", &entries[7]));
	CHECK(!ae_whitelist_line_parse("name:", &entries[7]) && !ae_whitelist_line_parse("junk:1", &entries[7]));
	CHECK(!ae_whitelist_line_parse("# only a comment", &entries[7]) && !ae_whitelist_line_parse("", &entries[7]));
	CHECK(ae_whitelist_line_format(&entries[0], line, sizeof(line)) && !strcmp(line, "ip:192.168.7.0/24\n"));
	CHECK(ae_whitelist_door_ok(entries, count, 0xC0A80705UL, "", ""));
	CHECK(ae_whitelist_door_ok(entries, count, 0x0A000009UL, "", ""));
	CHECK(!ae_whitelist_door_ok(entries, count, 0x0A00000AUL, "", ""));
	CHECK(ae_whitelist_door_ok(entries, count, 0x08080808UL, "ABCDEF0123", ""));
	CHECK(ae_whitelist_door_ok(entries, count, 0x08080808UL, "", "123456789012345678"));
	CHECK(!ae_whitelist_door_ok(entries, count, 0x08080808UL, "abcdef0124", "1234"));
	CHECK(!ae_whitelist_door_ok(entries, 0, 0x08080808UL, "", ""));
	CHECK(ae_whitelist_has_names(entries, count) && !ae_whitelist_has_names(entries, 4));
	CHECK(ae_whitelist_name_ok(entries, count, "chief bob") && !ae_whitelist_name_ok(entries, count, "Bob"));
}

static void test_bans(void)
{
	char kept[40], line[512];
	struct ae_ban_record record, back, other;

	CHECK(ae_cidr_normalise("203.0.113.0/24", kept, sizeof(kept)) && !strcmp(kept, "203.0.113.0/24"));
	CHECK(ae_cidr_normalise("203.0.113.5", kept, sizeof(kept)) && !strcmp(kept, "203.0.113.5"));
	CHECK(ae_cidr_normalise("203.0.113.5/32", kept, sizeof(kept)) && !strcmp(kept, "203.0.113.5"));
	CHECK(!ae_cidr_normalise("0.0.0.0/0", kept, sizeof(kept)) && !ae_cidr_normalise("1.2.3.4/33", kept, sizeof(kept)));
	CHECK(!ae_cidr_normalise("1.2.3", kept, sizeof(kept)) && !ae_cidr_normalise("a.b.c.d", kept, sizeof(kept)));
	CHECK(!ae_cidr_normalise("1.2.3.4/", kept, sizeof(kept)) && !ae_cidr_normalise("", kept, sizeof(kept)));
	CHECK(ae_pattern_valid("Bob*") && ae_pattern_valid("?ob") && ae_pattern_valid("*x*"));
	CHECK(!ae_pattern_valid("*") && !ae_pattern_valid("**??") && !ae_pattern_valid("") && !ae_pattern_valid(NULL));
	CHECK(!ae_pattern_valid("a\tb") && !ae_pattern_valid("0123456789012345678901234567890123"));
	CHECK(ae_glob_match("bob*", "Bobby") && ae_glob_match("*bob", "BIGBOB") && ae_glob_match("b?b", "bob"));
	CHECK(!ae_glob_match("bob", "bobby") && !ae_glob_match("b?b", "bb") && ae_glob_match("*a*b*", "xxaxxbxx"));
	CHECK(!ae_glob_match("*a*b*", "xxbxxaxx") && ae_glob_match("*", "") && !ae_glob_match("a", ""));
	/* a long text does not blow up the matcher */
	CHECK(ae_glob_match("*a*a*a*a*a*b", "aaaaaaaaaaaaaaaaaaaaaaaaaaaab") && !ae_glob_match("*a*a*a*a*a*c", "aaaaaaaaaaaaaaaaaaaaaaaaaaaab"));

	memset(&record, 0, sizeof(record));
	strcpy(record.when, "2026-11-06 10:00:00");
	strcpy(record.ip, "203.0.113.0/24");
	strcpy(record.players, "cheaters");
	strcpy(record.reason, "range ban");
	record.until = 5000;
	CHECK(ae_ban_line_format(&record, line, sizeof(line)));
	CHECK(strstr(line, "\tip=203.0.113.0/24\t") && strstr(line, "\thwid=none\t") && strstr(line, "\tuntil=5000\n"));
	CHECK(ae_ban_line_parse(line, &back) && !strcmp(back.ip, "203.0.113.0/24") && !back.hwid[0] && back.until == 5000 &&
		!strcmp(back.reason, "range ban") && !strcmp(back.when, "2026-11-06 10:00:00"));
	CHECK(ae_ban_network_holds(&back, 0xCB007105UL) && !ae_ban_network_holds(&back, 0xCB007205UL) && !ae_ban_network_holds(&back, 0));
	CHECK(ae_ban_over(&back, 5000) && !ae_ban_over(&back, 4999) && (back.until = 0, !ae_ban_over(&back, 99999)));
	memset(&record, 0, sizeof(record));
	strcpy(record.pattern, "sk8er*");
	strcpy(record.players, "sk8er*");
	CHECK(ae_ban_line_format(&record, line, sizeof(line)) && strstr(line, "\tnamepat=sk8er*"));
	CHECK(ae_ban_line_parse(line, &back) && !strcmp(back.pattern, "sk8er*") && !back.ip[0]);
	/* the game's own line (a single address, a self-reported Discord id) reads, a comment does not */
	CHECK(ae_ban_line_parse("2026-01-01 00:00:00\tip=198.51.100.7\thwid=ab12\tdiscord_username=x (self-reported)\tdiscord_id=123 (self-reported)\tplayers=Bob\treason=r\tuntil=77", &back) &&
		!strcmp(back.ip, "198.51.100.7") && !strcmp(back.hwid, "ab12") && !strcmp(back.discord_id, "123") && back.until == 77);
	CHECK(!ae_ban_network_holds(&back, 0xC6336407UL));
	CHECK(!ae_ban_line_parse("# a comment ip=1.2.3.4", &back) && !ae_ban_line_parse("nothing here", &back));
	/* an imported record is cleaned, or refused */
	memset(&record, 0, sizeof(record));
	strcpy(record.ip, "198.51.100.0/24");
	strcpy(record.hwid, "none");
	strcpy(record.discord_id, "none");
	strcpy(record.reason, "tab\there");
	CHECK(ae_ban_record_clean(&record) && !record.hwid[0] && !record.discord_id[0] && !strcmp(record.when, "imported") &&
		!strchr(record.reason, '\t'));
	memset(&other, 0, sizeof(other));
	strcpy(other.ip, "198.51.100.0/24");
	CHECK(ae_ban_same(&record, &other));
	strcpy(other.ip, "198.51.100.0/25");
	CHECK(!ae_ban_same(&record, &other));
	/* (patterns decide sameness: the same pattern is the same ban, another is not) */
	memset(&record, 0, sizeof(record));
	memset(&other, 0, sizeof(other));
	strcpy(record.pattern, "Bob*");
	strcpy(other.pattern, "bob*");
	CHECK(ae_ban_same(&record, &other));
	strcpy(other.pattern, "alice*");
	CHECK(!ae_ban_same(&record, &other));
	memset(&record, 0, sizeof(record));
	CHECK(!ae_ban_record_clean(&record));
	strcpy(record.ip, "999.1.1.1");
	CHECK(!ae_ban_record_clean(&record));
	memset(&record, 0, sizeof(record));
	strcpy(record.hwid, "xyz");
	CHECK(!ae_ban_record_clean(&record));
	memset(&record, 0, sizeof(record));
	strcpy(record.pattern, "*");
	CHECK(!ae_ban_record_clean(&record));
}

static void test_flood_and_ping(void)
{
	struct ae_flood flood;
	struct ae_ping_watch watch;
	int index, hits = 0;

	memset(&flood, 0, sizeof(flood));
	/* 4 lines a second against a limit of 5 in 5 s: the sixth line is the flood, and the count starts again */
	for (index = 0; index < 5; index++)
		CHECK(!ae_flood_note(&flood, 1000 + (unsigned long)index * 250, 5));
	CHECK(ae_flood_note(&flood, 2250, 5));
	CHECK(!ae_flood_note(&flood, 2500, 5));
	/* a slow talker never floods */
	memset(&flood, 0, sizeof(flood));
	for (index = 0; index < 40; index++)
		hits += ae_flood_note(&flood, 100000 + (unsigned long)index * 2000, 5);
	CHECK(hits == 0);
	/* limit 0 is off, an over-large limit is held below the table */
	memset(&flood, 0, sizeof(flood));
	for (index = 0; index < 100; index++)
		hits += ae_flood_note(&flood, 5000, 0);
	CHECK(hits == 0);
	for (index = 0; index < 100; index++)
		hits += ae_flood_note(&flood, 6000 + (unsigned long)index, 1000);
	CHECK(hits >= 1);

	memset(&watch, 0, sizeof(watch));
	CHECK(!ae_ping_watch_note(&watch, 300, 200, 10, 1000));
	CHECK(!ae_ping_watch_note(&watch, 300, 200, 10, 5000));
	CHECK(ae_ping_watch_note(&watch, 300, 200, 10, 11000));
	/* a good ping breaks the spell; an unknown ping too; off never kicks */
	CHECK(!ae_ping_watch_note(&watch, 100, 200, 10, 12000));
	CHECK(!ae_ping_watch_note(&watch, 300, 200, 10, 13000));
	CHECK(!ae_ping_watch_note(&watch, -1, 200, 10, 30000));
	CHECK(!ae_ping_watch_note(&watch, 300, 200, 10, 31000));
	CHECK(!ae_ping_watch_note(&watch, 9999, 0, 10, 99999));
}

static int lookup(void *context, const char *field, char *value, int size)
{
	(void)context;
	if (!strcmp(field, "name"))
		snprintf(value, (size_t)size, "%s", "Ev\"il;say x{name}\\\n");
	else if (!strcmp(field, "reason"))
		snprintf(value, (size_t)size, "%s", "kicked");
	else
		return 0;
	return 1;
}

static void test_hooks(void)
{
	struct ae_hook_rule rule;
	char commands[AE_HOOK_COMMAND_MAXIMUM][AE_HOOK_COMMAND_SIZE];
	char out[200];
	unsigned long start = 0;
	int used = 0, index, allowed = 0;

	CHECK(ae_hook_line_parse("game_ended -> say Next map; sv_map_next", &rule) && !strcmp(rule.event, "game_ended") &&
		!rule.field[0]);
	CHECK(ae_hook_split(&rule, commands) == 2 && !strcmp(commands[0], "say Next map") && !strcmp(commands[1], "sv_map_next"));
	CHECK(ae_hook_line_parse("player_left[reason=kicked] -> say bye {name}   # note", &rule) && !strcmp(rule.field, "reason") &&
		!strcmp(rule.value, "kicked"));
	CHECK(ae_hook_fires(&rule, "player_left", lookup, NULL) && !ae_hook_fires(&rule, "player_joined", lookup, NULL));
	CHECK(ae_hook_line_parse("player_left[reason=quit] -> say x", &rule) && !ae_hook_fires(&rule, "player_left", lookup, NULL));
	CHECK(ae_hook_line_parse("player_left[nothing=quit] -> say x", &rule) && !ae_hook_fires(&rule, "player_left", lookup, NULL));
	CHECK(!ae_hook_line_parse("no arrow here", &rule) && !ae_hook_line_parse("Bad Event -> say x", &rule));
	CHECK(!ae_hook_line_parse("x[field] -> say x", &rule) && !ae_hook_line_parse("# a comment -> say x", &rule));
	CHECK(!ae_hook_line_parse("game_ended ->", &rule) && !ae_hook_line_parse("", &rule));
	/* an expansion cannot add a command or end a quoted word, and an unknown field is empty */
	CHECK(ae_hook_expand("tell \"{name}\" hi {nothing}!", lookup, NULL, out, sizeof(out)));
	CHECK(!strchr(out + 6, ';') && !strstr(out, "{name}") && !strchr(out, '\\') && !strchr(out, '\n'));
	CHECK(!strcmp(out, "tell \"Ev_il_say x_name___\" hi !"));
	CHECK(ae_hook_expand("plain text", lookup, NULL, out, sizeof(out)) && !strcmp(out, "plain text"));
	CHECK(ae_hook_expand("brace { alone", lookup, NULL, out, sizeof(out)) && !strcmp(out, "brace { alone"));
	CHECK(!ae_hook_expand("{name}{name}{name}{name}", lookup, NULL, out, 10));
	/* at most 3 hooks run in a window; a new window lets more */
	for (index = 0; index < 10; index++)
		allowed += ae_hook_allow(&start, &used, 1000 + (unsigned long)index, 3, 10000);
	CHECK(allowed == 3);
	CHECK(ae_hook_allow(&start, &used, 20000, 3, 10000));
}

static void test_alerts_and_claim(void)
{
	char body[600];
	unsigned long starts[4] = { 100, 200, 300, 400 };

	CHECK(ae_webhook_url_valid("https://hooks.example.com/abc?x=1") && ae_webhook_url_valid("http://127.0.0.1:9000/hook"));
	CHECK(ae_webhook_url_valid("http://localhost/h") && !ae_webhook_url_valid("http://example.com/hook"));
	CHECK(!ae_webhook_url_valid("ftp://example.com/h") && !ae_webhook_url_valid("https://") && !ae_webhook_url_valid("https://a b/"));
	CHECK(!ae_webhook_url_valid("") && !ae_webhook_url_valid(NULL) && !ae_webhook_url_valid("https://x.example/\"q"));
	CHECK(ae_alert_body("My \"Server\"", "disk_low", "free \\ 10\nMB", 1700000000UL, body, sizeof(body)));
	CHECK(strstr(body, "\"server\":\"My \\\"Server\\\"\"") && strstr(body, "\"event\":\"disk_low\"") &&
		strstr(body, "\"text\":\"free \\\\ 10 MB\"") && strstr(body, "\"time\":1700000000}"));
	CHECK(!ae_alert_body("s", "k", "text", 1, body, 20));
	CHECK(ae_restart_loop(starts, 4, 450, 600, 3) && !ae_restart_loop(starts, 4, 450, 600, 5));
	CHECK(!ae_restart_loop(starts, 4, 5000, 600, 1) && !ae_restart_loop(starts, 0, 450, 600, 1) && !ae_restart_loop(starts, 4, 450, 600, 0));
	CHECK(ae_alert_gap_ok(0, 100, 3600) && !ae_alert_gap_ok(100, 200, 3600) && ae_alert_gap_ok(100, 3700, 3600));
	CHECK(ae_disk_low(100ULL * 1024 * 1024, 500) && !ae_disk_low(900ULL * 1024 * 1024, 500) && !ae_disk_low(0, 0));
	CHECK(!ae_empty_alert_due(0, 99999, 2, 0) && !ae_empty_alert_due(1000, 1000 + 7199, 2, 0));
	CHECK(ae_empty_alert_due(1000, 1000 + 7200, 2, 0) && !ae_empty_alert_due(1000, 99999, 2, 1) && !ae_empty_alert_due(1000, 99999, 0, 0));

	CHECK(ae_claim_token_valid("0123456789abcdef0123456789abcdef") && !ae_claim_token_valid("0123456789ABCDEF0123456789abcdef"));
	CHECK(!ae_claim_token_valid("abc") && !ae_claim_token_valid(NULL) && !ae_claim_token_valid("0123456789abcdef0123456789abcdeg"));
	CHECK(ae_claim_password_valid("correct horse") && !ae_claim_password_valid("short") && !ae_claim_password_valid(NULL));
	CHECK(!ae_claim_password_valid("tab\tinside-it") && !ae_claim_password_valid("0123456789012345678901234567890123456789"
		"0123456789012345678901234567890123456789012345678901234567890123456789"));
	CHECK(ae_claim_link("0.0.0.0", 8080, body, sizeof(body)) && !strcmp(body, "http://localhost:8080/claim"));
	CHECK(ae_claim_link("192.168.1.5", 80, body, sizeof(body)) && !strcmp(body, "http://192.168.1.5:80/claim"));
	CHECK(ae_claim_link("", 9, body, sizeof(body)) && !strcmp(body, "http://localhost:9/claim"));
}

int main(void)
{
	test_roles_and_immunity();
	test_admin_rows();
	test_access();
	test_whitelist();
	test_bans();
	test_flood_and_ping();
	test_hooks();
	test_alerts_and_claim();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
