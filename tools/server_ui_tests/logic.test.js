// The admin page's logic (port/assets/server_ui/app.js) on its own: the
// formatting, the log's cursor, the rotation editor's operations, the forms
// made from the settings' table and the gametype schema, the TOML they edit,
// the console's completion, and the element builder's refusals.
// node --test tools/server_ui_tests/

'use strict';

const test = require('node:test');
const assert = require('node:assert');
const { app, fakeDocument } = require('./helpers.js');

test('roles, and the ban presets a moderator may use', () => {
  assert.ok(app.roleAtLeast('admin', 'moderator'));
  assert.ok(!app.roleAtLeast('moderator', 'admin'));
  assert.ok(!app.roleAtLeast(null, 'viewer'));
  assert.ok(!app.roleAtLeast('nobody', 'viewer'));
  const moderator = app.banPresets('moderator').filter((preset) => preset.allowed).map((preset) => preset.value);
  assert.deepStrictEqual(moderator, ['1h', '1d']);
  assert.ok(app.banPresets('admin').every((preset) => preset.allowed));
});

test('durations, clocks and times left', () => {
  assert.strictEqual(app.formatDuration(42), '42 s');
  assert.strictEqual(app.formatDuration(600), '10 min');
  assert.strictEqual(app.formatDuration(3720), '1 h 02 min');
  assert.strictEqual(app.formatDuration(3 * 86400 + 7200), '3 d 2 h');
  assert.strictEqual(app.formatDuration(-5), '0 s');
  assert.strictEqual(app.formatClock(312), '5:12');
  assert.strictEqual(app.formatClock(3725), '1:02:05');
  assert.strictEqual(app.formatUntil(null), 'For ever');
  assert.strictEqual(app.formatUntil(1000, 2000 * 1000), 'Ended');
  assert.strictEqual(app.formatUntil(1000 + 7200, 1000 * 1000), 'For 2 h 00 min');
  assert.strictEqual(app.formatAgo(100, 160 * 1000), '1 min ago');
  assert.strictEqual(app.formatAgo(0), '–');
  assert.strictEqual(app.teamName(1), 'Blue');
  assert.strictEqual(app.stateLabel('postgame'), 'Postgame');
});

test('refusals in words', () => {
  assert.match(app.errorText({ error: { code: 'locked', retry_after: 600 } }), /10 min/);
  assert.strictEqual(app.errorText({ error: { code: 'invalid', line: 4, message: 'lives is 0 to 99' } }), 'Line 4: lives is 0 to 99');
  assert.strictEqual(app.errorText({ error: { code: 'schema', field: 'name', message: 'is needed' } }), 'name: is needed');
  assert.strictEqual(app.errorText({ error: { code: 'rejected', message: '1 entry cannot be played',
    rejected: [{ index: 1, entry: 'nowhere:ctf', reason: 'no map nowhere' }] } }), '1 entry cannot be played; nowhere:ctf: no map nowhere');
  assert.match(app.errorText({ error: { code: 'loading' } }), /map is loading/);
  assert.strictEqual(app.errorText({ status: 500, error: { code: 'odd' } }), 'Refused (odd).');
});

test("the log's cursor: new lines only, and a restart seen", () => {
  let state = { cursor: 0 };
  let taken = app.takeLog(state, { lines: [{ n: 1 }, { n: 2 }, { n: 3 }], next: 3, missed: 0 });
  assert.deepStrictEqual(taken.lines.map((line) => line.n), [1, 2, 3]);
  state = { cursor: taken.cursor };
  assert.strictEqual(state.cursor, 3);
  // a line seen already is not taken twice
  taken = app.takeLog(state, { lines: [{ n: 3 }, { n: 4 }], next: 4 });
  assert.deepStrictEqual(taken.lines.map((line) => line.n), [4]);
  assert.strictEqual(taken.cursor, 4);
  // the server's next is the last line it read, those it kept from this
  // role (chat's, from a moderator) too: the cursor goes past them
  taken = app.takeLog({ cursor: 4 }, { lines: [{ n: 6 }], next: 9 });
  assert.deepStrictEqual([taken.lines.map((line) => line.n), taken.cursor, taken.reset], [[6], 9, false]);
  taken = app.takeLog({ cursor: 4 }, { lines: [], next: 4 });
  assert.strictEqual(taken.cursor, 4);
  assert.ok(!taken.reset);
  // the numbering went back: the server started again
  taken = app.takeLog({ cursor: 900 }, { lines: [{ n: 1 }, { n: 2 }], next: 2 });
  assert.ok(taken.reset);
  assert.deepStrictEqual(taken.lines.map((line) => line.n), [1, 2]);
  assert.strictEqual(taken.cursor, 2);
  assert.ok(app.takeLog({ cursor: 900 }, { lines: [], next: 0 }).reset);
  assert.strictEqual(app.takeLog({ cursor: 5 }, { lines: [{ n: 6 }], missed: 12 }).missed, 12);
  assert.deepStrictEqual(app.takeLog({ cursor: 0 }, { lines: [{ n: 'x' }, null, { n: 1 }] }).lines, [{ n: 1 }]);
});

