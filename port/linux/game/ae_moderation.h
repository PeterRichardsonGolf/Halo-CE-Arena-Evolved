/*
AE_MODERATION.H

(0.7.6 HOST MODERATION) What the host does to a player of their game: KICK FOR 2 MINUTES, BAN FROM MY GAMES, and the
BANNED PLAYERS list that undoes a ban. The engine's own calls (network_server_manager.c: a kick drops the machine, a
ban writes bans.txt by address and hardware id and keeps every join out); AE adds the 2-minute rejoin block of a kick,
held in memory (the machine's address and hardware id, with an expiry), and one chat line to everyone.
*/

#ifndef AE_MODERATION_H
#define AE_MODERATION_H

enum { AE_MODERATION_KICK_SECONDS = 120, AE_MODERATION_NAME_SIZE = 48 };

/* (the host's join check, network_server_manager.c) whether a machine of this address (host byte order) and hardware
id is in its 2 minutes after a kick; *refusal_code is then the engine's "kept out" code. Nothing for the host's own */
int ae_moderation_kept_out(unsigned long address, char const *hardware_id, short *refusal_code);

/* (0.7.7, a dedicated server's kick from the game: ae_server_ingame.c) a machine's address and hardware id, read before
it is dropped; and those kept out for AE_MODERATION_KICK_SECONDS, as the host's kick does */
void ae_moderation_hold_machine(short machine, unsigned long *address, char *hardware_id, int hardware_id_size);
void ae_moderation_hold(unsigned long address, char const *hardware_id);

/* (0.7.7 MUTE IN CHAT, the approved F-1: player-hosted games too) the host mutes a player's chat and voice for everyone
until the game ends (the chat's default minutes), or lets them speak again; one chat line says so. TRUE when the chat
took it. muted: whether they are muted now */
int ae_moderation_mute(char const *name, int mute);
int ae_moderation_muted(char const *name);

/* the seconds a kicked machine still waits (0: not held) */
long ae_moderation_wait_seconds(unsigned long address, char const *hardware_id);

/* the host kicks the person at this network machine for 2 minutes / bans them: TRUE when the engine took it (the chat
line says "<host> kicked <name> for 2 minutes" and "<host> banned <name>"; text, when given, says why not) */
int ae_moderation_kick(short machine, char const *name, char const *host, char *text, int size);
int ae_moderation_ban(short machine, char const *name, char const *host, char *text, int size);

/* BANNED PLAYERS: the bans of bans.txt that still hold, as the list shows them */
enum { AE_MODERATION_BANS_MAXIMUM = 256 };
struct ae_moderation_ban
{
	long number;                                  /* its place in the file's bans (what unban takes) */
	char name[AE_MODERATION_NAME_SIZE];           /* the players' names at the ban ("Goat"), else its address */
	char when[24];                                /* "2026-11-04 18:03" */
	unsigned long until;                          /* seconds since 1970; 0: for ever */
};
long ae_moderation_bans(struct ae_moderation_ban *bans, long maximum);
/* the ban's line taken out of bans.txt (its number from the list): TRUE when it was */
int ae_moderation_unban(long number);

/* the time of a ban made now, for the list's "2 min ago" style: seconds since the ban line's date (-1 unknown) */
long ae_moderation_ban_age_seconds(struct ae_moderation_ban const *ban);

/* whether the host may kick or ban this player: a person of another PC (not a bot, not one of the host's own PC).
HALO_AE_MODERATION_TEST=1 also lets a bot through, so the card and its confirms can be looked at in a headless game
(the engine then refuses the kick: no such machine joined) */
int ae_moderation_can_kick(int bot, int local, int host);

/* the confirm dialog's words for a kick or a ban of the person called name: its title ("Kick Goat?"), its consequences
and the destructive choice ("KICK GOAT"); the ban names where to undo it */
void ae_moderation_confirm_text(int ban, char const *name, char *title, int title_size, char *body, int body_size,
	char *yes, int yes_size);

/* the wait as "1:58" */
void ae_moderation_wait_text(long seconds, char *text, int size);

#endif
