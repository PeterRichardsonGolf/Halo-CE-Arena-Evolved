/* A host's named sleep_forever on a scenario script of an MCC map, to the clients.
 * The map's startup check reads units (the host's); a client that loaded or joined
 * after the host's check sees other units and would run the script the host
 * stopped (a talking timer on every client). The host sends its set of stopped
 * scripts every half second, so a client that joins later gets it as well; the
 * client stops its own copy once, when that thread exists. */
#include "cseries.h"
#include "ae_script_sleep_network.h"
#include "ae_script_sleep_rules.h"
#include "mcc_cache.h"
#include "game/game.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "hs/hs_scenario_definitions.h"
#include "network_distributed.h"
#include <stdio.h>
#include <string.h>

struct ae_script_sleep_entry
{
    byte scripts[2]; /* the map's script count, low byte first */
    byte bits[AE_SLEEP_BYTES];
};
typedef char ae_script_sleep_entry_size_check[sizeof(struct ae_script_sleep_entry) == 62 ? 1 : -1];

boolean mcc_hs_sleep_script_remote(short script);
void platform_log(char const *format, ...);

static struct ae_script_sleeps host_sleeps, applied_sleeps;

void ae_script_sleep_network_new_game(void)
{
    ae_sleeps_clear(&host_sleeps);
    ae_sleeps_clear(&applied_sleeps);
}

void ae_script_sleep_network_note(short script)
{
    ae_sleeps_mark(&host_sleeps, script);
}

word ae_script_sleep_network_entry_size(void) { return sizeof(struct ae_script_sleep_entry); }

void ae_script_sleep_network_host_tick(void)
{
    struct {
        struct distributed_message_header header;
        struct ae_script_sleep_entry entry;
    } message;
    long scripts;
    if (game_connection() != _game_connection_network_server || game_time_get() % 15 ||
        !ae_sleeps_any(&host_sleeps) || !global_scenario_get()) return;
    scripts = global_scenario_get()->hs_scripts.count;
    memset(&message, 0, sizeof(message));
    message.entry.scripts[0] = (byte)(scripts & 0xFF);
    message.entry.scripts[1] = (byte)((scripts >> 8) & 0xFF);
    memcpy(message.entry.bits, host_sleeps.bits, sizeof(message.entry.bits));
    distributed_send(&message, _distributed_message_ae_script_sleep, 1, sizeof(message), _distributed_to_clients);
}

void ae_script_sleep_network_receive(void const *entries, short count)
{
    struct ae_script_sleep_entry entry;
    struct ae_script_sleeps host;
    struct scenario *scenario;
    char names[8][32];
    long scripts, script = -1, applied = 0, shown = 0;
    if (game_connection() != _game_connection_network_client || !mcc_cache_tags_loaded() || count != 1 ||
        !(scenario = global_scenario_get())) return;
    memcpy(&entry, entries, sizeof(entry));
    scripts = entry.scripts[0] | (entry.scripts[1] << 8);
    if (!ae_sleeps_entry_valid(scripts, scenario->hs_scripts.count)) return;
    memcpy(host.bits, entry.bits, sizeof(host.bits));
    while ((script = ae_sleeps_next_pending(&host, &applied_sleeps, script + 1)) != -1) {
        if (!mcc_hs_sleep_script_remote((short)script)) continue; /* its thread is not here yet: next time */
        ae_sleeps_mark(&applied_sleeps, script);
        applied++;
        if (shown < 8) {
            strncpy(names[shown], TAG_BLOCK_GET_ELEMENT(&scenario->hs_scripts, script, struct hs_script)->name, 31);
            names[shown++][31] = 0;
        }
    }
    if (applied) {
        char line[8 * 33 + 96];
        long i, used;
        used = snprintf(line, sizeof(line), "script sleep: applied host's sleeps (%ld):", applied);
        for (i = 0; i < shown && used < (long)sizeof(line); i++)
            used += snprintf(line + used, sizeof(line) - (size_t)used, " %s", names[i]);
        platform_log("%s", line);
    }
}
