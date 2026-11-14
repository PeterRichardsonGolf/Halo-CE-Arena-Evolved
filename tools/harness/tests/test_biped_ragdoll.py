"""A body's ragdoll in a multiplayer game (port/linux/game/biped_ragdoll.c): it falls onto the ground in one piece and
comes to rest, its nodes turn with its bones, and a blow turns it. The styles' rows (spaghetti_western, exaggerated, modern,
mixed), when each goes limp, its blasts, its loosening joints and modern's kick."""
import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, enum_with, function, mutated, read, run, structure

CASES = ['falls_and_rests', 'nodes_follow_bones', 'push_turns', 'thrown', 'knees_bend_one_way', 'hips_swing_so_far',
         'torso_heavier', 'limbs_keep_out', 'exaggerated_falls', 'setting_names', 'mode_table', 'limp_timing',
         'blast_scales', 'loosens', 'kick_turns', 'faithful_falls', 'corrections_capped']
CONTROLS = [
    ('\tragdoll_solve(ragdoll);\n', '\n', 'falls_and_rests'),
    ('rotation_between(&before, &after, turn);', 'rotation_identity(turn);', 'nodes_follow_bones'),
    ('weights[index] = 1.f / (1.f + distance * distance);', 'weights[index] = 1.f;', 'push_turns'),
    ('ragdoll_clamp_speed(velocity);\n\t\tincoming', 'incoming', 'thrown'),
    ('if (across < 0.f)\n\t\t\tacross = 0.f;', '', 'knees_bend_one_way'),
    ('twist = ragdoll_clamp(twist, -limit, limit);', '', 'knees_bend_one_way'),
    ('if (cosine >= joint->limit)\n\t\t\treturn;', 'return;', 'hips_swing_so_far'),
    ('weight_a = ragdoll->inverse_masses[constraint->a];', 'weight_a = 1.f;', 'torso_heavier'),
    ('if (constraint->type == _ragdoll_constraint_minimum && distance >= constraint->length)\n\t\t\t\tcontinue;',
     'if (constraint->type == _ragdoll_constraint_minimum)\n\t\t\t\tcontinue;', 'limbs_keep_out'),
    ('slowing = speed * mode->joint_drag;', 'slowing = 0.f;', 'limbs_keep_out'),
    ('!strncmp(name, "halo", 4)', '0', 'setting_names'),
    ('return ticks >= mode->wait_ticks || landed || played >= 1.f;', 'return TRUE;', 'limp_timing'),
    ('(ticks >= mode->wait_ticks && (landed || lying))', '(landed || lying)', 'limp_timing'),
    ('scale * mode->big_blast : scale', 'scale : scale', 'blast_scales'),
    ('joint->limit = joint->tight_limit * looseness;', 'joint->limit = joint->tight_limit;', 'loosens'),
    ('cosine(MIN(joint->bend_angle * looseness, _pi))', 'cosine(joint->bend_angle)', 'loosens'),
    ('float share = (positions[index][2] - middle[2]) * up / farthest;', 'float share = 1.f;', 'kick_turns'),
    ('scale_vector3d(&along, speed > slowing ? (speed - slowing) / speed : 0.f, &along);',
     'scale_vector3d(&along, speed > slowing ? 1.f : 1.f, &along);', 'faithful_falls'),
    ('if (size > limit)', 'if (0)', 'corrections_capped'),
]
FUNCTIONS = ['ragdoll_named', 'ragdoll_mode_get', 'ragdoll_clamp', 'ragdoll_clamp_speed', 'ragdoll_move',
             'rotation_identity', 'rotation_transform', 'rotation_between', 'basis_from_directions',
             'rotation_between_bases', 'ragdoll_add_constraint', 'ragdoll_bent_distance', 'ragdoll_add_bend',
             'ragdoll_push', 'ragdoll_frame', 'rotation_untransform', 'ragdoll_frame_node', 'ragdoll_add_hinge',
             'ragdoll_add_cone', 'ragdoll_hold_joint', 'ragdoll_looseness', 'ragdoll_loosen', 'ragdoll_link_bend',
             'ragdoll_random', 'ragdoll_kick', 'ragdoll_blast_scale', 'ragdoll_limp_now', 'ragdoll_solve',
             'ragdoll_limit_correction', 'ragdoll_substep', 'ragdoll_pose']


MODES = re.compile(r'^static struct ragdoll_mode const ragdoll_modes\[.*?^\};', re.M | re.S)


