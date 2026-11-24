/*
AE_SCRIPT_NOTICE.H

SCRIPT NOTICE: see ae_script_notice_rules.h. The MCC script check (mcc_syntax.c) hands over how many of a level's
scripts it switched off; the campaign tick (ae_campaign_score_tick) says so once at the level's start.
*/

#ifndef AE_SCRIPT_NOTICE_H
#define AE_SCRIPT_NOTICE_H

/* the level just checked had this many scripts switched off (0: none); the line is shown once for it */
void ae_script_notice_set(long refused);

/* whether the debug setting debug.script_refuse names this script: the check then refuses it (DEV MODE's script_refuse) */
int ae_script_notice_script_forced(char const *script_name);

/* a campaign tick: the line, once, when it is due */
void ae_script_notice_tick(void);

/* one line for the DEV MODE command: the script named, the count of the loaded level, whether its line was shown */
void ae_script_notice_status(char *text, unsigned long size);

#endif
