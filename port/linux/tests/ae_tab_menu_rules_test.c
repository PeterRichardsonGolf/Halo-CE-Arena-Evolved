#include <stdio.h>
#include <string.h>
#include "ae_tab_menu_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_tab_row const *rows;
	int count, index;
	char summary[200];

	/* tab_rows: the order the owner asked for, CREDITS and NETWORK STATUS before QUIT, QUIT alone under the rule */
	rows = ae_tab_menu_rows(&count);
	CHECK(count == AE_TAB_ROWS && count == 7);
	ae_tab_menu_summary(summary, (int)sizeof(summary));
	CHECK(!strcmp(summary, "PLAYER | CONTROLLER | KEYBOARD | SETTINGS | CREDITS | NETWORK STATUS | QUIT"));
	for (index = 0; index < count; index++)
	{
		CHECK(rows[index].id == index);
		CHECK(rows[index].label && rows[index].help && rows[index].help[0]);
		CHECK(rows[index].under_rule == (index == AE_TAB_ROW_QUIT));
	}
	if (!failures)
		printf("ae_tab_menu_rules_test: all passed\n");
	return failures ? 1 : 0;
}
