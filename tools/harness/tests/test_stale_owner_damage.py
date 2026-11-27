"""Damage whose owner object is deleted, or deleted and made again in its slot: no player read from it, no crash."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, enum_with, function, mutated, read, run, structure

CASES = ['live-vehicle', 'live-parent', 'deleted', 'recreated', 'gone-player', 'unowned']
OWNER_CHECK = ('if (damage->owner_object_index != NONE && !object_try_and_get(damage->owner_object_index)) '
               'damage->owner_object_index = NONE; /* AE hook */')


def define(source, name):
    for line in source.splitlines():
        if line.startswith('#define ' + name + '(') or line.startswith('#define ' + name + '\t') or \
                line.startswith('#define ' + name + ' '):
            return line + '\n'
    raise LookupError(f'define not found in the sources: {name}')


def generated(fault=None):
    data_h = read('source/memory/data.h')
    data = read('source/memory/data.c')
    objects_h = read('source/objects/objects.h')
    objects = read('source/objects/objects.c')
    damage = read('source/objects/damage.c')
    types = f"#define TAG_STRING_LENGTH {constant(read('source/tag_files/tag_files.h'), 'TAG_STRING_LENGTH')}\n"
    types += structure(data_h, 'data_array') + '\n'
    types += structure(data_h, 'datum_header') + '\n'
    types += enum_with(read('source/objects/object_types.h'), '_object_mask_all') + '\n'
    types += 'struct object_datum;\n'
    types += structure(objects_h, 'object_header_datum') + '\n'
    for name in ('object_header_get', 'object_header_try_and_get', 'object_get', 'object_try_and_get'):
        types += define(objects_h, name)
    types += define(read('source/units/units.h'), 'unit_try_and_get')
    types += structure(objects_h, 'location') + '\n'
    types += structure(read('source/objects/damage.h'), 'damage_data') + '\n'
    validate = function(damage, 'damage_data_validate_owner')
    if fault:
        validate = mutated(validate, *fault)
    code = function(data, 'datum_try_and_get') + '\n' + function(data, 'datum_get') + '\n'
    code += function(data, 'datum_delete') + '\n'
    code += function(objects, 'object_try_and_get_and_verify_type') + '\n'
    code += function(objects, 'object_get_and_verify_type') + '\n'
    code += validate + '\n'
    code += function(damage, 'get_player_index_from_object_or_parents') + '\n'
    return (('types.inc', types), ('under_test.inc', code))


@pytest.mark.parametrize('case', CASES)
def test_case(case):
    status, output = run(build('stale_owner_damage', generated()), case)
    assert status == 0, output


@pytest.mark.parametrize('case', ['deleted', 'recreated'])
def test_negative_control(case):
    # without the owner check, the deleted owner reaches object_get: the report a release build crashes after
    status, output = run(build('stale_owner_damage', generated((OWNER_CHECK, ''))), case)
    assert status == CHECK_FAILED, output


def test_owner_checked_before_it_is_read():
    # object_cause_damage checks the owner before its first read of the owner object
    cause = function(read('source/objects/damage.c'), 'object_cause_damage')
    assert cause.index('damage_data_validate_owner(damage);') < \
        cause.index('get_player_index_from_object_or_parents(damage->owner_object_index)')
