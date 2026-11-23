/*
AE_SCREEN_COMMUNITY.H

AE's COMMUNITY SERVICES page (ae_screen_community.c; 0.7.4): the question asked once, while network.community_services
is "ask", in front of AE's main menu (ae_community_screen.c says when; ae_hooks.c opens it in the main menu's place and
the main menu waits until it is answered). A full page of AE's row screens: on the left what the two services do (SERVER
BROWSER with its LIST GAMES I HOST row, GAME STATS) and DECLINE / ACCEPT, on the right who runs them and where to change
the answer. DECLINE has the focus first; an opt-in is not answered by a stray A. The answer is written at once
(ae_community_write_answer).
*/

#ifndef __AE_SCREEN_COMMUNITY_H
#define __AE_SCREEN_COMMUNITY_H

/* opens it over an empty stack; 0 when a screen is up or the stack is full */
int ae_screen_community_open(void);

#endif
