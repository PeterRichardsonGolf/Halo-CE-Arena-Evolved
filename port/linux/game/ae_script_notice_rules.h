/*
AE_SCRIPT_NOTICE_RULES.H

SCRIPT NOTICE: a campaign level whose scripts were switched off by the MCC script check (mcc_syntax.c) says so once at
its start, in the HUD's message list (hud_print_message): "N of this map's scripts were switched off;" and, on the
next line, "the mission may not play correctly" (a message holds 62 characters, so the sentence is two lines).
Nothing for none. Pure (no engine includes; port/linux/tests/ae_script_notice_rules_test.c).
*/

#ifndef AE_SCRIPT_NOTICE_RULES_H
#define AE_SCRIPT_NOTICE_RULES_H

/* the game tick (30 a second) from which the line is shown: after the level's first second */
#define AE_SCRIPT_NOTICE_TICK 30
#define AE_SCRIPT_NOTICE_LINE_SIZE 64

/* the two lines for a count of refused scripts: 2, or 0 (and empty lines) when there is nothing to tell (count 0 or less).
"1 of this map's scripts was switched off;" for one, "N ... were ..." for more */
int ae_script_notice_lines(long refused, char first[AE_SCRIPT_NOTICE_LINE_SIZE], char second[AE_SCRIPT_NOTICE_LINE_SIZE]);

/* whether the line is due this tick: scripts refused, not shown yet for this level, a campaign level running (no
game engine), the HUD shown (a cinematic or a scripted camera hides it: the line waits for play) and the level's first second past */
int ae_script_notice_due(long refused, int shown, int campaign_level_running, int hud_hidden, long game_ticks);

/* whether the script of this name is the one the debug setting names (case-insensitive; an empty setting names none) */
int ae_script_notice_forced(const char *setting, const char *script_name);

#endif
