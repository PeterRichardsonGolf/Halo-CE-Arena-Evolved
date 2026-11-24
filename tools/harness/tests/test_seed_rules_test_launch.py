"""A built-in gametype's AE block (the seed rules, a preset's table) through a TEST RUN map test's launch: network_test.c's
network_test_saved_gametype, ae_glue_setup_options.c's ae_setup_edit_begin_rules and ae_setup_edit_apply, ae_h2_presets.c's
ae_h2_preset_choose_variant."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

NETWORK_CHOICE = ('{ struct ae_variant_ext built_in; ae_h2_preset_ext_of_name(variant->human_readable_game_description, '
                  '&built_in); ae_variant_ext_choose(&built_in); } /* AE hook */')
PRESET_CHOICE = 'ae_h2_preset_ext_of_name(variant->human_readable_game_description, &setup_edit.edit.ext);'


def generated(network_fault=None, setup_fault=None):
    ext = read('port/linux/game/ae_variant_ext.c')
    presets = read('port/linux/game/ae_h2_presets.c')
    glue = read('port/linux/game/ae_glue_setup_options.c')
    network = read('port/linux/game/network_test.c')
    state = glue[glue.index('struct rules {'):glue.index('} setup_edit;') + len('} setup_edit;')]
    setup = (state + '\n' + function(glue, 'narrow_name') + function(glue, 'ae_setup_edit_begin_rules') +
             function(glue, 'ae_setup_edit_apply'))
    found = function(network, 'network_test_saved_gametype')
    if network_fault:
        found = mutated(found, *network_fault)
    if setup_fault:
        setup = mutated(setup, *setup_fault)
    return (('ae_variant_ext_rules.h', read('port/linux/game/ae_variant_ext_rules.h')),
            ('ae_seed_ext_rules.h', read('port/linux/game/ae_seed_ext_rules.h')),
            ('rules.inc', read('port/linux/game/ae_variant_ext_rules.c') + read('port/linux/game/ae_seed_ext_rules.c')),
            ('ext.inc', function(ext, 'env_override') + function(ext, 'ae_variant_ext_choose') +
             function(ext, 'ae_variant_ext_game')),
            ('presets.inc', function(presets, 'ae_h2_preset_ext_of_name') + function(presets, 'ae_h2_preset_choose_variant')),
            ('setup.inc', setup),
            ('network.inc', found))


@pytest.mark.parametrize('case', ['ctf', 'slayer', 'comp', 'again', 'after', 'preset'])
def test_case(case):
    status, output = run(build('seed_rules_test_launch', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('network_fault,setup_fault,case', [
    # (a built-in found by name chooses no block: the GAME OPTIONS copy opens on Halo CE's rules and applies them)
    ((NETWORK_CHOICE, 'ae_variant_ext_choose(NULL); /* AE hook */'), None, 'ctf'),
    ((NETWORK_CHOICE, 'ae_variant_ext_choose(NULL); /* AE hook */'), None, 'slayer'),
    ((NETWORK_CHOICE, 'ae_variant_ext_choose(NULL); /* AE hook */'), None, 'again'),
    # (no choice at all: the last game's block carries over)
    ((NETWORK_CHOICE, '/* AE hook */'), None, 'after'),
    # (a preset's copy takes its table alone, without the seed rules)
    (None, (PRESET_CHOICE, 'ae_h2_preset_ext(ae_h2_preset_find_wide(variant->human_readable_game_description), '
                           '&setup_edit.edit.ext);'), 'preset'),
])
def test_negative_control(network_fault, setup_fault, case):
    status, output = run(build('seed_rules_test_launch', generated(network_fault, setup_fault)), case)
    assert status == CHECK_FAILED, output
