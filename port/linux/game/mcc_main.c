/* MCC transitions use the native menu lifecycle, without selecting Xbox
 * campaign progression or its credits movie for a standalone MCC scenario. */
#include <stdlib.h>
#include <string.h>
#include "cseries.h"
#include "errors.h"
#include "main/main.h"
#include "interface/ui_widget.h"
#include "networking/network_game_globals.h"
#include "mcc_cache.h"
#include "mcc_main.h"
#include "mcc_errors.h"
#include "server_director.h"

boolean mcc_main_map_completed(void)
{
    if (!mcc_cache_tags_loaded()) return FALSE;
    /* Unknown campaign levels reach main_roll_credits. MCC packages have
     * no native next-level mapping: return normally instead of starting the
     * Xbox outro immediately after menu precaching. That movie borrows and
     * protects texture-cache memory while menu reads may still be pending. */
    error(_error_silent, "mcc: scenario completed; returning to the menu (select the next campaign map manually)");
    main_goto_main_menu();
    return TRUE;
}

boolean mcc_main_load_failed(char const *name)
{
    wchar_t text[512];
    if (!mcc_level_name(name)) return FALSE;
    error(_error_silent, "mcc: returning to the menu after failed load of %s", name);
    mcc_error_text(name,"Its level could not be prepared. See debug.txt for the failing setup check.",text,512);
    display_error_text_when_main_menu_loaded(text);
    errors_clear();
    main_goto_main_menu();
    main_menu_load();
    return TRUE;
}

boolean mcc_main_network_load_failed(char const *name)
{
    wchar_t text[512];
    if (!mcc_level_name(name)) return FALSE;
    /* (0.7.9) a dedicated server keeps its session: back to the lobby with its players, the entry marked failed */
    if (server_director_load_failed(name))
    {
        error(_error_silent, "mcc: %s did not load: the server is back in its lobby", name);
        errors_clear();
        return TRUE;
    }
    error(_error_silent, "mcc: ending the network session after failed load of %s", name);
    mcc_error_text(name,"Its level could not be prepared. See debug.txt for the failing setup check.",text,512);
    display_error_text_when_main_menu_loaded(text);
    errors_clear();
    /* This runs inside a network receive callback. Queue both operations:
     * the next frame needs the menu's globals before input_abstraction_update,
     * which runs before the normal network abort is processed. Do not free
     * the connection or load the menu on this receive stack. */
    network_game_abort();
    main_goto_main_menu();
    return TRUE;
}

/* debug: HALO_TEST_MCC_LOAD_FAIL=<map name> makes that map's load fail (game_load), for the dedicated server's
return to its lobby (no effect without it) */
int mcc_main_test_load_fail(char const *name)
{
    char const *wanted = getenv("HALO_TEST_MCC_LOAD_FAIL");
    return wanted && wanted[0] && name && !strcmp(wanted, name);
}
