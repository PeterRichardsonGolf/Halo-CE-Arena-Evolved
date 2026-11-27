/*
AE_SCREEN_DAILY_SERVER.H

SERVER ADDRESS (ae_screen_daily_server.c): the address of another community's server for DAILY CHALLENGE (community.custom_server),
typed in a field, https only (ae_daily_server_clean). Opened from SETTINGS > CAMPAIGN > DAILY CHALLENGE > SERVER ADDRESS.
*/

#ifndef __AE_SCREEN_DAILY_SERVER_H
#define __AE_SCREEN_DAILY_SERVER_H

/* opens it over the stack; 0 when the stack is full */
int ae_screen_daily_server_open(void);

/* the row's value in SETTINGS: the address, "NOT SET" or "USES DEFAULT" */
void ae_screen_daily_server_summary(char *text, int size);

#endif