test("the log's lines in order: duplicates out, late ones in place, the oldest dropped", () => {
  let merged = app.mergeLog([{ n: 1 }, { n: 2 }], [{ n: 2 }, { n: 3 }]);
  assert.deepStrictEqual(merged.lines.map((line) => line.n), [1, 2, 3]);
  assert.ok(merged.appended);
  assert.deepStrictEqual(merged.fresh.map((line) => line.n), [3]);
  merged = app.mergeLog(merged.lines.concat([{ n: 7 }]), [{ n: 5 }]);
  assert.ok(!merged.appended);
  assert.deepStrictEqual(merged.lines.map((line) => line.n), [1, 2, 3, 5, 7]);
  merged = app.mergeLog([{ n: 1 }, { n: 2 }, { n: 3 }], [{ n: 4 }, { n: 5 }], 3);
  assert.deepStrictEqual(merged.lines.map((line) => line.n), [3, 4, 5]);
  assert.strictEqual(merged.dropped, 2);
});

test('the sparkline: ten minutes of samples, as steps', () => {
  const history = [];
  app.pushSample(history, 0, 2);
  app.pushSample(history, 300000, 4);
  app.pushSample(history, 700000, 3);
  // (the last sample before the window stays: it is the line's start)
  assert.deepStrictEqual(history.map((item) => item.time), [0, 300000, 700000]);
  app.pushSample(history, 950000, 1);
  assert.deepStrictEqual(history.map((item) => item.time), [300000, 700000, 950000]);
  const steps = app.sparkPoints([{ time: 100000, value: 2 }, { time: 400000, value: 3 }], 700000, 300, 64, 4).split(' ');
  assert.deepStrictEqual(steps, ['0.0,32.0', '150.0,32.0', '150.0,17.0', '300.0,17.0']);
  assert.strictEqual(app.sparkPoints([], 0, 300, 64, 4), '');
});

test('events in words, kills kept out of the feed', () => {
  const players = [{ slot: 0, name: 'Milenko' }, { slot: 3, name: 'Wren' }];
  assert.strictEqual(app.describeEvent({ event: 'kick', name: 'Wren', by: 'paul', reason: 'spawn killing' }).text,
    'Wren kicked by paul: spawn killing');
  const kill = app.describeEvent({ event: 'kill', killer: 0, victim: -1 }, players);
  assert.strictEqual(kill.text, 'Milenko killed the world');
  assert.ok(kill.quiet);
  assert.strictEqual(app.describeEvent({ event: 'speed_hack', machine: 4, names: ['Odin'], rate: 1.4 }).kind, 'bad');
  assert.strictEqual(app.describeEvent({ event: 'status' }), null);
});

test("the rotation editor's operations leave the list given alone", () => {
  const entries = ['a', 'b', 'c'].map((map) => app.makeEntry(map, 'slayer', ''));
  const moved = app.rotationMove(entries, 2, -1);
  assert.deepStrictEqual(moved.map((entry) => entry.map), ['a', 'c', 'b']);
  assert.deepStrictEqual(entries.map((entry) => entry.map), ['a', 'b', 'c']);
  assert.deepStrictEqual(app.rotationMove(entries, 0, -1).map((entry) => entry.map), ['a', 'b', 'c']);
  assert.deepStrictEqual(app.rotationRemove(entries, 1).map((entry) => entry.map), ['a', 'c']);
  assert.deepStrictEqual(app.rotationInsert(entries, app.makeEntry('d', '', ''), 0).map((entry) => entry.map), ['d', 'a', 'b', 'c']);
  assert.deepStrictEqual(app.rotationInsert(entries, app.makeEntry('d', '', '')).map((entry) => entry.map), ['a', 'b', 'c', 'd']);
  const marked = app.markRejected(entries, [{ index: 1, entry: 'b:slayer', reason: 'no map b' }]);
  assert.deepStrictEqual(marked.map((entry) => entry.rejected), [null, 'no map b', null]);
  assert.strictEqual(entries[1].rejected, null);
});

test("an entry's text, its rules, and custom maps' names", () => {
  const entry = app.makeEntry('sidewinder', 'team_slayer', 'score=50 time=15');
  assert.strictEqual(entry.text, 'sidewinder:team_slayer+score=50+time=15');
  assert.deepStrictEqual(entry.rules, { score: 50, time: 15 });
  assert.strictEqual(app.makeEntry('prisoner', 'slayer', '+ff=off').text, 'prisoner:slayer+ff=off');
  assert.throws(() => app.rulesText('score'), /not a rule/);
  assert.deepStrictEqual(app.parseEntryText('bloodgulch'), { map: 'bloodgulch', gametype: '', rules: {} });
  assert.strictEqual(app.parseEntryText('bad entry'), null);
  assert.strictEqual(app.mapToken({ name: 'hugeass', kind: 'custom' }), 'custom_maps\\hugeass');
  assert.strictEqual(app.mapToken({ name: 'custom_maps\\hugeass', kind: 'custom' }), 'custom_maps\\hugeass');
  assert.strictEqual(app.mapToken({ name: 'bloodgulch', kind: 'xbox' }), 'bloodgulch');
});

