#include <stdio.h>
#include <string.h>
#include "ae_server_admin_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_keys(void)
{
	char id[16], secret[64];
	const char *good = "aek_0123abcd_0123456789abcdef0123456789abcdef0123456789abcdef";
	unsigned long expires = 7;
	struct ae_key_record record, back;
	char line[AE_KEY_LINE_SIZE];

	CHECK(ae_key_name_valid("agent-1_x") && ae_key_name_valid("a"));
	CHECK(!ae_key_name_valid("") && !ae_key_name_valid("Agent") && !ae_key_name_valid("a b") && !ae_key_name_valid(NULL));
	CHECK(ae_key_name_valid("12345678901234567890123456789012") && !ae_key_name_valid("123456789012345678901234567890123"));
	CHECK(ae_key_role_parse("viewer") == 0 && ae_key_role_parse("moderator") == 1 && ae_key_role_parse("admin") == 2);
	CHECK(ae_key_role_parse("owner") == -1 && ae_key_role_parse("Admin") == -1 && ae_key_role_parse(NULL) == -1);
	CHECK(ae_key_token_parse(good, id, secret) && !strcmp(id, "0123abcd") && strlen(secret) == 48);
	CHECK(!ae_key_token_parse("ae_0123abcd_0123456789abcdef0123456789abcdef0123456789abcdef", id, secret));
	CHECK(!ae_key_token_parse("aek_0123ABCD_0123456789abcdef0123456789abcdef0123456789abcdef", id, secret));
	CHECK(!ae_key_token_parse("aek_0123abcd-0123456789abcdef0123456789abcdef0123456789abcdef", id, secret));
	CHECK(!ae_key_token_parse("aek_0123abcd_0123", id, secret) && !ae_key_token_parse("", id, secret));
	CHECK(!ae_key_expired(0, 1000000) && !ae_key_expired(100, 99) && ae_key_expired(100, 100) && ae_key_expired(100, 101));
	CHECK(ae_key_expiry(0, 1000, &expires) == 0 && expires == 0);
	CHECK(ae_key_expiry(2, 1000, &expires) == 0 && expires == 1000 + 172800);
	CHECK(ae_key_expiry(-1, 1000, &expires) == -1 && ae_key_expiry(3651, 1000, &expires) == -1);
	CHECK(ae_key_equal("abcd", "abcd", 4) && !ae_key_equal("abcd", "abce", 4) && !ae_key_equal("abcd", "bbcd", 4));
	/* the roles a key may be minted with: never the owner's, never above the minter's */
	CHECK(ae_key_role_may_mint(3, 2) && ae_key_role_may_mint(2, 2) && ae_key_role_may_mint(2, 0));
	CHECK(!ae_key_role_may_mint(1, 2) && !ae_key_role_may_mint(3, 3) && !ae_key_role_may_mint(3, -1));
	memset(&record, 0, sizeof(record));
	strcpy(record.id, "0123abcd");
	strcpy(record.name, "bot-1");
	record.role = 1;
	record.created = 123;
	record.expires = 456;
	memset(record.hash, 'a', 64);
	strcpy(record.allow, "192.168.7.0/24,10.0.0.1");
	CHECK(ae_key_line_format(&record, line, sizeof(line)));
	CHECK(ae_key_line_parse(line, &back) && !strcmp(back.name, "bot-1") && back.role == 1 && back.expires == 456 &&
		!strcmp(back.allow, "192.168.7.0/24,10.0.0.1") && !strcmp(back.hash, record.hash));
	record.allow[0] = 0;
	CHECK(ae_key_line_format(&record, line, sizeof(line)) && ae_key_line_parse(line, &back) && !back.allow[0]);
	CHECK(!ae_key_line_parse("junk", &back) && !ae_key_line_parse("0123abcd\tBAD\tadmin\t1\t0\taa\t", &back));
	CHECK(!ae_key_line_parse("0123abcd\tx\towner\t1\t0\taaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", &back));
}

static void test_allow(void)
{
	CHECK(ae_key_allow_valid("") && ae_key_allow_valid("10.0.0.1") && ae_key_allow_valid("192.168.7.0/24"));
	CHECK(ae_key_allow_valid("10.0.0.0/8,192.168.1.5/32") && ae_key_allow_valid("0.0.0.0/0"));
	CHECK(!ae_key_allow_valid("10.0.0") && !ae_key_allow_valid("10.0.0.256") && !ae_key_allow_valid("10.0.0.0/33"));
	CHECK(!ae_key_allow_valid("10.0.0.0/") && !ae_key_allow_valid("abc") && !ae_key_allow_valid("10.0.0.1,"));
	CHECK(ae_key_allow_permits("", 0xC0A80705UL) && ae_key_allow_permits(NULL, 1));
	CHECK(ae_key_allow_permits("192.168.7.0/24", 0xC0A80705UL) && !ae_key_allow_permits("192.168.7.0/24", 0xC0A80C05UL));
	CHECK(ae_key_allow_permits("10.0.0.1,192.168.7.0/24", 0x0A000001UL) && !ae_key_allow_permits("10.0.0.1", 0x0A000002UL));
	CHECK(ae_key_allow_permits("0.0.0.0/0", 0x01020304UL));
	/* (a network address with host bits set is the network) */
	CHECK(ae_key_allow_permits("192.168.7.77/24", 0xC0A80701UL));
}

