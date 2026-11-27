/*
AE_SCREEN_BOT_RECORD.H

(0.9.4) BOT RECORD: the SERVICE RECORD page of one AE TOURNAMENT bot (notes/specs/2026-11-15-bot-service-records.md
section 4.2, mockups B-2, B-3, S-1), on the SERVICE RECORD page's grid (ae_screen_record.c):
- the head: SERVICE RECORD, a dot, BOT; the bot's name with an outlined BOT chip; "MET IN N TOURNAMENT GAMES ON THIS PC",
  a dot, "FIRST IN <PLAYLIST>";
- THIS GAME (opened from the post-game report: the game's own numbers) or LAST GAME TOGETHER (from CAREER: the last game
  of the first profile with the bot);
- CAREER TOTALS (every tournament game on this PC, with any profile), IDENTITY (skill, tier, style, favourite weapon, the
  weapon of the most kills) with the BOT RANK slot a later version fills;
- VS YOU: a line for each profile of the page (split screen: every local profile of the game), the rival line's words
  (ae_bot_record_rival_line) and the numbers against and with it.
(rc2) Three pages, OVERVIEW | COMBAT | MODES & MAPS (ae_record_pages.h), as a player's record has, gone through with LB / RB (Q / E, Y or Tab too) or a click on the chips of the head. The D-pad, arrows or stick move a focus between
the boxes that have a DETAILS page (ae_record_focus.h; the ice-blue ring); A or a click opens its page of the detail window (ae_screen_record_window.h; the
MEDALS box opens the MEDALS page of the bot's medals; VS YOU its whole versus record, ae_screen_record_details.h), B backs out to the box. LT / RT (Page Up /
Page Down) step to the next bot of the board (or of the CAREER list), in its order, wrapping, the page and the focused box kept.

Log: "ae botrecord: page <name> (<n> games)" and "ae botrecord: page closed"; DEV MODE event bot_record_page.
*/

#ifndef __AE_SCREEN_BOT_RECORD_H
#define __AE_SCREEN_BOT_RECORD_H

#include "ae_bot_record_rules.h"

enum
{
	/* the bots of a page with THIS GAME's numbers (the board's first ones), and its profiles */
	AE_BOT_PAGE_GAMES = 32,
	AE_BOT_PAGE_PROFILES = 4
};

/* THIS GAME of one bot, from the post-game report */
struct ae_bot_page_game
{
	int has;
	char result[8];                 /* "WIN", "LOSS", "TIE", a free for all's "3RD" */
	long kills, deaths, assists;
	long spree;                     /* < 0: not known (no summary) */
	char weapon[AE_BOT_RECORD_WEAPON]; /* the summary's first weapon, "" for none */
	long its_kills_on[AE_BOT_PAGE_PROFILES]; /* its kills on each profile of the page, in their order */
};

/* what a page shows: its bots in their order (the board's, or the CAREER list's) and the profiles of VS YOU */
struct ae_bot_page_source
{
	int from_career;
	int count;
	char name[AE_BOT_RECORD_BOTS][AE_BOT_RECORD_KEY];
	struct ae_bot_page_game game[AE_BOT_PAGE_GAMES];
	int profiles;
	char profile_key[AE_BOT_PAGE_PROFILES][AE_BOT_RECORD_KEY];
	char profile_name[AE_BOT_PAGE_PROFILES][AE_BOT_RECORD_KEY];
	unsigned int profile_chip[AE_BOT_PAGE_PROFILES]; /* the profile's colour, 0xRRGGBBAA */
	int profile_player[AE_BOT_PAGE_PROFILES];        /* its player's number (1-4), in its chip */
	/* THIS GAME's head after its label: the map and the playlist ("" for none) */
	char map[48], playlist[48];
};

/* the source the next page opens on, cleared: fill it, then open. NULL while a page is open (its source is in use) */
struct ae_bot_page_source *ae_screen_bot_record_source(void);
/* opens the page of the source's bot `index` over the stack, on page first_page (AE_RP_OVERVIEW, _COMBAT or _MODES: ae_record_pages.h).
   0 when a page is open, the source is empty or the stack is full */
int ae_screen_bot_record_open(short owner, int index, int first_page);
/* (DEV MODE) whether a page is open; an open page leaves */
int ae_screen_bot_record_is_open(void);
void ae_screen_bot_record_remove(void);

#endif
