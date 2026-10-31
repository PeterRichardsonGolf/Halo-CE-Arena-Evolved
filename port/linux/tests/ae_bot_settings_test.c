/* The bot menu settings' typed count: accepted text and the menu limit. */
#include <stdio.h>
#include <string.h>

#include "ae_bot_settings.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static long count_now;
static char written[16];
long config_integer(const char *name) { (void)name; return count_now; }
int config_boolean(const char *name) { (void)name; return 0; }
const char *config_string(const char *name) { (void)name; return "normal"; }
int config_write(const char *name, const char *value)
{
	(void)name;
	snprintf(written, sizeof(written), "%s", value);
	return 1;
}

/* BOTS stepped from a count: the count written */
static const char *stepped(long from, short direction)
{
	count_now = from;
	written[0] = 0;
	ae_bot_setting_step(AE_BOT_COUNT, direction, "test");
	return written;
}
void platform_log(const char *format, ...) { (void)format; }
void ae_sound_request(int sound, int repeat) { (void)sound; (void)repeat; }
short ae_value_step(short count, short current, short direction)
{
	return (short)((current + direction + count) % count);
}

int main(void)
{
	char value[8];

	CHECK(ae_bot_count_typing_ok("") && ae_bot_count_typing_ok("0") && ae_bot_count_typing_ok("31") &&
		ae_bot_count_typing_ok("999"));
	CHECK(!ae_bot_count_typing_ok(NULL) && !ae_bot_count_typing_ok("1234") && !ae_bot_count_typing_ok("-1") &&
		!ae_bot_count_typing_ok("3.1") && !ae_bot_count_typing_ok("OFF"));
	CHECK(ae_bot_count_typed("0", value, sizeof(value)) && !strcmp(value, "0"));
	CHECK(ae_bot_count_typed("7", value, sizeof(value)) && !strcmp(value, "7"));
	CHECK(ae_bot_count_typed("31", value, sizeof(value)) && !strcmp(value, "31"));
	CHECK(ae_bot_count_typed("99", value, sizeof(value)) && !strcmp(value, "99"));
	CHECK(ae_bot_count_typed("127", value, sizeof(value)) && !strcmp(value, "127"));
	CHECK(ae_bot_count_typed("999", value, sizeof(value)) && !strcmp(value, "127"));
	CHECK(!ae_bot_count_typed("", value, sizeof(value)) && !ae_bot_count_typed("x", value, sizeof(value)) &&
		!ae_bot_count_typed("1", NULL, 0));
	/* the stops: OFF, 1-31, then 47, 63, 95, 127, wrapping; a count between them steps to its neighbours */
	CHECK(!strcmp(stepped(30, 1), "31") && !strcmp(stepped(31, 1), "47") && !strcmp(stepped(47, 1), "63") &&
		!strcmp(stepped(95, 1), "127"));
	CHECK(!strcmp(stepped(127, 1), "0") && !strcmp(stepped(0, -1), "127") && !strcmp(stepped(47, -1), "31"));
	CHECK(!strcmp(stepped(40, 1), "47") && !strcmp(stepped(40, -1), "31") && !strcmp(stepped(100, -1), "95"));
	/* the warning past 31 */
	count_now = 31;
	CHECK(!ae_bot_setting_warns(AE_BOT_COUNT) && !ae_bot_setting_warns(AE_BOT_SKILL));
	count_now = 32;
	CHECK(ae_bot_setting_warns(AE_BOT_COUNT) && strstr(ae_bot_setting_help(AE_BOT_COUNT), "Past 31") != NULL);
	if (failures)
		return 1;
	printf("ae_bot_settings_test: ok\n");
	return 0;
}
