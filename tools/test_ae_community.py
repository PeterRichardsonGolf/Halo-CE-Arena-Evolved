"""The daily challenge's community code (port/linux/game/community.c, port/linux/src/community_net.c) sends nothing unless
the player turned it on. Content Evolved defaults community.enabled to true; Arena Evolved's defaults are false, and a
request needs both the setting and the community services' consent. These checks read the sources, since the game's units
need the engine."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = (ROOT / "port/linux/src/port_config.c").read_text()
NET = (ROOT / "port/linux/src/community_net.c").read_text()
NET_H = (ROOT / "port/linux/src/community_net.h").read_text()
GAME = (ROOT / "port/linux/game/community.c").read_text()
MAIN = (ROOT / "source/main/main.c").read_text()


def row(name):
    match = re.search(r'\{ "%s", (_config_\w+), ("(?:[^"\\]|\\.)*"), ' % re.escape(name), CONFIG)
    assert match, name
    return match.group(1), match.group(2)


def test_community_defaults_are_off():
    assert row("community.enabled") == ("_config_boolean", '"false"')
    assert row("community.upload_scores") == ("_config_boolean", '"false"')
    assert row("community.key") == ("_config_string", '"\\"\\""')
    assert row("community.custom_server") == ("_config_string", '"\\"\\""')
    assert row("game.daily_challenge") == ("_config_boolean", '"false"')


def test_a_request_needs_the_setting_and_the_consent():
    enabled = re.search(r"int community_net_enabled\(void\)\s*\{(.*?)\n\}", NET, re.S).group(1)
    assert 'config_boolean("community.enabled")' in enabled and "ae_community_services_on()" in enabled
    send = re.search(r"int community_net_send\(.*?\n\}", NET, re.S).group(0)
    assert send.index("community_net_enabled()") < send.index("community_net_key_for(")
    assert send.index("community_net_enabled()") < send.index("community_net_start(")
    # the install's key is made only inside a request that passed the check above
    assert NET.count("community_net_key(") == 2


def test_only_the_community_server_is_asked():
    assert "community_net_get" not in NET and "community_net_get" not in GAME
    assert "api.github.com" not in GAME and "NEWS_URL" not in GAME
    assert set(re.findall(r"https://[a-z.]+", NET + NET_H)) <= {"https://", "https://oceapi.watanoge.com", "https://example.com"}


def test_no_personal_data_in_a_url():
    paths = re.findall(r'snprintf\(path, sizeof\(path\), ("[^"]*")(.*?)\);', GAME)
    assert paths
    for text, arguments in paths:
        assert "name" not in arguments and "player" not in arguments, text


def test_a_challenge_never_goes_on_into_multiplayer():
    update = re.search(r"void community_update\(boolean main_menu\)(.*)", GAME, re.S).group(1)
    assert "_scenario_type_solo" in update and "config_overrides_clear()" in update


def test_main_hooks_are_inert_without_a_challenge():
    assert "community_challenge_running()" in MAIN and "community_update(main_globals.main_menu_scenario_loaded)" in MAIN


SKULLS = (ROOT / "port/linux/game/skulls.c").read_text()


def test_a_challenge_sets_only_its_own_keys_and_the_progress_locally():
    reader = re.search(r"static void challenge_read\(.*?\n\}\n", GAME, re.S)
    body = reader.group(0) if reader else GAME
    assert "ae_daily_challenge_key_allowed(name)" in body
    assert 'strncmp(name, "game.skulls"' not in body and 'strncmp(name, "game.randomizer"' not in body
    for key, value in (("unlock_mode", "play"), ("all_unlocked", "true"), ("instant_unlock", "false")):
        assert 'local_setting("game.skulls_%s", "%s");' % (key, value) in body


def test_an_overridden_setting_is_never_written():
    write = re.search(r"int config_write\(const char \*name, const char \*value\)\n\{.*?\n\}\n", CONFIG, re.S).group(0)
    assert "config_write_overridden(index, name)" in write
    helper = re.search(r"static int config_write_overridden\(.*?\n", CONFIG).group(0)
    assert "config_overridden[index]" in helper and "return 1" in helper and "told[index]" in helper
    # the refusal comes before anything is written
    assert write.index("config_write_overridden") < write.index("config_write_file")


def test_no_skull_progress_is_written_during_a_challenge():
    keep = re.search(r"static void skulls_keep\(.*?\n\}\n", SKULLS, re.S).group(0)
    assert keep.index("community_challenge_running()") < keep.index("config_write")
    mode = re.search(r"int skulls_unlock_mode\(void\)\n\{.*?\n\}\n", SKULLS, re.S).group(0)
    assert "!community_challenge_running() && config_write" in mode
    # these are the only writes of skull progress in skulls.c
    writes = re.findall(r'config_write\("([a-z_.]+)"', SKULLS)
    assert sorted(set(writes)) == ["game.skulls_all_unlocked", "game.skulls_unlock_mode", "game.skulls_unlocked"]


def test_a_custom_server_never_gets_the_default_servers_key():
    send = re.search(r"int community_net_send\(.*?\n\}\n", NET, re.S).group(0)
    assert "community_net_key_for(server, key, sizeof(key))" in send and "community_net_key(key" not in send
    derive = re.search(r"static int community_net_key_for\(.*?\n\}\n", NET, re.S).group(0)
    # the install's key goes only where the server is the default one; the rest get an HMAC of the key and the address
    assert derive.index("COMMUNITY_NET_DEFAULT_SERVER") < derive.index("p2p_hmac_sha256")
    assert "snprintf(key, size, \"%s\", install)" in derive.split("p2p_hmac_sha256")[0]
    assert "server" in derive.split("p2p_hmac_sha256")[0].split("message")[-1]
    # the headers hold the longest key, the profile's part and the content type
    assert len(re.findall(r"char headers\[256\];", NET)) == 2


def test_a_request_has_a_total_deadline_and_takes_the_tls_lock_once():
    make = re.search(r"static int community_net_make\(.*?\n\}\n", NET, re.S).group(0)
    assert "#define COMMUNITY_NET_DEADLINE_MILLISECONDS 15000" in NET
    # update_https_request takes the Mbed TLS lock itself (ae_https_guard.c, not re-entrant): the request never holds it too
    assert "posix_browser_lock()" not in make and "posix_browser_unlock()" not in make
    guard = (ROOT / "port/linux/src/ae_https_guard.c").read_text()
    assert "posix_browser_lock();" in guard
    assert make.index("update_https_deadline(COMMUNITY_NET_DEADLINE_MILLISECONDS)") < make.index("update_https_request(") < \
        make.index("update_https_deadline(0)")
    posix = (ROOT / "port/linux/src/posix_update.c").read_text()
    fill = re.search(r"static int connection_fill\(.*?\n\}\n", posix, re.S).group(0)
    assert "wait_milliseconds()" in fill and "return -1" in fill.split("wait_milliseconds()")[1][:200]
    assert "net_connect(&connection->net, host, port)" in posix and "request_deadline ? connect_within_deadline" in posix


def test_a_challenge_ends_at_the_first_map_that_is_not_its_own():
    game = (ROOT / "source/game/game.c").read_text()
    load = re.search(r"boolean game_load\(.*?\n\}\n", game, re.S).group(0)
    assert "community_map_loading(options->map_name); } /* AE hook */" in load
    loading = re.search(r"void community_map_loading\(.*?\n\}\n", GAME, re.S).group(0)
    assert "ae_daily_map_keeps_settings(key, community.running_challenge.level)" in loading
    assert "config_overrides_clear();" in loading and "community.running = FALSE;" in loading
    # a hosted lobby keeps them past the menus only once the level was played
    update = re.search(r"void community_update\(boolean main_menu\)\n\{.*?\n\}\n", GAME, re.S).group(0)
    assert "global_network_game_server_get() && community.running_loaded" in update


def test_a_servers_text_is_checked_before_it_goes_back_into_a_request():
    read = re.search(r"static void challenge_read\(.*?\n\}\n", GAME, re.S).group(0)
    assert "ae_daily_day_valid(challenge->day)" in read
    assert "skull_of_key(challenge->skulls[challenge->skull_count]) != NONE" in read
    history = re.search(r"static void history_read\(.*?\n\}\n", GAME, re.S).group(0)
    assert "ae_daily_day_valid(day->day)" in history
    # \u takes exactly four hex digits, no strlen of the rest of the answer for each
    assert "strlen(value) >= 5" not in GAME and "isxdigit((unsigned char)value[4])" in GAME
    # dates from a server are clamped before the arithmetic
    civil = re.search(r"static long days_from_civil\(.*?\n\}\n", GAME, re.S).group(0)
    assert "PIN(year, 2000, 2100)" in civil
    iso = re.search(r"static long long iso_seconds\(.*?\n\}\n", GAME, re.S).group(0)
    assert "hour > 23" in iso