def generated(fault=None):
    source = read('port/linux/game/biped_ragdoll.c')
    code = '\n'.join(
        [enum_with(source, member) for member in ('MAXIMUM_RAGDOLLS', '_ragdoll_style_mixed', '_ragdoll_start_near_end',
                                                  '_ragdoll_part_hand', '_ragdoll_constraint_minimum',
                                                  '_ragdoll_joint_cone', 'RAGDOLL_LONGEST_WAIT_TICKS')] +
        re.findall(r'^#define RAGDOLL_\w+ [^\n]*', source, re.M) +
        [structure(source, name) for name in ('ragdoll_mode', 'ragdoll_constraint', 'ragdoll_rotation',
                                              'ragdoll_joint', 'ragdoll_blow', 'ragdoll')] +
        [MODES.search(source).group(0)] +
        # (the MODERN kick, push and tumble rules the functions call: the header's text, so a control reaches it)
        [read('port/linux/game/ae_ragdoll_rules.h')] +
        [function(source, name) for name in FUNCTIONS])
    if fault:
        code = mutated(code, *fault)
    return (('under_test.inc', code),)


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('biped_ragdoll', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('before,after,case', CONTROLS)
def test_negative_control(before, after, case):
    status, output = run(build('biped_ragdoll', generated((before, after))), case)
    assert status == CHECK_FAILED, output


def test_ragdoll_where():
    # The setting chooses the ragdoll in a campaign and in multiplayer. A cinematic, and a body marked as an
    # actor in one, keep the scripted death.
    source = read('port/linux/game/biped_ragdoll.c')
    # A player's body takes the players' setting, an AI's the AI's: a player's while its unit has a player, and once
    # dead (the game clears that index at the kill) while a player's unit or dead unit index names it.
    style = function(source, 'ragdoll_style')
    should = function(source, 'ragdoll_should_start')
    assert 'config_string("game.ragdoll_players")' in style and 'config_string("game.ragdoll_ai")' in style
    assert 'ragdoll_is_player_body(biped_index, biped) ? players : ai' in style
    assert 'game_engine_running()' not in style
    player = function(source, 'ragdoll_is_player_body')
    assert 'biped->unit.player_index != NONE' in player
    assert 'player->unit_index' in player and 'player->dead_unit_index' in player
    assert 'ae_ragdoll_is_player_body(' in player
    assert 'ragdoll_style(biped_index) != _ragdoll_style_none' in should
    assert 'cinematic_in_progress()' in should and '_object_movie_star_bit' in should


def test_host_word_skips_ragdolls():
    # A client takes no correction of a body its own ragdoll moves.
    states = function(read('port/linux/game/network_objects.c'), 'network_objects_handle_states')
    skip = states.index('biped_ragdoll_active(state->object_index)')
    assert skip < states.index('distributed_body_set_at_rest(state->object_index, at_rest);')


def test_host_only_draws():
    # Hosting a network game, a ragdoll is only drawn: the body moves and poses as the stock game has it (what the
    # host sends its clients and what its shots hit), and the renderer draws the ragdoll's nodes.
    source = read('port/linux/game/biped_ragdoll.c')
    assert 'ragdoll->drawn_only = game_connection() == _game_connection_network_server;' in function(source, 'ragdoll_start')
    update = function(source, 'biped_ragdoll_update')
    assert update.index('if (ragdoll->drawn_only)\n\t\treturn FALSE;') < update.index('ragdoll_move_body(')
    assert 'return !ragdoll->drawn_only;' in function(source, 'biped_ragdoll_accelerate')
    for name in ('ragdoll_sleep', 'ragdoll_wake'):
        body = function(source, name)
        assert body.index('drawn_only') < body.index('SET_FLAG(biped->')
    drawn = function(read('port/linux/game/render_interpolation.c'), 'render_interpolation_object_node_matrices')
    assert 'biped_ragdoll_drawn_nodes(object_index)' in drawn


def test_waiting_body_is_the_stock_games():
    # A body playing its death animation (modern, mixed) is the stock game's until it goes limp: the host's word moves
    # it, blasts push it as they push any body, and it is drawn as it animates.
    source = read('port/linux/game/biped_ragdoll.c')
    assert 'ragdoll && !ragdoll->waiting && ragdoll_body(ragdoll)' in function(source, 'biped_ragdoll_active')
    assert '!ragdoll || ragdoll->waiting ||' in function(source, 'biped_ragdoll_accelerate')
    update = function(source, 'biped_ragdoll_update')
    assert update.index('if (!ragdoll_wait(ragdoll, biped_index))\n\t\t\treturn FALSE;') < update.index('ragdoll_build(')
    assert '!ragdoll->waiting && !ragdoll->drawn_only' in function(source, 'ragdoll_release')
    # (its own random numbers: the game's are what every machine of a network game must agree on)
    assert 'real_random' not in source and 'global_random_seed' not in source


def test_setting_help_names_the_styles():
    config = read('port/linux/src/port_config.c')
    row = config[config.index('{ "game.ragdoll_players"'):config.index('{ "game.ragdoll_ai"')]
    for word in ('none', 'spaghetti_western', 'exaggerated', 'modern', 'mixed'):
        assert '\\"%s\\"' % word in row, word
    readme = read('port/linux/README.md')
    line = next(line for line in readme.splitlines() if line.startswith('| `game.ragdoll_players`'))
    for word in ('none', 'spaghetti_western', 'exaggerated', 'modern', 'mixed'):
        assert '`"%s"`' % word in line, word