const SETTINGS = [
  { name: 'server.name', type: 'string', value: 'OpenCE', default: 'OpenCE Dedicated', comment: 'The name', env: 'HALO_SERVER_NAME',
    pinned: false, secret: false, choices: null, min: null, max: null },
  { name: 'server.max_players', type: 'integer', value: 16, default: 16, comment: '', env: 'HALO_SERVER_MAX_PLAYERS',
    pinned: false, secret: false, choices: null, min: 2, max: 127 },
  { name: 'server.public', type: 'boolean', value: true, default: true, comment: '', env: 'HALO_SERVER_PUBLIC',
    pinned: false, secret: false, choices: null, min: null, max: null },
  { name: 'server.vote_ratio', type: 'real', value: 0.5, default: 0.5, comment: '', env: 'HALO_SERVER_VOTE_RATIO',
    pinned: false, secret: false, choices: null, min: 0, max: 1 },
  { name: 'server.log_format', type: 'string', value: 'text', default: 'text', comment: '', env: 'HALO_SERVER_LOG_FORMAT',
    pinned: false, secret: false, choices: ['text', 'json'], min: null, max: null },
  { name: 'server.web_port', type: 'integer', value: 8080, default: 8080, comment: '', env: 'HALO_SERVER_WEB_PORT',
    pinned: true, secret: false, choices: null, min: 0, max: 65535 },
  { name: 'server.password', type: 'string', value: '', default: '', comment: '', env: 'HALO_SERVER_PASSWORD',
    pinned: false, secret: true, set: true, choices: null, min: null, max: null },
];

test("the settings' form from the server's table", () => {
  const controls = Object.fromEntries(SETTINGS.map((setting) => [setting.name, app.settingControl(setting, 'admin')]));
  assert.strictEqual(controls['server.name'].kind, 'text');
  assert.strictEqual(controls['server.name'].defaultText, 'OpenCE Dedicated');
  assert.strictEqual(controls['server.max_players'].kind, 'number');
  assert.strictEqual(controls['server.max_players'].step, '1');
  assert.strictEqual(controls['server.vote_ratio'].step, 'any');
  assert.strictEqual(controls['server.public'].kind, 'checkbox');
  assert.strictEqual(controls['server.public'].defaultText, 'on');
  assert.strictEqual(controls['server.log_format'].kind, 'select');
  assert.ok(controls['server.web_port'].disabled);
  assert.strictEqual(controls['server.web_port'].reason, 'Set by HALO_SERVER_WEB_PORT');
  assert.strictEqual(controls['server.password'].kind, 'secret');
  assert.strictEqual(controls['server.password'].value, '');
  assert.match(controls['server.password'].placeholder, /^Set/);
  const moderator = app.settingControl(SETTINGS[0], 'moderator');
  assert.ok(moderator.disabled);
  assert.strictEqual(moderator.reason, 'Needs the admin role');
});

test("settings' values as strings (an older server's) or typed: the same form, the same body", () => {
  const strings = SETTINGS.map((setting) => ({ ...setting, value: setting.secret ? '' : String(setting.value),
    default: String(setting.default), min: setting.min === null ? null : String(setting.min),
    max: setting.max === null ? null : String(setting.max) }));
  strings.push({ name: 'server.coop', type: 'boolean', value: 'false', default: 'false', pinned: false, secret: false });
  const controls = Object.fromEntries(strings.map((setting) => [setting.name, app.settingControl(setting, 'admin')]));
  assert.strictEqual(controls['server.public'].value, true);
  assert.strictEqual(controls['server.coop'].value, false);
  assert.strictEqual(controls['server.coop'].defaultText, 'off');
  assert.strictEqual(controls['server.max_players'].value, 16);
  assert.strictEqual(controls['server.max_players'].max, 127);
  // nothing changed: nothing sent, a boolean "false" not turned on
  const same = { 'server.name': 'OpenCE', 'server.max_players': '16', 'server.public': true, 'server.vote_ratio': '0.5',
    'server.log_format': 'text', 'server.coop': false };
  assert.deepStrictEqual(app.settingsChanges(strings, same).body, {});
  assert.deepStrictEqual(app.settingsChanges(SETTINGS, same).body, {});
  assert.deepStrictEqual(app.settingsChanges(strings, { ...same, 'server.coop': true, 'server.max_players': '20' }).body,
    { 'server.coop': true, 'server.max_players': 20 });
  assert.strictEqual(app.settingValue('boolean', 'true'), true);
  assert.strictEqual(app.settingValue('boolean', 0), false);
  assert.strictEqual(app.settingValue('real', '0.25'), 0.25);
  assert.strictEqual(app.settingValue('string', null), '');
});

test("the owner's settings are never sent from the page", () => {
  const owner = [
    { name: 'server.log_addresses', type: 'boolean', value: 'false', default: 'false', pinned: false, secret: false },
    { name: 'server.status_file', type: 'string', value: '', default: '', pinned: false, secret: false },
    { name: 'server.auto_update', type: 'boolean', value: false, default: false, pinned: false, secret: false },
    { name: 'server.new_owner_one', type: 'string', value: '', role: 'owner', pinned: false, secret: false },
  ];
  for (const setting of owner) {
    const control = app.settingControl(setting, 'admin');
    assert.ok(control.disabled, setting.name);
    assert.match(control.reason, /console/);
  }
  const changes = app.settingsChanges(owner, { 'server.log_addresses': true, 'server.status_file': '/tmp/x',
    'server.auto_update': true, 'server.new_owner_one': 'x' }, 'admin');
  assert.deepStrictEqual(changes, { body: {}, errors: [] });
  assert.deepStrictEqual(app.settingsChanges(SETTINGS, { 'server.max_players': '20' }, 'moderator').body, {});
});

