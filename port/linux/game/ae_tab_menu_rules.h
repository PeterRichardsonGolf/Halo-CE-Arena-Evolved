/*
AE_TAB_MENU_RULES.H

The rows of the TAB / START menu (ae_screen_tab_menu.c), in order: PLAYER, CONTROLLER, KEYBOARD, SETTINGS, CREDITS,
NETWORK STATUS, then under a rule QUIT. Pure (port/linux/tests/ae_tab_menu_rules_test.c).
*/

#ifndef __AE_TAB_MENU_RULES_H
#define __AE_TAB_MENU_RULES_H

enum { AE_TAB_ROW_PLAYER, AE_TAB_ROW_CONTROLLER, AE_TAB_ROW_KEYBOARD, AE_TAB_ROW_SETTINGS, AE_TAB_ROW_CREDITS,
	AE_TAB_ROW_NETWORK_STATUS, AE_TAB_ROW_QUIT, AE_TAB_ROWS };

struct ae_tab_row
{
	int id;
	char const *label, *help;
	int under_rule;    /* the rule is drawn above this row */
};

/* the row table: AE_TAB_ROWS rows */
struct ae_tab_row const *ae_tab_menu_rows(int *count);
/* the rows' labels joined by " | " into text, for the log */
void ae_tab_menu_summary(char *text, int size);

#endif
