/*
AE_INVITE_SHARE.H

(0.7.6) The game's invite link, for every place that offers to copy it: CREATE GAME's INVITE row and MY PARTY's INVITE
FRIEND, the pause menu's INVITE A FRIEND (a host's, and a joiner's: a joined PC may copy the host's invite too). The
host's link is the one its game made (p2p_invite_link); a joiner's is the invite its PC joined through
(p2p_joined_invite), as halo://join/<code>. A copy puts the link on the clipboard and keeps the "COPIED" state for a
few seconds, which the rows and the party's notice show.
*/

#ifndef AE_INVITE_SHARE_H
#define AE_INVITE_SHARE_H

/* how long COPIED shows after a copy (ms) */
enum { AE_INVITE_COPIED_MS = 3000 };

/* the link of the game this PC is in (host or joiner) into link; 1 when there is one (a LAN or offline game, or a
join with no invite, has none) */
int ae_invite_share_link(char *link, int size);
/* the link to the clipboard, COPIED started; 1 when it was copied */
int ae_invite_share_copy(void);
/* the value an INVITE row shows: "COPIED" in the copy's window, else what idle says */
int ae_invite_share_copied(void);
/* the copy's notice (the quiet card under MY PARTY): until when it shows (0: none) */
unsigned long ae_invite_share_notice_until(void);

#endif