test("the settings' form back as PUT /api/settings's body: what changed, typed", () => {
  const { body, errors } = app.settingsChanges(SETTINGS, {
    'server.name': 'OpenCE', 'server.max_players': '24', 'server.public': false, 'server.vote_ratio': '0.75',
    'server.log_format': 'json', 'server.web_port': '9000', 'server.password': { text: '', clear: false },
  });
  assert.deepStrictEqual(errors, []);
  assert.deepStrictEqual(body, { 'server.max_players': 24, 'server.public': false, 'server.vote_ratio': 0.75, 'server.log_format': 'json' });
  const wrong = app.settingsChanges(SETTINGS, { 'server.max_players': '200', 'server.vote_ratio': 'half', 'server.log_format': 'xml' });
  assert.deepStrictEqual(wrong.errors.map((error) => error.name), ['server.max_players', 'server.vote_ratio', 'server.log_format']);
  assert.deepStrictEqual(wrong.body, {});
  assert.deepStrictEqual(app.settingsChanges(SETTINGS, { 'server.password': { text: 'hunter22' } }).body, { 'server.password': 'hunter22' });
  assert.deepStrictEqual(app.settingsChanges(SETTINGS, { 'server.password': { text: 'x', clear: true } }).body, { 'server.password': '' });
});

const GAMETYPE = `base = "ctf"   # the engine
name = "Fast CTF"

[universal]
score_to_win = 3 # three caps
radar = false

[ctf]
assault = true
`;

