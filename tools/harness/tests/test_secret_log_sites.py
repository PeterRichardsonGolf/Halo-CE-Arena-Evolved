"""Every place a command line is logged before the server runs it keeps a secret setting's line out of the log
(server_console_line_is_secret, ae_secret_settings.h), and DEV MODE's config command logs a secret setting's line and
answer without its value."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import function, mutated, read  # noqa: E402

# the functions that log a command line and then run it: file, function, the line's variable
RUNNERS = [
    ("port/linux/game/server_director.c", "server_run_line", "line"),
    ("port/linux/src/server_http.c", "ticket_run_line", "line"),
    ("port/linux/game/ae_server_mod.c", "run_hook_command", "command"),
]
LOGS = re.compile(r"\bserver_log(?:_private)?\((.*?)\);", re.S)


def logged_bare(code, variable):
    """The server_log calls in code that print variable without asking whether it is a secret's line."""
    bare = []
    for match in LOGS.finditer(code):
        arguments = match.group(1)
        if re.search(rf"(?<![\w.>])\b{variable}\b(?!\s*\[)", arguments) and \
                f"server_console_line_is_secret({variable})" not in arguments:
            bare.append(match.group(0))
    return bare


def runner_code(path, name):
    source = read(path)
    try:
        return function(source, name)
    except LookupError:
        pytest.fail(f"{path}: {name} not found (renamed? this test lists the functions that log a line, then run it)")


@pytest.mark.parametrize("path,name,variable", RUNNERS)
def test_a_logged_line_is_asked_about_first(path, name, variable):
    code = runner_code(path, name)
    assert "server_commands_run(" in code, f"{name} no longer runs the line"
    assert not logged_bare(code, variable), f"{path} {name}: a line logged whole: {logged_bare(code, variable)}"


def test_every_line_runner_is_listed():
    # (a caller of server_commands_run not in RUNNERS logs no line of its own whole before it: ae_server_ingame.c's
    # lines are made by the server, from the action asked, and are logged as made)
    listed = {path: [] for path, _, _ in RUNNERS}
    for path, name, _ in RUNNERS:
        listed[path].append(runner_code(path, name))
    for path in ("port/linux/game/server_director.c", "port/linux/src/server_http.c", "port/linux/game/ae_server_mod.c"):
        source = read(path)
        for match in re.finditer(r"(?<!\*)server_commands_run\(\s*(\w+),", source):
            if any(code in source and source.index(code) <= match.start() < source.index(code) + len(code)
                   for code in listed[path]):
                continue
            body = source[source.rindex("\n}\n", 0, match.start()):match.start()]
            assert not logged_bare(body, match.group(1)), f"{path}: a line logged whole before it is run"


def test_negative_control_the_hook_line_logged_whole():
    code = runner_code("port/linux/game/ae_server_mod.c", "run_hook_command")
    old = mutated(code, 'server_console_line_is_secret(command) ? "(a password\'s command)" :\n\t\tcommand', "command")
    assert logged_bare(old, "command")


DEV = "port/linux/game/ae_dev_cmd.c"


def dev_hides(source):
    """Whether DEV MODE's config command keeps a secret setting's value out of the log and the event stream."""
    answer = function(source, "answer")
    # (the log and the event stream show the line without its value; the TEST RUN answers keep the line as sent)
    shown = "answering_shown ? answering_shown : line, ok" in answer and \
        "ae_dev_event_text(line_text, sizeof line_text, answering_shown ? answering_shown : line)" in answer
    config = source[source.index("\tcase AE_DEV_CMD_CONFIG:"):]
    config = config[:config.index("\t\tbreak;")]
    asked = "ae_setting_is_secret(cmd->word[0]" in config and 'secret ? "(set)" : cmd->rest' in config and \
        "answering_shown = secret ? shown : NULL;" in config
    return shown and asked and "ae_setting_is_secret(key" in function(source, "setting_put_back")


def test_dev_config_hides_a_secret_value():
    assert dev_hides(read(DEV))


@pytest.mark.parametrize("fault", [
    ("answering_shown ? answering_shown : line, ok", "line, ok"),
    ('secret ? "(set)" : cmd->rest', "cmd->rest"),
])
def test_negative_control_dev_line_or_answer_logged(fault):
    assert not dev_hides(mutated(read(DEV), *fault))
