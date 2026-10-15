/* HUD STYLE (display.hud_style): AE unless the setting reads "classic" (port/linux/game/ae_hud_style.c), with a
fake config_string in place of the port's */
#include <stdio.h>
#include <string.h>

#include "ae_hud_style.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define AE_CHECK_STR(a, b) AE_CHECK(strcmp((a), (b)) == 0)

/* the fake config: one string setting; "" as the port's config_string gives for a missing value */
static const char *hud_style_value = "";

const char *config_string(const char *name)
{
	return strcmp(name, "display.hud_style") == 0 ? hud_style_value : "";
}

static void test_default_is_ae(void)
{
	hud_style_value = "";
	AE_CHECK(ae_hud_style_is_ae());
	AE_CHECK_STR(ae_hud_style_name(), "ae");
	hud_style_value = "ae";
	AE_CHECK(ae_hud_style_is_ae());
}

static void test_classic(void)
{
	hud_style_value = "classic";
	AE_CHECK(!ae_hud_style_is_ae());
	AE_CHECK_STR(ae_hud_style_name(), "classic");
}

static void test_unknown_value_is_ae(void)
{
	hud_style_value = "fancy";
	AE_CHECK(ae_hud_style_is_ae());
	hud_style_value = "CLASSIC";
	AE_CHECK(ae_hud_style_is_ae());
}

int main(void)
{
	test_default_is_ae();
	test_classic();
	test_unknown_value_is_ae();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
