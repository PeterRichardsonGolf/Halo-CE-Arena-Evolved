/*
AE_STRINGS.H

Every text the AE menus' M2 widgets, test screens and glue show (ae_strings.c),
English, UTF-8, by id; later milestones add theirs. Never U+00B7 (Overpass draws
it as a combining mark: separators are drawn dots, ae_widget_separator), and
every code point is in Overpass 900 (port/linux/tests/ae_strings_test.c checks
both). Texts with %d / %s are formats.
*/

#ifndef __AE_STRINGS_H
#define __AE_STRINGS_H

enum ae_string_id
{
	/* prompts */
	AE_STR_SELECT,
	AE_STR_BACK,
	AE_STR_CANCEL,
	AE_STR_DONE,
	AE_STR_SAVE,
	AE_STR_TYPE,
	AE_STR_ALL_VALUES,
	AE_STR_RESET_ROW,
	AE_STR_SEARCH,
	AE_STR_PAGES,
	AE_STR_TABS,
	AE_STR_CHANGE,
	AE_STR_PROFILE,
	AE_STR_QUIT,
	AE_STR_SWITCH,
	AE_STR_BACKSPACE,
	AE_STR_SPACE,
	AE_STR_MOVE_CARET,
	AE_STR_SHIFT,
	/* key caps (spec 4.10) */
	AE_STR_KEY_ENTER,
	AE_STR_KEY_ESC,
	AE_STR_KEY_CTRL_F,
	AE_STR_KEY_R,
	AE_STR_KEY_Q,
	AE_STR_KEY_E,
	AE_STR_KEY_PAGE_UP,
	AE_STR_KEY_PAGE_DOWN,
	AE_STR_KEY_TAB,
	AE_STR_KEY_CTRL_S,
	AE_STR_KEY_CTRL_SHIFT_S,
	AE_STR_CAP_OPTIONS,
	AE_STR_CAP_CREATE,
	/* rows, lists, help */
	AE_STR_MORE,
	AE_STR_VALUES,
	AE_STR_DEFAULT,
	AE_STR_CHANGED_FROM,
	AE_STR_RESET,
	AE_STR_BADGE_AE,
	AE_STR_BADGE_HOST,
	/* text entry */
	AE_STR_COUNT,
	AE_STR_KEY_SHIFT,
	AE_STR_KEY_SYMBOLS,
	AE_STR_KEY_LETTERS,
	AE_STR_KEY_SPACE,
	AE_STR_KEY_DONE,
	/* dialogs */
	AE_STR_REVERTING,
	AE_STR_KEEP,
	AE_STR_REVERT,
	AE_STR_OK,
	AE_STR_CANCEL_CAPS,
	AE_STR_LEAVE,
	AE_STR_STAY,
	/* roster */
	AE_STR_PRESS,
	AE_STR_TO_ADD_PLAYER,
	AE_STR_CONNECT_CONTROLLER,
	AE_STR_GUEST,
	AE_STR_CONTROLLER_N,
	AE_STR_KEYBOARD_MOUSE,
	AE_STR_DISCONNECTED,
	AE_STR_AWAY,
	AE_STR_PLAYER_N,
	/* the old menu off placeholder (spec 8) */
	AE_STR_KICKER,
	AE_STR_AE_TITLE,
	AE_STR_CAMPAIGN,
	AE_STR_CUSTOM_GAMES,
	AE_STR_SERVER_BROWSER,
	AE_STR_SETTINGS,
	/* glue reasons (Tasks 12-14) */
	AE_STR_ERR_NOT_HOSTING,
	AE_STR_ERR_LOBBY_FULL,
	AE_STR_ERR_NO_GAME,
	AE_STR_ERR_VERSION,
	AE_STR_ERR_MAP,
	AE_STR_ERR_GAMETYPE,
	AE_STR_ERR_HOST,
	AE_STR_ERR_PROFILE,
	AE_STR_ERR_GUEST,
	AE_STR_ERR_IN_GAME,
	/* (Task 13: joining an invite) */
	AE_STR_ERR_INTERNET_OFF,
	AE_STR_ERR_NOT_INVITE,
	AE_STR_ERR_INVITE_VERSION,
	AE_STR_ERR_NO_ANSWER,
	/* (Task 14: the profiles) */
	AE_STR_ERR_PROFILE_EDITING,
	AE_STR_ERR_NAME_EMPTY,
	AE_STR_ERR_NAME_LONG,
	AE_STR_ERR_NAME_TAKEN,
	AE_STR_ERR_PROFILE_NEW,
	AE_STR_ERR_SETTING,
	AE_STR_GUEST_NAME,
	AE_NUMBER_OF_STRINGS
};

/* the text of an id ("" for one out of range) */
const char *ae_string(int id);

#endif
