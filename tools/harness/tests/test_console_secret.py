"""Which command lines the dedicated server keeps out of its log (port/linux/src/server_console.c): sv_password, and
sv_set of a secret setting (ae_secret_settings.h: a password's, the publisher key's, a webhook's), matched in any
case as the commands are."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["secrets"]
NAMES = ["word_is", "holds_word", "next_word", "server_console_line_is_secret"]

# a fault in the code, the function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "case-matters": (("tolower((unsigned char)word[index]) != tolower((unsigned char)text[index])",
                      "word[index] != text[index]"), "word_is", "secrets"),
    "prefix-is-enough": (("return index == length && !text[index];", "return index == length;"), "word_is",
                         "secrets"),
    # (the shared rule and the console's own line both say it: the fault takes it out of both)
    "publisher-key-logged": ((' || ae_secret_name_holds(name, length, "publisher_key")', ""),
                             "ae_secret_settings.h", "secrets"),
    "quotes-kept": (("\t\t*word = ++at;", "\t\t*word = at++;"), "next_word", "secrets"),
}


def generated(fault=None, faulty_function=None):
    source = read("port/linux/src/server_console.c")
    # (the one rule of which settings are secret, which the console's check asks first)
    text = read("port/linux/src/ae_secret_settings.h") + "\n"
    if fault and faulty_function == "ae_secret_settings.h":
        text = mutated(text, *fault)
        fault, faulty_function = (' || holds_word(setting, setting_length, "publisher_key")', ""), \
            "server_console_line_is_secret"
    for name in NAMES:
        code = function(source, name)
        if fault and name == faulty_function:
            code = mutated(code, *fault)
        text += code + "\n"
    return (("under_test.inc", text),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("console_secret", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("console_secret", generated(fault, faulty_function)), case)
    assert status == CHECK_FAILED, f"the code with a fault ({control}) passed '{case}': the test cannot see it"
