"""Every setting the settings side treats as secret is hidden in the console log too (and the other way round)."""
import re
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import build, function, mutated, read

CONSOLE_HOOK = '\tif (ae_setting_is_secret(setting, setting_length)) return 1; /* AE hook */\n'
EXPECTED_SECRET = ['server.password', 'server.web_password', 'server.web_owner_password',
                   'server.web_moderator_password', 'server.web_viewer_password', 'server.alert_webhook']


def setting_names():
    names = re.findall(r'^\t\{ "([a-z0-9_.]+)", _config_', read('port/linux/src/port_config.c'), re.M)
    assert len(names) > 100, 'port_config.c settings table not found'
    return sorted(set(names))


def generated(old_console=False):
    console = read('port/linux/src/server_console.c')
    commands = read('port/linux/game/server_commands.c')
    code = read('port/linux/src/ae_secret_settings.h') + '\n'
    for name in ('word_is', 'holds_word', 'next_word', 'server_console_line_is_secret'):
        code += function(console, name) + '\n'
    if old_console:
        code = mutated(code, CONSOLE_HOOK, '')
    code += function(commands, 'server_commands_setting_secret') + '\n'
    return (('under_test.inc', code),)


def views(executable):
    output = subprocess.run([str(executable), *setting_names()], capture_output=True, text=True, timeout=60).stdout
    return {name: (settings == '1', console == '1') for name, settings, console in
            (line.split() for line in output.splitlines())}


def test_every_setting_agrees():
    seen = views(build('secret_settings', generated()))
    assert len(seen) == len(setting_names())
    disagree = [name for name, (settings, console) in seen.items() if settings != console]
    assert not disagree, f'shown on one side, hidden on the other: {disagree}'


@pytest.mark.parametrize('name', EXPECTED_SECRET)
def test_secret(name):
    seen = views(build('secret_settings', generated()))
    assert seen[name] == (True, True), f'{name}: settings secret {seen[name][0]}, hidden in the log {seen[name][1]}'


def test_ordinary_settings_shown():
    seen = views(build('secret_settings', generated()))
    for name in ('server.name', 'server.max_players', 'server.motd', 'server.alert_disk_mb'):
        assert seen[name] == (False, False), name


def test_negative_control_webhook_logged_without_the_hook():
    seen = views(build('secret_settings', generated(old_console=True)))
    assert seen['server.alert_webhook'] == (True, False)