test('a gametype file: read, and one key changed with the rest as it was', () => {
  const parsed = app.parseToml(GAMETYPE);
  assert.deepStrictEqual(parsed.errors, []);
  assert.deepStrictEqual(parsed.values, { '': { base: 'ctf', name: 'Fast CTF' }, universal: { score_to_win: 3, radar: false },
    ctf: { assault: true } });
  let text = app.setTomlValue(GAMETYPE, 'universal', 'score_to_win', 5);
  assert.match(text, /^score_to_win = 5 # three caps$/m);
  text = app.setTomlValue(text, 'universal', 'lives', 3);
  assert.match(text, /radar = false\nlives = 3\n\n\[ctf\]/);
  text = app.setTomlValue(text, 'options', 'friendly_fire', 'off');
  assert.match(text, /\[options\]\nfriendly_fire = "off"\n$/);
  text = app.setTomlValue(text, 'universal', 'radar', undefined);
  assert.doesNotMatch(text, /radar/);
  text = app.setTomlValue(text, '', 'description', 'Caps "fast"');
  assert.match(text, /^name = "Fast CTF"\ndescription = "Caps \\"fast\\""\n\n\[universal\]/m);
  assert.deepStrictEqual(app.parseToml(text).values.universal, { score_to_win: 5, lives: 3 });
  assert.strictEqual(app.parseToml(text).values[''].description, 'Caps "fast"');
  assert.strictEqual(app.setTomlValue(GAMETYPE, 'king', 'moving_hill', undefined), GAMETYPE);
  const bad = app.parseToml('base = "ctf"\nlives 3\n[ctf]\n[ctf]\nscore = maybe');
  assert.deepStrictEqual(bad.errors.map((error) => error.line), [2, 4, 5]);
  assert.match(app.newGametypeText('team_slayer', 'a_long_gametype_name'), /^base = "team_slayer"\nname = "a_long_game"\n/);
});

test("the gametype form's fields: every engine's and the base's own", () => {
  const schema = { fields: [
    { key: 'score_to_win', alias: 'score', engine: 'all', type: 'integer', min: 0, max: 999, choices: null, default: null },
    { key: 'time_limit', alias: 'time', engine: 'all', table: 'options', type: 'integer', min: 0, max: 1440 },
    { key: 'assault', alias: null, engine: 'ctf', type: 'boolean' },
    { key: 'moving_hill', alias: null, engine: 'king', type: 'boolean' },
  ] };
  const fields = app.gametypeFields(schema, 'ironctf');
  assert.deepStrictEqual(fields.map((field) => [field.key, field.table]),
    [['score_to_win', 'universal'], ['time_limit', 'options'], ['assault', 'ctf']]);
  assert.deepStrictEqual(app.gametypeFields(schema, 'team_king').map((field) => field.key), ['score_to_win', 'time_limit', 'moving_hill']);
  assert.strictEqual(app.fieldValue(fields[0], ''), undefined);
  assert.strictEqual(app.fieldValue(fields[0], '12'), 12);
  assert.strictEqual(app.fieldValue(fields[2], 'false'), false);
  assert.strictEqual(app.engineOf('accumulation'), 'oddball');
});

test("each gametype's engine: the server's word, else the builders' of game_engine.c", () => {
  for (const [name, engine] of [['juggernaut', 'oddball'], ['stalker', 'oddball'], ['slayer_pro', 'slayer'], ['phantoms', 'slayer'],
    ['ctf_pro', 'ctf'], ['invasion', 'ctf'], ['king_pro', 'king'], ['crazy_king', 'king'], ['team_rally', 'race'],
    ['swat', 'slayer'], ['gun_game', 'slayer'], ['hardcore_ctf', 'ctf'], ['hardcore_oddball', 'oddball'], ['koth', 'king']]) {
    assert.strictEqual(app.engineOf(name), engine, name);
  }
  assert.strictEqual(app.engineOf('nothing'), null);
  const builtin = [{ name: 'newtype', engine: 'race' }, 'slayer'];
  assert.strictEqual(app.engineOf('newtype', builtin), 'race');
  assert.deepStrictEqual(app.builtinNames(builtin), ['newtype', 'slayer']);
  const schema = { fields: [{ key: 'race_type', engine: 'race', type: 'integer', min: 0, max: 2 }] };
  assert.deepStrictEqual(app.gametypeFields(schema, 'newtype', builtin).map((field) => field.key), ['race_type']);
});

test("maps without a title, and Play now's body keeping an entry's rules", () => {
  assert.strictEqual(app.titleOf({ name: 'bloodgulch', title: 'Blood Gulch' }), 'Blood Gulch');
  assert.strictEqual(app.titleOf({ name: 'bloodgulch', level: 'levels\\test\\bloodgulch\\bloodgulch', kind: 'multiplayer' }), 'bloodgulch');
  assert.deepStrictEqual(app.playBody(app.makeEntry('bloodgulch', 'ctf', 'score=5')), { map: 'bloodgulch:ctf+score=5' });
  assert.deepStrictEqual(app.playBody(app.makeEntry('bloodgulch', 'ctf', '')), { map: 'bloodgulch', gametype: 'ctf' });
  assert.deepStrictEqual(app.playBody({ text: 'prisoner : slayer', map: 'prisoner', gametype: 'slayer', rules: { time_limit: 10 } }),
    { map: 'prisoner:slayer' });
  assert.deepStrictEqual(app.playBody({ text: 'a10', map: 'a10', gametype: '', rules: {} }), { map: 'a10' });
  const share = app.describeEvent({ event: 'map_share', machine: 2, map: 'custom_maps\\x', progress: 42 });
  assert.match(share.text, /: 42%$/);
  assert.match(app.describeEvent({ event: 'player_left', name: 'Wren', reason: 'kicked' }).text, /Wren was kicked/);
});

test("the console's completion and history", () => {
  // (help's lines as server_commands.c says them: a heading, then each
  // command indented by two spaces)
  const commands = app.commandsFromHelp(["the server's commands (help <command> says more; the game's console commands run too):",
    '  help               lists the commands, or says more of one', '  sv_status          the server, the game and its players',
    '  sv_set             changes a setting', '  kick               drops a player', '    a description going on']);
  assert.deepStrictEqual(commands, ['help', 'kick', 'sv_set', 'sv_status']);
  // (a server whose help does not indent them)
  assert.deepStrictEqual(app.commandsFromHelp(['sv_map <map> [gametype]', 'quit']), ['quit', 'sv_map']);
  assert.deepStrictEqual(app.complete('sv_st', commands, []), { line: 'sv_status ', options: ['sv_status'] });
  assert.strictEqual(app.complete('sv_', commands, []).line, 'sv_s');
  const players = [{ name: 'Milenko' }, { name: 'Mil Two' }, { name: 'Wren' }];
  assert.strictEqual(app.complete('kick w', commands, players).line, 'kick Wren ');
  assert.deepStrictEqual(app.complete('kick Mil', commands, players).options, ['Milenko', '"Mil Two"']);
  assert.strictEqual(app.complete('say hello', commands, players).line, 'say hello');
  const history = ['sv_status', 'kick 3'];
  let step = app.historyStep(history, 2, -1);
  assert.deepStrictEqual(step, { index: 1, text: 'kick 3' });
  step = app.historyStep(history, 0, -1);
  assert.strictEqual(step.text, 'sv_status');
  assert.deepStrictEqual(app.historyStep(history, 1, 1), { index: 2, text: '' });
});

test('elements are made without HTML, and no style or handler attribute gets in', () => {
  app.setDocument(fakeDocument);
  const clicks = [];
  const node = app.h('button', { class: 'small', text: '<b>Kick</b>', data: { slot: 3 }, attrs: { 'aria-label': 'Kick' },
    disabled: true, listen: { click: () => clicks.push(1) } }, ['ignored? no: ', 'children follow the text']);
  assert.strictEqual(node.className, 'small');
  assert.strictEqual(node.childNodes[0].textContent, '<b>Kick</b>');
  assert.strictEqual(node.textContent, '<b>Kick</b>ignored? no: children follow the text');
  assert.strictEqual(node.getAttribute('data-slot'), '3');
  assert.strictEqual(node.disabled, true);
  node.listeners.click[0]();
  assert.deepStrictEqual(clicks, [1]);
  for (const props of [{ onclick: 'x' }, { style: 'color:red' }, { innerHTML: '<i>' }, { outerHTML: '' },
    { attrs: { onload: 'x' } }, { attrs: { style: 'x' } }, { srcdoc: '' }, { nonsense: 1 }]) {
    assert.throws(() => app.h('div', props), /not allowed|unknown property/, JSON.stringify(props));
  }
  const list = app.h('ul', {}, [app.h('li', { text: 'a' }), null, false, 'b', 3]);
  assert.deepStrictEqual(list.childNodes.map((child) => child.textContent), ['a', 'b', '3']);
});

test("Arena Evolved's built-in gametypes in folders: the menus' style and group order, others after", () => {
  const folders = app.aeBuiltinFolders([
    { name: 'oct_br', display: 'OCT BR', style: 'TRAINING', group: '' },
    { name: 'ae_comp_ts', display: 'AE COMP TS', style: 'AE COMP', group: 'TEAM' },
    { name: 'ae_ffa', display: 'AE FFA', style: 'AE CASUAL', group: 'FREE FOR ALL' },
    { name: 'ae_team_sly', display: 'AE TEAM SLY', style: 'AE CASUAL', group: 'TEAM ARENA' },
    { name: 'odd_one', display: 'ODD ONE', style: 'NEW STYLE', group: 'NEW GROUP' },
    { name: 'ae_ctf', display: 'AE CTF', style: 'AE CASUAL', group: 'TEAM ARENA' },
    { name: 'no_style', style: '' },
    null, { display: 'nameless' },
  ]);
  assert.deepStrictEqual(folders.map((style) => style.style), ['AE CASUAL', 'AE COMP', 'TRAINING', 'NEW STYLE', 'OTHER']);
  assert.deepStrictEqual(folders[0].groups.map((group) => group.group), ['TEAM ARENA', 'FREE FOR ALL']);
  assert.deepStrictEqual(folders[0].groups[0].entries, [{ name: 'ae_team_sly', display: 'AE TEAM SLY' },
    { name: 'ae_ctf', display: 'AE CTF' }]);
  assert.strictEqual(folders[0].count, 3);
  assert.deepStrictEqual(folders[4].groups[0].entries, [{ name: 'no_style', display: 'no_style' }]);
  assert.deepStrictEqual(app.aeBuiltinFolders(undefined), []);
});

test("the status's values for reading: times rounded, no empty gametype, masters' states, settings in words", () => {
  assert.strictEqual(app.formatMs(6.76856), '6.8 ms');
  assert.strictEqual(app.formatMs(0.492143), '0.49 ms');
  assert.strictEqual(app.formatMs(4), '4 ms');
  assert.strictEqual(app.formatMs(250.4), '250 ms');
  assert.strictEqual(app.formatMs(undefined), '–');
  assert.strictEqual(app.gametypeText({ variant: '', gametype: '' }), '');
  // (the game's name alone: the score's row shows the teams)
  assert.strictEqual(app.gametypeText({ variant: 'CTF', gametype: 'ctf', teams: true }), 'CTF');
  assert.strictEqual(app.gametypeText({ variant: 'AE TEAM SLY', gametype: 'slayer', teams: true }), 'AE TEAM SLY');
  assert.strictEqual(app.gametypeText({ variant: '', gametype: 'team_slayer', teams: true }), 'Team Slayer');
  assert.deepStrictEqual(app.breakParts('master.example.org'), ['master.', 'example.', 'org']);
  assert.deepStrictEqual(app.breakParts('sidewinder:team_slayer+score=50'), ['sidewinder:', 'team_slayer+', 'score=50']);
  assert.deepStrictEqual(app.breakParts(''), []);
  // (red for a failure alone: a server that does not publish is private by design)
  const kind = (master) => app.masterView(master).kind;
  assert.strictEqual(kind({ url: 'https://m.example.org', listed: true, probe: 'ok', code: '' }), 'ok');
  assert.strictEqual(kind({ url: 'https://m.example.org', listed: false, probe: 'off', code: 'no_key' }), 'off');
  assert.strictEqual(kind({ url: 'https://m.example.org', listed: false, probe: 'off', code: 'off' }), 'off');
  assert.strictEqual(kind({ url: 'https://m.example.org', listed: false, probe: 'pending', code: '' }), 'wait');
  assert.strictEqual(kind({ url: 'https://m.example.org', listed: false, probe: 'failed', code: 'unreachable' }), 'bad');
  assert.strictEqual(app.masterView({ url: 'https://m.example.org/v1' }).host, 'm.example.org');
  assert.strictEqual(app.settingLabel('network.coop_ally_friendly_fire'), 'co-op ally friendly fire');
  assert.strictEqual(app.settingLabel('server.motd'), 'message of the day');
  assert.deepStrictEqual(app.settingTopic('server.chat_filter_slurs'), { table: 'server', topic: 'Chat and AFK' });
  assert.deepStrictEqual(app.settingTopic('network.shield_effects'), { table: 'network', topic: 'General' });
  assert.deepStrictEqual(app.settingTopic('game.something'), { table: 'game', topic: 'General' });
});

test('rotation entries, maps, gametypes and choices for people, never their machine words', () => {
  const gametypes = { ae_builtin: [{ name: 'ae_team_sly', display: 'AE TEAM SLY' }], files: [{ name: 'btb_slayer', description: 'BTB Slayer' }] };
  const next = app.entryView('sidewinder:team_slayer+score=50+time=15', [], gametypes);
  assert.deepStrictEqual([next.map.title, next.map.tag, next.gametype, next.rules], ['Sidewinder', '', 'Team Slayer', ['First to 50', '15 min']]);
  assert.strictEqual(app.entryLabel(next), 'Sidewinder, Team Slayer, first to 50, 15 min');
  assert.strictEqual(app.entryView('hektik@smp:ae_team_sly', [], gametypes).gametype, 'AE TEAM SLY');
  assert.deepStrictEqual(app.mapView('hektik@smp'), { title: 'Hektik', tag: 'SMP', word: 'hektik@smp' });
  assert.deepStrictEqual(app.mapView('pat_beavercreek@ws2899342285').tag, 'Workshop');
  assert.deepStrictEqual(app.mapView('bloodgulch@mcc'), { title: 'Blood Gulch', tag: 'MCC', word: 'bloodgulch@mcc' });
  assert.deepStrictEqual(app.mapView('custom_maps\\hugeass'), { title: 'Hugeass', tag: 'CE', word: 'custom_maps\\hugeass' });
  assert.strictEqual(app.mapView('levels\\test\\bloodgulch\\bloodgulch').title, 'Blood Gulch');
  assert.strictEqual(app.mapView('x', [{ name: 'x', title: 'The X' }]).title, 'The X');
  assert.strictEqual(app.gametypeTitle('btb_slayer', gametypes), 'BTB Slayer');
  assert.strictEqual(app.gametypeTitle('king'), 'King of the Hill');
  assert.strictEqual(app.ruleText('moving_hill', 1), 'Moving hill');
  assert.strictEqual(app.ruleText('teams', false), 'No teams');
  assert.strictEqual(app.ruleText('lives', 1), '1 life');
  assert.strictEqual(app.ruleText('score', 0), 'No score limit');
  assert.strictEqual(app.ruleText('weapons', 'pistols'), 'Weapon set: pistols');
  assert.strictEqual(app.humanize('team_global_enemy_proximity'), 'Team global enemy proximity');
  assert.strictEqual(app.describeEvent({ event: 'lobby_opened', map: 'bloodgulch', gametype: 'team_slayer', teams: true }).text,
    'Lobby: Team Slayer on Blood Gulch');
  // (a pack's map by the server's name for it, when the entry has it)
  const pack = app.entryView({ text: 'bloodgulch@cex:ctf', map_title: 'Abyss', pack: 'Halo CE+' }, [], {});
  assert.deepStrictEqual([pack.map.title, pack.map.tag, pack.map.pack], ['Abyss', 'CE+', 'Halo CE+']);
});

test('stop or restart: the countdown and the drain in words, the request, the question', () => {
  const now = 1_000_000;
  const view = app.stopView({ restart_warning: { seconds: 42, total: 60, restart: true, reason: 'updating' } }, now + 2000, now);
  assert.deepStrictEqual([view.kind, view.left, view.clock, view.text, view.reason], ['countdown', 40, '0:40', 'Restarting in 0:40', 'updating']);
  assert.strictEqual(app.stopView({ restart_warning: { seconds: 5, restart: false } }, now + 9000, now).left, 0);
  assert.strictEqual(app.stopView({ drain: { after: 'game', restart: false, since: 1 } }).text, 'Stops when this game ends');
  assert.strictEqual(app.stopView({ drain: { after: 'empty', restart: true } }).text, 'Restarts once nobody plays');
  assert.strictEqual(app.stopView({ drain: null, restart_warning: null }), null);
  assert.strictEqual(app.stopView({}), null);
  assert.deepStrictEqual(app.stopBody('seconds', '90', true, ' updating '), { restart: true, notice: 'updating', seconds: 90 });
  assert.deepStrictEqual(app.stopBody('now', '', false, ''), { restart: false, after: 'now' });
  assert.deepStrictEqual(app.stopBody('empty', '', true), { restart: true, after: 'empty' });
  for (const bad of ['0', '3601', '1.5', 'x', '']) assert.throws(() => app.stopBody('seconds', bad, true), /1 to 3600/);
  assert.strictEqual(app.stopQuestion({ restart: true, seconds: 60 }).title, 'Restart the server in 1 min?');
  assert.strictEqual(app.stopQuestion({ restart: false, after: 'game' }).title, 'Stop the server when this game ends?');
});

test('keys, admins and the whitelist: roles, ends, allow-lists, the lines sent, the secret read back', () => {
  assert.deepStrictEqual(app.keyRoles('owner'), ['viewer', 'moderator', 'admin']);
  assert.deepStrictEqual(app.keyRoles('admin'), ['viewer', 'moderator', 'admin']);
  assert.deepStrictEqual(app.keyRoles('moderator'), ['viewer', 'moderator']);
  const now = 1_700_000_000_000;
  assert.strictEqual(app.formatExpiry(0, false, now), 'Never');
  assert.strictEqual(app.formatExpiry(now / 1000 + 12 * 86400 + 60, false, now), 'In 12 days');
  assert.strictEqual(app.formatExpiry(now / 1000 + 3600, false, now), 'In 1 h 00 min');
  assert.strictEqual(app.formatExpiry(now / 1000 - 1, false, now), 'Ended');
  assert.strictEqual(app.allowText(' 192.168.1.0/24, 10.0.0.5 '), '192.168.1.0/24,10.0.0.5');
  assert.strictEqual(app.allowText(''), '');
  for (const bad of ['300.1.1.1', '10.0.0.0/33', 'example.org', '10.0.0.0/24,,']) assert.throws(() => app.allowText(bad));
  assert.strictEqual(app.adminAddLine({ kind: 'login', name: 'ops', role: 'moderator', immunity: '5', days: '30', allow: '10.0.0.0/8' }),
    'sv_admin_add name:ops moderator 5 30 10.0.0.0/8');
  assert.strictEqual(app.adminAddLine({ kind: 'network', name: '192.168.1.0/24, 10.1.2.3', role: 'admin', immunity: '', days: '', allow: '1.2.3.4' }),
    'sv_admin_add cidr:192.168.1.0/24,10.1.2.3 admin 0 0');
  assert.throws(() => app.adminAddLine({ kind: 'login', name: 'Ops Team', role: 'moderator' }), /a-z/);
  assert.throws(() => app.adminAddLine({ kind: 'login', name: 'ops', role: 'owner' }), /role/);
  assert.throws(() => app.adminAddLine({ kind: 'login', name: 'ops', role: 'admin', immunity: '100' }), /0 to 99/);
  assert.deepStrictEqual(app.adminView({ identity: 'cidr:192.168.1.0/24,10.0.0.0/8' }),
    { kind: 'network', title: '192.168.1.0/24, 10.0.0.0/8', identity: 'cidr:192.168.1.0/24,10.0.0.0/8' });
  assert.strictEqual(app.adminView({ identity: 'name:ops' }).title, 'ops');
  const secret = 'a1'.repeat(20);
  assert.strictEqual(app.secretFromLines(['admin ops made. The secret is shown this once:', secret]), secret);
  assert.strictEqual(app.secretFromLines(['admin row cidr:1.2.3.4 made']), '');
  assert.strictEqual(app.commandWord('name:Big Al'), '"name:Big Al"');
  assert.strictEqual(app.commandWord('ip:10.0.0.0/8'), 'ip:10.0.0.0/8');
  assert.throws(() => app.commandWord('name:a"b'), /quote/);
  assert.deepStrictEqual(app.whitelistView('ip:192.168.1.0/24'), { kind: 'Address', value: '192.168.1.0/24', entry: 'ip:192.168.1.0/24', mono: true });
  assert.strictEqual(app.whitelistView('name:Milenko').mono, false);
});

test('backups by file, sizes for reading, and a bans file before its import', () => {
  assert.deepStrictEqual(['0', '412', '18204', '2516582'].map((size) => app.formatBytes(Number(size))), ['0 B', '412 B', '18 KB', '2.4 MB']);
  assert.strictEqual(app.formatBytes(-1), '–');
  const groups = app.backupGroups([{ file: 'config.toml', name: 'a', time: 1 }, { file: 'bans.txt', name: 'b', time: 5 },
    { file: 'config.toml', name: 'c', time: 9 }]);
  assert.deepStrictEqual(groups.map((group) => [group.file, group.backups.map((backup) => backup.name)]),
    [['config.toml', ['c', 'a']], ['bans.txt', ['b']]]);
  const now = 1_700_000_000_000;
  const existing = [{ ip: '198.51.100.23', hwid: '', discord_id: '', name_pattern: '' }];
  const file = JSON.stringify({ ok: true, bans: [
    { ip: '198.51.100.23', hwid: '', reason: 'already' },
    { ip: '203.0.113.0/24', players: 'Spam ring', until: null },
    { name_pattern: 'Sk8*', until: now / 1000 + 600 },
    { ip: '203.0.113.9', until: now / 1000 - 5 },
    { ip: 'not an address' }, { reason: 'bans nothing' },
    { ip: '203.0.113.0/24' },
  ] });
  const preview = app.importPreview(file, existing, now);
  assert.deepStrictEqual([preview.fresh.length, preview.have.length, preview.over.length, preview.wrong.length], [2, 2, 1, 2]);
  assert.strictEqual(app.banLine(preview.fresh[0], now), 'Spam ring, 203.0.113.0/24, for ever');
  assert.strictEqual(app.banLine(preview.fresh[1], now), 'Names like Sk8*, for 10 min');
  assert.strictEqual(app.importPreview('[]', [], now).fresh.length, 0);
  assert.throws(() => app.importPreview('{nope', [], now), /not JSON/);
  assert.throws(() => app.importPreview('{"x":1}', [], now), /no list/);
});


test('a setting\'s help is in plain words, short, and has no code in it', () => {
  assert.strictEqual(app.settingHelp('server.frame_rate', 'raw'), app.SETTING_HELP['server.frame_rate']);
  assert.strictEqual(app.settingHelp('server.unknown_one', 'the comment'), 'the comment');
  for (const [name, text] of Object.entries(app.SETTING_HELP)) {
    assert.ok(text.length >= 20 && text.length <= 160, `${name}: ${text.length} characters`);
    assert.ok(!/[a-z]+_[a-z]+|HALO_|\\"|\bsv_/.test(text), `${name} quotes a key: ${text}`);
    assert.ok(!/\b(server|network)\.[a-z_]+/.test(text), `${name} names another setting's key: ${text}`);
  }
});
