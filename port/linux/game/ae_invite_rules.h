/*
AE_INVITE_RULES.H

An internet game's invite as a player gives it to JOIN WITH INVITE (ae_screen_join_invite.c) and the lobby's join
(ae_glue_lobby.c ae_lobby_join_address): a link, halo://join/<code>, or the code alone. Pure, no engine includes
(unit test: port/linux/tests/ae_invite_rules_test.c).
*/

#ifndef __AE_INVITE_RULES_H
#define __AE_INVITE_RULES_H

#include <stddef.h>

/* the code's digits: the host's key hash and the token, 16 bytes each in hexadecimal (p2p_internal.h) */
enum { AE_INVITE_CODE_DIGITS = 64 };

/* the invite's code in text (spaces, tabs and line ends around it ignored), as p2p.c parse_invite reads it: after
"halo://join/" (any case) when the text holds a link, else the whole text is the code. Into code (size > 64), lower
case. 1 found; 0 for anything else (a key link, an older or cut invite, other text) */
int ae_invite_code(const char *text, char *code, size_t size);

#endif