static void test_warnings(void)
{
	/* a 90 s delay: announced at once, then at 60, 30, 10, 5 */
	CHECK(ae_warning_due(90, 90, 0) == 90);
	CHECK(ae_warning_due(90, 80, 90) == 0 && ae_warning_due(90, 60, 90) == 60);
	CHECK(ae_warning_due(90, 59, 60) == 0 && ae_warning_due(90, 30, 60) == 30);
	CHECK(ae_warning_due(90, 10, 30) == 10 && ae_warning_due(90, 5, 10) == 5 && ae_warning_due(90, 4, 5) == 0);
	/* a tick that skipped: 8 s left after 30 announced -> the 10 s notice, then the 5 s at 5 */
	CHECK(ae_warning_due(90, 8, 30) == 10 && ae_warning_due(90, 8, 10) == 0);
	/* 60 s: 60 at once, 30 next (no repeat of 60) */
	CHECK(ae_warning_due(60, 60, 0) == 60 && ae_warning_due(60, 59, 60) == 0 && ae_warning_due(60, 30, 60) == 30);
	/* 20 s: 20, then 10, then 5 */
	CHECK(ae_warning_due(20, 20, 0) == 20 && ae_warning_due(20, 10, 20) == 10 && ae_warning_due(20, 5, 10) == 5);
	/* 3 s: 3 at once, nothing more */
	CHECK(ae_warning_due(3, 3, 0) == 3 && ae_warning_due(3, 2, 3) == 0 && ae_warning_due(3, 1, 3) == 0);
	CHECK(ae_warning_due(0, 0, 0) == 0 && ae_warning_due(60, 0, 30) == 0);
}

static void test_daily(void)
{
	CHECK(ae_daily_parse("04:00") == 240 && ae_daily_parse("00:00") == 0 && ae_daily_parse("23:59") == 1439);
	CHECK(ae_daily_parse("") == -1 && ae_daily_parse("24:00") == -1 && ae_daily_parse("4:00") == -1 &&
		ae_daily_parse("04:60") == -1 && ae_daily_parse("04-00") == -1 && ae_daily_parse(NULL) == -1);
	CHECK(ae_daily_seconds_until(240, 3 * 3600) == 3600);
	CHECK(ae_daily_seconds_until(240, 5 * 3600) == 23 * 3600);
	CHECK(ae_daily_seconds_until(240, 240 * 60) == 86400);
}

static void test_backups(void)
{
	char name[64];
	const char *entries[] = { "config.toml.20261106-010000", "config.toml.20261107-010000", "config.toml.20261105-235959",
		"config.toml.20261106-120000" };
	int drop[4];

	CHECK(ae_backup_name("bans.txt", 2026, 11, 6, 3, 4, 5, name, sizeof(name)) && !strcmp(name, "bans.txt.20261106-030405"));
	CHECK(!ae_backup_name("bans.txt", 2026, 11, 6, 3, 4, 5, name, 10));
	CHECK(ae_backup_matches("bans.txt", "bans.txt.20261106-030405"));
	CHECK(!ae_backup_matches("bans.txt", "bans.txt.2026110-030405") && !ae_backup_matches("bans.txt", "config.toml.20261106-030405"));
	CHECK(!ae_backup_matches("bans.txt", "bans.txt.bak") && !ae_backup_matches("bans", "bans.txt.20261106-030405"));
	/* (a stamp's time: date -u -d '2026-11-06 03:04:05' +%s is 1793934245) */
	CHECK(ae_backup_stamp_time("bans.txt.20261106-030405") == 1793934245LL);
	CHECK(ae_backup_stamp_time("19700101-000000") == 0 && ae_backup_stamp_time("config.toml.20240229-235959") == 1709251199LL);
	CHECK(ae_backup_stamp_time("bans.txt.bak") == -1 && ae_backup_stamp_time("bans.txt.20261306-030405") == -1 &&
		ae_backup_stamp_time(NULL) == -1);
	CHECK(ae_backup_prune(entries, 4, 2, drop) == 2);
	CHECK(!drop[1] && !drop[3] && drop[0] && drop[2]);
	CHECK(ae_backup_prune(entries, 4, 20, drop) == 0);
	CHECK(ae_backup_prune(entries, 4, 0, drop) == 3 && !drop[1]);
}

int main(void)
{
	test_keys();
	test_allow();
	test_warnings();
	test_daily();
	test_backups();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
