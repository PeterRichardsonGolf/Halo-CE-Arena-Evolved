/*
AE_SCREEN_HOST_SERVER.H

MULTIPLAYER's HOST A SERVER page (0.7.7; ae_screen_host_server.c): a dedicated server started on this PC from the
menus, and MY SERVER once it runs.
*/

#ifndef __AE_SCREEN_HOST_SERVER_H
#define __AE_SCREEN_HOST_SERVER_H

/* opens the page over the stack (crumb: the screen it came from): 0 when the stack is full */
int ae_screen_host_server_open(char const *crumb);
/* the MULTIPLAYER row's value while a server of this PC runs or starts ("RUNNING  •  6/16"): 1 and it in value, else 0 */
int ae_screen_host_server_row(char *value, int size);

#endif
