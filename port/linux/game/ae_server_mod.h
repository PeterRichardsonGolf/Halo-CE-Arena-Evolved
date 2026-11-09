#ifndef AE_SERVER_MOD_H
#define AE_SERVER_MOD_H

/*
AE_SERVER_MOD.H

The dedicated server's moderation depth and abuse limits (0.7.7; docs/dedicated_server.md): the door (a banned network,
the whitelist, the reserved slots), immunity, per-command access, a chat flood, a ping kick, bans by name, the event
hooks and the alerts. Rules: ae_server_mod_rules.c; logins and the webhook: ../src/ae_server_ops.c. The hooks are
no-ops outside a dedicated server.
*/

#include "../src/server_mode.h"

struct network_game_server;
struct server_json;

/* every frame, from the director: the files read again every so often, the events read for hooks and alerts, the players
checked once a second */
void ae_server_mod_update(void);
/* sv_reload: the files read again at once */
void ae_server_mod_reload(void);

/* (network_server_manager.c) a machine at the door, before it is let in: 1 if it is refused, with the rejection code */
int ae_server_mod_door(struct network_game_server *server, unsigned long address, unsigned long real_address,
	const char *hardware_id, const char *discord_id, short *code);

/* (network_chat.c) a line a machine's player sent: 1 if it was a flood (the player is muted, the line goes no further) */
int ae_server_mod_chat_flood(long machine_index, const char *name);

/* (server_commands.c) the role a command needs: command_access.txt's, else its own */
int ae_server_mod_command_role(const char *command, int own_role);
/* a command run: who runs it (a web session's "by"), for immunity */
void ae_server_mod_set_actor(const char *by);
/* the immunity of the acting machine as ae_server_ingame computed it (its machine's address row too), for the next run:
without it a key's immunity is looked up from the first machine that holds that key */
void ae_server_mod_set_actor_immunity(int immunity);
void ae_server_mod_run_begin(void);
void ae_server_mod_run_end(void);
/* whether the caller may act on the player in that slot / on an address, given the actor and the target's admin row */
int ae_server_mod_may_act_slot(long slot, int actor_role);
int ae_server_mod_may_act_address(unsigned long address, int actor_role);


/* (the commands' work: ae_server_mod_commands.inc) */
void ae_server_mod_rescan(void);
/* (ae_server_ingame.c) a machine's key signed in with a role: its hardware id passes the slots kept for admins from now on */
void ae_server_mod_admin_hardware_id(const char *hardware_id);
int ae_server_mod_ban_write(const char *network, const char *pattern, long seconds, const char *reason, int role, char *why,
	int why_size);
int ae_server_mod_whitelist_add(const char *text, char *why, int why_size);
int ae_server_mod_whitelist_remove(const char *text);
int ae_server_mod_whitelist_text(char *text, int size);
int ae_server_mod_access_text(char *text, int size);
void ae_server_mod_bans_export(struct server_json *json);
int ae_server_mod_bans_import(const char *body, int length, int *added, int *skipped, int *refused, char *why, int why_size);
int ae_server_mod_bans_export_file(const char *name, char *why, int why_size);
int ae_server_mod_bans_import_file(const char *name, int *added, int *skipped, int *refused, char *why, int why_size);

#endif
