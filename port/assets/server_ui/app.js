/*
APP.JS

The dedicated server's admin page (docs/dedicated_server.md: the HTTP API
and the RCON). It is served with Content-Security-Policy default-src 'self',
so it makes every element with createElement and sets text with textContent:
no HTML from strings, no inline styles or handlers.

- The session is the HttpOnly cookie POST /api/login sets; the page keeps
  only its CSRF token, sent on every change (X-CSRF-Token). It ends after
  30 minutes without an HTTP request (the WebSocket's do not count): the
  page asks GET /api/session every 5 minutes while it is open.
- What the server sends may be older or newer than the page: settings'
  values typed or as text, maps with or without a title, gametypes with or
  without their engine. The page takes either.
- Events and the status come over the RCON's WebSocket (/ws, signed in by
  the cookie); while it is down the page polls /api/status, /api/players,
  /api/log and /api/events every 3 seconds.
- Everything but startPage uses no document: Node tests it
  (tools/server_ui_tests).
*/

'use strict';

const ROLES = ['viewer', 'moderator', 'admin', 'owner'];
const CATEGORIES = ['server', 'lobby', 'game', 'player', 'chat', 'moderation', 'net', 'master', 'console', 'health'];
const GROUPS = ['player', 'chat', 'game', 'moderation', 'status', 'log', 'master'];
const BAN_PRESETS = [['1h', '1 hour'], ['1d', '1 day'], ['7d', '7 days'], ['30d', '30 days'], ['forever', 'For ever']];
// each gametype's game engine, when /api/gametypes's builtin[] names them
// alone: the builders' of source/game/game_engine.c for the built-ins
// (port/linux/game/server_gametypes.c), with their aliases, and game_modes.c's
// for OpenCE's
const ENGINES = {
  slayer: 'slayer', team_slayer: 'slayer', slayer_pro: 'slayer', elimination: 'slayer', phantoms: 'slayer',
  endurance: 'slayer', rockets: 'slayer', snipers: 'slayer',
  ctf: 'ctf', ironctf: 'ctf', ctf_pro: 'ctf', invasion: 'ctf',
  oddball: 'oddball', team_oddball: 'oddball', reverse_tag: 'oddball', accumulation: 'oddball', juggernaut: 'oddball',
  stalker: 'oddball',
  king: 'king', team_king: 'king', king_pro: 'king', crazy_king: 'king',
  race: 'race', team_race: 'race', rally: 'race', team_rally: 'race',
  ffa: 'slayer', ffa_slayer: 'slayer', teamslayer: 'slayer', tslayer: 'slayer', koth: 'king', team_koth: 'king',
  iron_ctf: 'ctf', teamrace: 'race', teamoddball: 'oddball',
  swat: 'slayer', infection: 'slayer', gun_game: 'slayer', hardcore: 'slayer', hardcore_ctf: 'ctf',
  hardcore_oddball: 'oddball', hardcore_king: 'king',
};
// the settings the server's own console alone changes (server_commands.c's
// owner_settings): the page's admin is refused them
const OWNER_SETTINGS = ['server.status_file', 'server.auto_update', 'server.log_addresses'];
const SESSION_REFRESH = 5 * 60 * 1000;
const PLAYER_COMMANDS = ['tell', 'kick', 'ban', 'mute', 'unmute'];
const LOG_LINES = 2048;
const SPARK_WINDOW = 600000;

// ---------- roles

function roleAtLeast(role, needed) {
  return ROLES.includes(role) && ROLES.indexOf(role) >= ROLES.indexOf(needed);
}

// a moderator bans for a day at most (the server checks it too)
function banPresets(role) {
  return BAN_PRESETS.map(([value, label]) => ({ value, label, allowed: roleAtLeast(role, 'admin') || /^1[hd]$/.test(value) }));
}

// ---------- formatting

const pad = (number) => String(number).padStart(2, '0');

function formatDuration(seconds) {
  seconds = Math.max(0, Math.floor(Number(seconds) || 0));
  const minutes = Math.floor(seconds / 60);
  const hours = Math.floor(minutes / 60);
  if (seconds < 60) return `${seconds} s`;
  if (minutes < 60) return `${minutes} min`;
  if (hours < 48) return `${hours} h ${pad(minutes % 60)} min`;
  return `${Math.floor(hours / 24)} d ${hours % 24} h`;
}

function formatClock(seconds) {
  seconds = Math.max(0, Math.floor(Number(seconds) || 0));
  const hours = Math.floor(seconds / 3600);
  const rest = `${pad(Math.floor(seconds / 60) % 60)}:${pad(seconds % 60)}`;
  return hours ? `${hours}:${rest}` : rest.replace(/^0/, '');
}

function formatTime(ms) {
  const date = new Date(Number(ms) || 0);
  return `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
}

function formatAgo(unix, nowMs = Date.now()) {
  return unix ? formatDuration(nowMs / 1000 - unix) + ' ago' : '–';
}

function formatUntil(until, nowMs = Date.now()) {
  if (until === null || until === undefined) return 'For ever';
  const left = until - nowMs / 1000;
  return left <= 0 ? 'Ended' : 'For ' + formatDuration(left);
}

const teamName = (team) => (team === 0 ? 'Red' : team === 1 ? 'Blue' : '–');

const STATES = { waiting: 'Starting', starting: 'Starting', hosting: 'Making the game', lobby: 'Lobby',
  loading: 'Loading', playing: 'Playing', postgame: 'Postgame', stopping: 'Stopping' };
const stateLabel = (state) => STATES[state] || String(state || '–');

// a time in milliseconds, rounded for reading: 2 decimals under 1 ms, 1 under
// 100, none above, no trailing zero ('–' when the server sends none)
function formatMs(value) {
  const ms = Number(value);
  if (value === null || value === undefined || value === '' || !Number.isFinite(ms)) return '–';
  return `${parseFloat(ms.toFixed(ms < 1 ? 2 : ms < 100 ? 1 : 0))} ms`;
}

// the status's gametype in words: the variant's name as the game shows it
// (else the gametype's); '' when the server has none yet. Teams are not
// said: the score's row shows them
function gametypeText(status, gametypes) {
  const variant = String((status && status.variant) || '').trim();
  return variant || gametypeTitle((status && status.gametype) || '', gametypes);
}

// ---------- names for people: maps, gametypes, rotation rules (the page
// shows no machine word as its main text; the word itself goes in a title
// or a small line under it)

// a word in words: "team_global_enemy_proximity" -> "Team global enemy
// proximity" (a few short words kept as people write them)
const WORDS = { coop: 'co-op', ctf: 'CTF', ffa: 'FFA', pc: 'PC', ae: 'AE', btb: 'BTB', swat: 'SWAT', mcc: 'MCC', ce: 'CE',
  ui: 'UI', vpn: 'VPN', lan: 'LAN', afk: 'AFK', bsp: 'BSP', id: 'id', ip: 'IP', hud: 'HUD', fov: 'FOV' };
function humanize(word) {
  const text = String(word ?? '').trim().split(/[_\s]+/).filter(Boolean).map((part) => WORDS[part.toLowerCase()] || part).join(' ');
  return text.charAt(0).toUpperCase() + text.slice(1);
}
// a map's file name in words, each word capitalized ("pat_beavercreek":
// "Pat Beavercreek")
const titleCase = (word) => humanize(word).replace(/(^|\s)([a-z])/g, (all, space, letter) => space + letter.toUpperCase());

// the stock multiplayer maps' names (as server_status.c's), for a map named
// by its file alone (an MCC copy, bloodgulch@mcc)
const MAP_TITLES = { beavercreek: 'Battle Creek', bloodgulch: 'Blood Gulch', boardingaction: 'Boarding Action',
  carousel: 'Derelict', chillout: 'Chill Out', damnation: 'Damnation', dangercanyon: 'Danger Canyon',
  deathisland: 'Death Island', gephyrophobia: 'Gephyrophobia', hangemhigh: "Hang 'Em High", icefields: 'Ice Fields',
  infinity: 'Infinity', longest: 'Longest', prisoner: 'Prisoner', putput: 'Chiron TL-34', ratrace: 'Rat Race',
  sidewinder: 'Sidewinder', timberland: 'Timberland', wizard: 'Wizard' };
// where a map comes from, as a tag: <file>@<pack> (an Xbox map pack), @mcc,
// @ws<item> (Workshop), @ce or custom_maps\ (Custom Edition)
const PACK_TAGS = { mcc: 'MCC', ce: 'CE', smp: 'SMP', pro: 'PRO', pe: 'PE', cex: 'CE+', md: 'HaloMD', nhe: 'NHE' };

// a rotation's map word as people know it: {title, tag, word}; maps is
// /api/maps's list (titles where the server has them)
// (named: a rotation entry's map_title and pack, the server's names for a
// pack's map, when it sent them)
function mapView(word, maps, named) {
  const text = String(word || '');
  let base = text;
  let tag = '';
  const custom = /^custom_maps[\\/]/i.exec(base);
  if (custom) {
    base = base.slice(custom[0].length);
    tag = 'CE';
  }
  base = base.split(/[\\/]/).pop();
  const at = base.indexOf('@');
  if (at > 0) {
    const pack = base.slice(at + 1).toLowerCase();
    base = base.slice(0, at);
    tag = /^ws\d+$/.test(pack) ? 'Workshop' : PACK_TAGS[pack] || pack.toUpperCase();
  }
  const found = (maps || []).find((map) => map && (map.name === text || map.name === base || map.level === text));
  if (found && found.kind === 'custom' && !tag) tag = 'CE';
  const title = (named && named.map_title) || (found && found.title && found.title !== found.name ? found.title : '');
  const view = { title: title || MAP_TITLES[base.toLowerCase()] || titleCase(base), tag, word: text };
  if (named && named.pack) view.pack = named.pack;
  return view;
}

// the stock gametypes' names as the game shows them
const GAMETYPE_TITLES = { slayer: 'Slayer', team_slayer: 'Team Slayer', slayer_pro: 'Slayer Pro', ctf: 'CTF',
  ironctf: 'Iron CTF', ctf_pro: 'CTF Pro', king: 'King of the Hill', team_king: 'Team King of the Hill',
  king_pro: 'King Pro', crazy_king: 'Crazy King', reverse_tag: 'Reverse Tag', team_race: 'Team Race',
  team_rally: 'Team Rally', team_oddball: 'Team Oddball', gun_game: 'Gun Game', hardcore_ctf: 'Hardcore CTF',
  coop: 'Co-op' };
// a rotation's gametype word as people know it: Arena Evolved's built-ins by
// their menus' name, a file by its name inside, the stock ones by the game's
function gametypeTitle(word, gametypes) {
  const text = String(word || '');
  if (!text) return '';
  const g = gametypes || {};
  const ae = (Array.isArray(g.ae_builtin) ? g.ae_builtin : []).find((item) => item && item.name === text);
  if (ae && ae.display) return ae.display;
  const file = (Array.isArray(g.files) ? g.files : []).find((item) => item && item.name === text);
  if (file && file.description) return file.description;
  return GAMETYPE_TITLES[text.toLowerCase()] || titleCase(text);
}

// a rotation rule in words ("score=50": "First to 50"); the server's
// aliases taken; true and false (or 1 and 0) for a rule that is on or off
const RULE_ALIASES = { score: 'score_to_win', time: 'time_limit', respawn: 'respawn_time', weapons: 'weapon_set',
  vehicles: 'vehicle_set', ff: 'friendly_fire', balance: 'auto_team_balance' };
const RULE_SWITCHES = new Set(['teams', 'odd_man_out', 'radar', 'friend_indicators', 'infinite_grenades', 'no_shields',
  'invisible', 'generic_equipment', 'auto_team_balance', 'no_map_weapons', 'hardcore', 'assault', 'reset_on_capture',
  'flag_must_reset', 'flag_at_home_to_score', 'no_death_bonus', 'no_kill_penalty', 'kill_in_order', 'moving_hill',
  'random_start', 'ball_spawn_delay']);
function ruleText(key, value) {
  const name = RULE_ALIASES[key] || key;
  const number = Number(value);
  const counted = (one, many) => `${number} ${number === 1 ? one : many}`;
  if (RULE_SWITCHES.has(name)) {
    const on = value === true || value === 1 || value === 'true' || value === '1';
    return on ? humanize(name) : `No ${humanize(name).toLowerCase()}`;
  }
  switch (name) {
    case 'score_to_win': return number ? `First to ${number}` : 'No score limit';
    case 'time_limit': return number ? `${number} min` : 'No time limit';
    case 'lives': return number ? counted('life', 'lives') : 'Unlimited lives';
    case 'respawn_time': return `Respawn ${number} s`;
    case 'health': return `${number}% health`;
    case 'vehicle_respawn_time': return number ? `Vehicles back in ${number} s` : 'Vehicles never back';
    case 'warthogs': case 'ghosts': case 'scorpions': case 'rocket_warthogs': case 'banshees': case 'gun_turrets':
      return `${number} ${humanize(name).toLowerCase()}`;
    default: return `${humanize(name)}: ${typeof value === 'string' ? humanize(value).toLowerCase() : value}`;
  }
}

// a rotation entry (an /api/rotation entry, or its text) for people:
// {map: mapView, gametype: its name, rules: [words], text: the entry}
function entryView(entry, maps, gametypes) {
  const text = typeof entry === 'string' ? entry : String((entry && entry.text) || '');
  // (the map's word from the text, where custom_maps\ and @<pack> stay; the
  // server's typed rules when it sent them)
  const object = typeof entry === 'object' && entry ? entry : {};
  const parsed = parseEntryText(text) || { map: object.map || text, gametype: object.gametype || '', rules: {} };
  const rules = object.rules && Object.keys(object.rules).length ? object.rules : parsed.rules;
  return { map: mapView(parsed.map, maps, object), gametype: gametypeTitle(parsed.gametype || object.gametype, gametypes),
    rules: Object.entries(rules || {}).map(([key, value]) => ruleText(key, value)), text };
}
// an entry in one line of words: "Sidewinder (PRO), Team Slayer, first to 50, 15 min"
function entryLabel(view) {
  return [view.map.title + (view.map.tag ? ` (${view.map.tag})` : ''), view.gametype,
    ...view.rules.map((rule) => rule.charAt(0).toLowerCase() + rule.slice(1))].filter(Boolean).join(', ');
}

// a long word (an address, a rotation entry) in pieces, each ending at a
// '.', ':', '+', '/', '\' or ',': the page lets a line break only there
const breakParts = (text) => String(text ?? '').match(/[^.:+/\\,]*(?:[.:+/\\,]+|$)/g).filter(Boolean);

// a master server's line: its host (the whole address in the title), and
// how it stands: listed, not published (the server does not publish: a
// private server, or no publisher key), waiting for its probe, or failed
function masterView(master) {
  const url = String((master && master.url) || '');
  const host = url.replace(/^[a-z]+:\/\//i, '').replace(/\/.*$/, '') || url;
  const code = String(master.code || '');
  const answer = [code, master.message].filter(Boolean).join(': ');
  if (master.listed) return { url, host, kind: 'ok', text: 'Listed' };
  if (code === 'no_key') return { url, host, kind: 'off', text: 'Not published: no publisher key' };
  if (master.probe === 'off' || code === 'off') return { url, host, kind: 'off', text: 'Not published' };
  if (master.probe === 'failed' || code) return { url, host, kind: 'bad', text: answer ? `Failed: ${answer}` : 'Failed' };
  return { url, host, kind: 'wait', text: 'Waiting for its probe' };
}

// what each setting does, in the words a host would use (the server's own
// comments, which also head config.toml, are the fallback for any not here)
const SETTING_HELP = {
  'server.name': 'The name players see in the lobby and the server list. Up to 15 letters, digits and symbols.',
  'server.max_players': 'The most players the server lets in (2 to 127).',
  'server.min_players': 'Players needed in the lobby before the countdown starts (1 to 16).',
  'server.password': 'Players must type this to join from the list. Invite links still work. Leave empty for none.',
  'server.public': 'Show the server in the public list. Off: players join by invite link or on the LAN.',
  'server.rotation_fallback': 'When no map in the rotation can be played, play Blood Gulch slayer instead of stopping.',
  'server.rotation_shuffle': 'Play the rotation in a new random order each pass. The same map never comes twice in a row.',
  'server.lobby_seconds': 'Seconds the lobby counts down before a game starts (0 to 300). 0 starts at once.',
  'server.postgame_seconds': 'Seconds the final scores stay up after a game (0 to 300).',
  'server.empty_seconds': 'Seconds an empty game keeps going before it ends. 0 never ends it. A game ends at once if the last player leaves.',
  'server.idle_minutes': 'Minutes a game can go without anyone scoring before it ends. 0 never ends it.',
  'server.team_start': 'How a team game starts: one team is enough, both teams must have a player, or skip it until both do.',
  'server.motd': 'A message each player sees as they join (up to 127 characters). Leave empty for none.',
  'server.frame_rate': 'Frames a second the server runs (30 to 120). With nobody online it idles at 10.',
  'server.web_password': 'The admin password for this web page (8 characters or more). Empty: one is made at first start and shown once.',
  'server.web_moderator_password': 'A second password for moderators: kick, mute, say, next map and short bans. Leave empty for none.',
  'server.metrics': 'Publish server numbers for monitoring tools at /metrics with no password. Off unless you need it.',
  'server.log_format': 'How the server writes its console lines: plain text or JSON.',
  'server.log_addresses': "Put players' internet addresses in the log and in what every role sees. The owner's console always sees them.",
  'server.status_file': 'A file the server rewrites every 2 seconds with its status, for panels that cannot call the web page.',
  'server.auto_update': 'Check only reports a new release. Install updates and restarts when the server is quiet. Off never looks.',
  'server.update_check_minutes': 'Minutes between looks for a new release (5 to 1440).',
  'server.update_game_minutes': 'After a new network version is installed, minutes a running game may finish before the restart. 0 waits for it to end.',
  'server.coop_difficulty': 'Difficulty of co-op campaign levels in the rotation: easy, normal, hard or impossible.',
  'server.master_publish': 'List this public server with the master servers, using the server key.',
  'server.master_publish_address': 'Let the master servers hand this address to players. Off: they only use it to check the server is reachable.',
  'server.region': "A region tag shown in the server list, such as eu or na-east (16 characters). Leave empty for none.",
  'server.chat_filter': 'Keep chosen words out of chat and player names. Censor turns them into stars. Block refuses the line, or the name at the door.',
  'server.chat_filter_slurs': 'How much of the slur list the chat filter uses: off, severe only, strong or mild (everything).',
  'server.chat_filter_swearing': 'How much of the swearing and sexual word list the chat filter uses: off, severe, strong or mild (everything).',
  'server.chat_filter_insults': 'How much of the insult list the chat filter uses: off, severe, strong or mild (everything).',
  'server.vpn_policy': 'Players joining from a VPN, Tor or data centre address. Link: allowed only with a linked Discord account. Block: refused. Off: allowed.',
  'server.vpn_fail': 'Until the address list has loaded: open lets everyone in, closed refuses internet players as Block would.',
  'server.vpn_check_lan': 'Also check players on the local network against the list. For testing.',
  'server.link_min_age_days': 'Over a VPN, a linked Discord account must be at least this many days old (0: any age).',
  'server.link_one_seat': 'Refuse a second machine from a Discord account that already has one in the game.',
  'server.max_machines_per_address': 'Most machines that may play from one internet address (0: no limit). Homes, cafes and mobile networks share addresses.',
  'server.afk_kick_minutes': 'Minutes a player may stand still in a game before being kicked, warned a minute ahead (0: never).',
  'server.afk_kick_when_full': 'Kick idle players only while the server is full, using the shorter time below.',
  'server.afk_kick_full_minutes': 'Minutes a player may stand still while the server is full, when the option above is on.',
  'server.coop_bsp_switch': 'Co-op loading zones. Majority: more than half the active players must be in the zone. Anyone: one player brings everyone.',
  'server.coop_bsp_switch_cooldown_seconds': 'Seconds after a co-op zone switches the level before another can (0 to 300).',
  'server.map_vote': 'After each game, players vote for the next one from a few random rotation entries (never the one just played).',
  'server.map_vote_choices': 'How many entries the map vote offers (2 to 8).',
  'server.map_vote_seconds': 'How long the map vote runs (5 to 120). It ends early once everyone has voted.',
  'server.bots_fill': 'Bots keep games full. Minimum: up to the lobby minimum. Empty slots: fill the server. Auto picks minimum for AE modes. Bots are never rated.',
  'server.bots_skill': 'How well the bots play, from recruit to demon.',
  'server.bots_alone': 'On: bots play on their own when nobody is in. Off: the server waits for a player.',
  'server.bots_delay': 'Seconds the lobby waits for people, once the first is in, before bots fill it (0 to 300).',
  'server.restart_warning_seconds': 'Warning players get before a stop or restart: notices at the start, then 60, 30, 10 and 5 seconds. 0 stops at once.',
  'server.restart_daily': 'Restart every day at this time (HH:MM, 24 hours, the server machine\'s clock), with the warning above. Empty: never.',
  'server.backups_keep': 'How many older copies of the settings, bans and admin files to keep (1 to 1000). One is made before each change.',
  'server.whitelist': 'Only players listed in the whitelist file join, by address, hardware id, Discord account or name. Admins always can.',
  'server.reserved_slots': 'Keep this many of the last slots for admins. Everyone else is told the server is full (0 to 32).',
  'server.chat_flood_lines': 'Mute a player who sends more than this many chat lines in 5 seconds (2 to 7 works well). 0: off.',
  'server.chat_flood_mute_minutes': 'How long a chat flood mutes the player (1 to 1440 minutes).',
  'server.ping_kick_ms': 'Kick a player whose ping stays above this many milliseconds (they can rejoin). 0: off.',
  'server.ping_kick_seconds': 'How long the ping must stay too high before the kick (5 to 600 seconds).',
  'server.alert_disk_mb': 'With an alert address set: warn when free disk space falls below this many megabytes (0: never).',
  'server.alert_empty_hours': 'With an alert address set: warn once when nobody has played for this many hours (0: off).',
  'server.alert_restart_loop': 'With an alert address set: warn when the server starts this many times in 10 minutes (0: off).',
  'server.alert_webhook': "A web address the server posts alerts to: restart loops, refused listings, updates, low disk. Treat it as a secret. Owner only.",
  'server.on_event': 'A file of event hooks that run server commands when something happens. Owner only. Empty: none.',
  'network.marks': 'Players can mark and call from the wheel. Off: nobody\'s marks are made or shown.',
  'network.text_chat': 'Text chat: on (links refused), links (links allowed) or off. Always off in a public game a player hosts.',
  'network.voice_lobby': 'Voice chat in the lobby, before and after games, where everyone hears everyone.',
  'network.voice_mode': 'Voice chat in games: off, teammates or enemies by distance, or all teammates plus nearby enemies.',
  'network.voice_kbps': 'Voice quality in kilobits a second (8 to 64). Higher sounds better and uses more bandwidth.',
  'network.votekick': 'Let players vote to kick someone. Yes votes needed follow the threshold setting.',
  'network.vote_end_game': 'Let players vote to end the game by typing !endgame. It counts like a kick vote.',
  'network.votekick_threshold': 'Yes votes a vote needs. Dynamic scales with player count. Static uses the percentage below. Never fewer than 2.',
  'network.votekick_percent': 'With a static threshold, the percentage of eligible players who must vote yes (1 to 100).',
  'network.votekick_minutes': 'Minutes a player must have played here before starting a kick vote (0 to 60).',
  'network.votekick_ban_minutes': 'Minutes a player kicked by a vote cannot rejoin (1 to 1440).',
  'network.map_share': 'Send joining players the map they lack, and the shared files it needs. Other players in the game help send.',
  'network.map_share_upload': 'The most kilobytes a second the server uses to send maps (16 to 20480), so game traffic keeps the rest.',
  'network.public_address': 'The internet address players reach this server at, as host or host:port. Use it behind a router with one forwarded port.',
  'network.coop_friendly_fire': 'Whether co-op players hurt each other: off, on, shields only or explosives only.',
  'network.coop_ally_friendly_fire': 'Whether co-op players can hurt their AI allies, as in the campaign.',
  'network.coop_player_collisions': 'Whether co-op players bump into each other. Off: they walk through.',
  'network.coop_team_wipe': 'When every co-op player dies. Auto: restart from the checkpoint in private games, respawn in public ones. Or force either.',
  'network.shield_effects': 'The shield effect everyone sees in network games, whatever their own settings. Players: each sees their own.',
  'network.fire_origins': 'Count a shot that left from the shooter\'s hand or weapon rather than the camera, within a short reach. Off: camera only.',
};
const settingHelp = (setting, comment) => SETTING_HELP[setting] || comment;

// a setting's name as the menus would print it: the key after its table,
// in words (network.shield_effects: "shield effects"; the page sets it in
// capitals)
const SETTING_WORDS = { coop: 'co-op', motd: 'message of the day', afk: 'AFK', vpn: 'VPN', lan: 'LAN', bsp: 'BSP' };
function settingLabel(name) {
  const key = String(name || '').split('.').slice(1).join(' ') || String(name || '');
  return key.split('_').map((word) => SETTING_WORDS[word] || word).join(' ');
}

// the settings' sections: a table's settings by what they are about (the
// first rule whose pattern the key matches; the rest "General")
const SETTING_TOPICS = {
  network: [[/^(voice|text_chat|marks)/, 'Voice and chat'], [/^vote/, 'Votes'], [/^map_share/, 'Map sharing'],
    [/^coop/, 'Co-op']],
  server: [[/^(name|motd|password|public|region|master_)/, 'Name and listing'],
    [/^(max_players|min_players|team_start|lobby|postgame|empty|idle|frame_rate)/, 'Games'],
    [/^(rotation|map_vote)/, 'Rotation and map vote'], [/^bots/, 'Bots'], [/^(chat_filter|afk)/, 'Chat and AFK'],
    [/^(vpn|link_|max_machines)/, 'Who may join'], [/^(web_|metrics|log_|status_file)/, 'Web page and logs'],
    [/^(auto_update|update_)/, 'Updates'], [/^coop/, 'Co-op']],
};
function settingTopic(name) {
  const [table, ...rest] = String(name || '').split('.');
  const key = rest.join('.');
  const rule = (SETTING_TOPICS[table] || []).find(([pattern]) => pattern.test(key));
  return { table, topic: rule ? rule[1] : 'General' };
}

// a refusal as a sentence (the error codes of docs/dedicated_server.md)
function errorText(result) {
  const error = (result && result.error) || {};
  const message = error.message || '';
  const rejected = error.rejected || (result && result.data && result.data.rejected) || [];
  switch (error.code) {
    case 'network': return 'The server cannot be reached.';
    case 'denied': return message === 'wrong password' ? 'Wrong password.' : 'Signed out: sign in again.';
    case 'locked': return `Too many wrong passwords: try again in ${formatDuration(error.retry_after || 600)}.`;
    case 'rate_limited': return `Too many at once: try again in ${formatDuration(error.retry_after || 60)}.`;
    case 'csrf': return 'The page is out of date: reload it.';
    case 'origin': return 'Refused: the request did not come from this page.';
    case 'loading': return 'A map is loading: try again in a few seconds.';
    case 'timeout': return 'No answer in time (it may still have run).';
    case 'invalid': return error.line ? `Line ${error.line}: ${message}` : message;
    case 'schema': return error.field ? `${error.field}: ${message}` : message;
    case 'failed': return message || 'The command failed.';
    case 'rejected': return [message, ...rejected.map((item) => `${item.entry}: ${item.reason}`)].filter(Boolean).join('; ');
    default: return message || `Refused (${error.code || (result && result.status) || 'unknown'}).`;
  }
}

// ---------- the API: fetch, with the CSRF token on changes

function createApi(fetchFunction, base = '') {
  let csrf = '';
  return {
    setCsrf(token) {
      csrf = token || '';
    },
    // {ok, status, data (the answer's JSON), error (a refusal's)}
    async call(method, path, body) {
      const headers = { Accept: 'application/json' };
      if (body !== undefined) headers['Content-Type'] = 'application/json';
      if (method !== 'GET') headers['X-CSRF-Token'] = csrf;
      let response;
      let data = null;
      try {
        response = await fetchFunction(base + path, { method, headers, credentials: 'same-origin', cache: 'no-store',
          body: body === undefined ? undefined : JSON.stringify(body) });
      } catch {
        return { ok: false, status: 0, data: {}, error: { code: 'network', message: 'the server cannot be reached' } };
      }
      try {
        data = await response.json();
      } catch {
        // (not JSON: a proxy's page)
      }
      if (!data || typeof data !== 'object') data = {};
      const result = { ok: response.ok && data.ok !== false, status: response.status, data };
      if (!result.ok) {
        result.error = data.error && typeof data.error === 'object' ? data.error :
          { code: response.status === 401 ? 'denied' : 'unknown', message: `HTTP ${response.status}` };
        const retry = Number(response.headers && response.headers.get('Retry-After'));
        if (retry && !result.error.retry_after) result.error.retry_after = retry;
      }
      return result;
    },
  };
}

// ---------- the log: a cursor, and lines kept in order

// an answer of /api/log?after=<cursor> (the cursor: the highest n read):
// the lines not seen before, the next cursor (the server's next: the last
// line it read, those it kept from this role, chat's, counted too), and
// whether the numbering started again (the server restarted: it answers a
// cursor past its latest from its first line)
function takeLog(state, answer) {
  const lines = (answer && Array.isArray(answer.lines) ? answer.lines : []).filter((line) => line && Number.isInteger(line.n));
  const cursor = state.cursor || 0;
  const last = lines.length ? lines[lines.length - 1].n : null;
  const given = answer && Number.isInteger(answer.next) ? answer.next : null;
  const reset = given !== null ? given < cursor : last !== null && last < cursor;
  const fresh = reset ? lines : lines.filter((line) => line.n > cursor);
  const read = given !== null ? given : last !== null ? last : cursor;
  const next = reset ? read : Math.max(cursor, read);
  return { cursor: next, lines: fresh, reset, missed: Number(answer && answer.missed) || 0 };
}

// lines into a list in the order of their n, those it has left out, the
// oldest dropped past max; appended: all the new ones went at the end
function mergeLog(list, incoming, max = LOG_LINES) {
  const have = new Set(list.map((line) => line.n));
  const fresh = incoming.filter((line) => !have.has(line.n));
  const tail = list.length ? list[list.length - 1].n : -Infinity;
  const appended = fresh.every((line, index) => line.n > (index ? fresh[index - 1].n : tail));
  let lines = list.concat(fresh);
  if (!appended) lines.sort((a, b) => a.n - b.n);
  const dropped = Math.max(0, lines.length - max);
  if (dropped) lines = lines.slice(dropped);
  return { lines, fresh, appended, dropped };
}

// ---------- the player-count sparkline

function pushSample(history, time, value, windowMs = SPARK_WINDOW) {
  history.push({ time, value: Number(value) || 0 });
  while (history.length > 1 && history[1].time <= time - windowMs) history.shift();
  return history;
}

// the samples as an SVG polyline's points (steps), the right edge now
function sparkPoints(history, now, width, height, max, windowMs = SPARK_WINDOW) {
  const top = Math.max(1, max || 0, ...history.map((item) => item.value));
  const x = (time) => Math.max(0, Math.min(width, width - ((now - time) / windowMs) * width));
  const y = (value) => height - 2 - (value / top) * (height - 4);
  const points = [];
  history.forEach((item, index) => {
    if (index) points.push([x(item.time), y(history[index - 1].value)]);
    points.push([x(item.time), y(item.value)]);
  });
  if (history.length) points.push([width, y(history[history.length - 1].value)]);
  return points.map(([px, py]) => `${px.toFixed(1)},${py.toFixed(1)}`).join(' ');
}

// ---------- events, in words: {kind, text}; quiet ones stay out of the feed

function describeEvent(event, players, names) {
  const mapName = (names && names.map) || ((word) => mapView(word).title);
  const name = (slot) => ((players || []).find((item) => item.slot === slot) || { name: slot < 0 ? 'the world' : `slot ${slot}` }).name;
  const by = (event.by ? ` by ${event.by}` : '') + (event.reason ? `: ${event.reason}` : '');
  const gametype = event.variant || ((names && names.gametype) || gametypeTitle)(event.gametype);
  const say = {
    player_joined: ['player', `${event.name} joined (slot ${event.slot})`],
    player_left: ['player', `${event.name} ${{ kicked: 'was kicked', banned: 'was banned' }[event.reason] ||
      `left (${event.reason || 'left'})`}`],
    chat: ['chat', `${event.name}: ${event.text}`],
    lobby_opened: ['game', `Lobby: ${gametype} on ${mapName(event.map)}`],
    game_started: ['game', `Game started: ${gametype} on ${mapName(event.map)}`],
    game_ended: ['game', `Game ended (${event.reason}) after ${formatClock(event.duration)}`],
    postgame: ['game', `Postgame: ${event.seconds} s`],
    kick: ['moderation', `${event.name} kicked${by}`],
    ban: ['moderation', `${event.name} banned${by}`],
    unban: ['moderation', `${event.name} unbanned${by}`],
    mute: ['moderation', `${event.name} muted${by}`],
    unmute: ['moderation', `${event.name} unmuted${by}`],
    speed_hack: ['bad', `Speed hack: machine ${event.machine} (${[].concat(event.names || []).join(', ')}) at ${event.rate}×`],
    master: [masterView(event).kind === 'bad' ? 'bad' : 'master', `${masterView(event).host}: ${masterView(event).text.toLowerCase()}`],
    warning: ['bad', event.text],
    // (progress: in percent, 0 to 100)
    map_share: ['net', `Machine ${event.machine} gets ${mapName(event.map)}: ${Math.round(Number(event.progress) || 0)}%`],
    server_started: ['server', `Server started: ${event.name} (${event.build})`],
    reloaded: ['server', 'Settings, rotation and gametypes read again'],
    shutdown: ['bad', `Stopping: ${event.reason}`],
    kill: ['kill', `${name(event.killer)} killed ${name(event.victim)}`, true],
  }[event.event];
  return say ? { kind: say[0], text: say[1], quiet: !!say[2] } : null;
}

// ---------- the rotation's entries: map[:gametype[+rule=value…]]

function parseEntryText(text) {
  const match = /^([^:+\s]+)(?::([^+\s]+)((?:\+[^+\s]+)*))?$/.exec(String(text).trim());
  if (!match) return null;
  const rules = {};
  for (const rule of (match[3] || '').split('+').filter(Boolean)) {
    const [key, value = ''] = rule.split('=');
    rules[key] = /^-?\d+$/.test(value) ? Number(value) : value === 'true' ? true : value === 'false' ? false : value;
  }
  return { map: match[1], gametype: match[2] || '', rules };
}

// "score=50 time=15" or "+score=50+time=15": an entry's rules
function rulesText(text) {
  const rules = String(text || '').split(/[\s+,]+/).filter(Boolean);
  const bad = rules.find((rule) => !/^[a-z_]+=[^+\s=,]+$/.test(rule));
  if (bad) throw new Error(`${bad} is not a rule (key=value)`);
  return rules.map((rule) => '+' + rule).join('');
}

// a map of /api/maps as the rotation names it
function mapToken(map) {
  if (!map) return '';
  return map.kind === 'custom' && !/^custom_maps\\/.test(map.name) ? 'custom_maps\\' + map.name : map.name;
}

// a map of /api/maps as people know it: its title, else its name (a
// server that sends no title)
const titleOf = (map) => String((map && (map.title || map.name)) || '');

// POST /api/rotation/play's body for an entry: map and gametype, or the
// entry's whole text when it has rules (sv_map takes a rotation entry,
// map:gametype+rule=value, as its map)
function playBody(entry) {
  const text = String(entry.text || '').replace(/\s+/g, '');
  if (entry.gametype && (text.includes('+') || Object.keys(entry.rules || {}).length)) return { map: text };
  return entry.gametype ? { map: entry.map, gametype: entry.gametype } : { map: entry.map };
}

function makeEntry(map, gametype, rules) {
  const text = map + (gametype ? ':' + gametype + rulesText(rules) : '');
  const parsed = parseEntryText(text) || { map, rules: {} };
  return { text, map: parsed.map, gametype: gametype || '', rules: parsed.rules, rejected: null };
}

function rotationMove(entries, index, delta) {
  const moved = entries.slice();
  const target = index + delta;
  if (index < 0 || index >= entries.length || target < 0 || target >= entries.length) return moved;
  moved.splice(target, 0, moved.splice(index, 1)[0]);
  return moved;
}

const rotationRemove = (entries, index) => entries.filter((_, at) => at !== index);

function rotationInsert(entries, entry, at = entries.length) {
  const result = entries.slice();
  result.splice(Math.max(0, Math.min(at, result.length)), 0, entry);
  return result;
}

// PUT /api/rotation's rejected[] on the entries it names (by index)
function markRejected(entries, rejected) {
  const reasons = new Map((rejected || []).map((item) => [item.index, item.reason]));
  return entries.map((entry, index) => ({ ...entry, rejected: reasons.has(index) ? reasons.get(index) : null }));
}

// ---------- settings: a form from the server's table

// a setting's value of /api/settings as its type: a JSON boolean or number,
// or its text ("true", "16"), as servers have sent either; a value that is
// not of its type stays as it came
function settingValue(type, value) {
  if (type === 'boolean') {
    if (typeof value === 'boolean') return value;
    if (typeof value === 'number') return value !== 0;
    const text = String(value ?? '').trim().toLowerCase();
    if (/^(true|yes|on|1)$/.test(text)) return true;
    if (/^(false|no|off|0|)$/.test(text)) return false;
    return value;
  }
  if (type === 'integer' || type === 'real') {
    if (typeof value === 'number') return value;
    const text = String(value ?? '').trim();
    return text !== '' && Number.isFinite(Number(text)) ? Number(text) : value;
  }
  return value === null || value === undefined ? '' : String(value);
}

// a limit (min, max) as a number, or null
const settingLimit = (value) => (value === null || value === undefined || value === '' || !Number.isFinite(Number(value)) ? null : Number(value));

// the role that may change a setting: the server's own console (owner) for
// those of OWNER_SETTINGS or that the server marks so, else an admin
function settingRole(setting) {
  if (ROLES.includes(setting.role)) return setting.role;
  return setting.owner || OWNER_SETTINGS.includes(setting.name) ? 'owner' : 'admin';
}

// a setting of /api/settings with its value, default and limits typed
function normalizeSetting(setting) {
  return { ...setting, value: setting.secret ? '' : settingValue(setting.type, setting.value),
    default: setting.secret ? '' : settingValue(setting.type, setting.default),
    min: settingLimit(setting.min), max: settingLimit(setting.max),
    choices: Array.isArray(setting.choices) && setting.choices.length ? setting.choices.map(String) : null };
}

// whether the role may send a setting (the server refuses the rest)
const settingEditable = (setting, role) => !setting.pinned && roleAtLeast(role, settingRole(setting));

// a setting of /api/settings as a control: kind checkbox, number, select,
// text or secret (written only), disabled with its reason
function settingControl(raw, role) {
  const setting = normalizeSetting(raw);
  const choices = setting.choices;
  const number = setting.type === 'integer' || setting.type === 'real';
  const control = {
    name: setting.name, comment: setting.comment || '', env: setting.env || '', min: setting.min, max: setting.max,
    choices, value: setting.value, disabled: false, reason: '', step: number ? (setting.type === 'integer' ? '1' : 'any') : null,
    kind: setting.secret ? 'secret' : choices ? 'select' : setting.type === 'boolean' ? 'checkbox' : number ? 'number' : 'text',
  };
  if (setting.secret) {
    control.value = '';
    control.placeholder = setting.set ? 'Set: type to change it' : 'Not set';
  }
  if (control.kind === 'select' && !choices.includes(String(control.value))) control.choices = [String(control.value), ...choices];
  const needed = settingRole(setting);
  if (setting.pinned) [control.disabled, control.reason] = [true, `Set by ${setting.env}`];
  else if (needed === 'owner' && !roleAtLeast(role, 'owner')) [control.disabled, control.reason] = [true, "Only the server's console changes it"];
  else if (!roleAtLeast(role, needed)) [control.disabled, control.reason] = [true, `Needs the ${needed} role`];
  const fallback = setting.default;
  control.defaultText = setting.secret ? '' : setting.type === 'boolean' ? (fallback === true ? 'on' : fallback === false ? 'off' : String(fallback)) :
    fallback === '' || fallback === null || fallback === undefined ? '(empty)' : String(fallback);
  return control;
}

// the form's values ({name: a checkbox's boolean, an input's text, a
// secret's {text, clear}}) as PUT /api/settings's body: what changed, typed,
// and only what the role may change (role: the page's; none, an admin's)
function settingsChanges(settings, values, role = 'admin') {
  const body = {};
  const errors = [];
  for (const given of settings) {
    const setting = normalizeSetting(given);
    if (!(setting.name in values) || !settingEditable(setting, role)) continue;
    const raw = values[setting.name];
    if (setting.secret) {
      if (raw && (raw.clear || raw.text)) body[setting.name] = raw.clear ? '' : raw.text;
      continue;
    }
    let value;
    const fail = (message) => errors.push({ name: setting.name, message });
    if (setting.type === 'boolean') {
      value = settingValue('boolean', raw);
      if (typeof value !== 'boolean') {
        fail('on or off');
        continue;
      }
    } else if (setting.type === 'integer' || setting.type === 'real') {
      const text = String(raw ?? '').trim();
      if (!(setting.type === 'integer' ? /^-?\d+$/ : /^-?(\d+\.?\d*|\.\d+)(e-?\d+)?$/i).test(text)) {
        fail(setting.type === 'integer' ? 'a whole number' : 'a number');
        continue;
      }
      value = Number(text);
      if ((setting.min !== null && value < setting.min) || (setting.max !== null && value > setting.max)) {
        fail(setting.min !== null && setting.max !== null ? `${setting.min} to ${setting.max}` :
          setting.min !== null ? `${setting.min} or more` : `${setting.max} or less`);
        continue;
      }
    } else {
      value = String(raw ?? '');
      if (setting.choices && !setting.choices.includes(value) && value !== setting.value) {
        fail('one of ' + setting.choices.join(', '));
        continue;
      }
    }
    // (only what changed: the rest is not sent again)
    if (value !== setting.value) body[setting.name] = value;
  }
  return { body, errors };
}

// ---------- gametype files: their TOML, and a form from the schema

function stripComment(line) {
  let quoted = false;
  for (let index = 0; index < line.length; index++) {
    if (line[index] === '"' && line[index - 1] !== '\\') quoted = !quoted;
    else if (line[index] === '#' && !quoted) return line.slice(0, index);
  }
  return line;
}

function tomlValue(text) {
  if (text === 'true' || text === 'false') return text === 'true';
  if (/^[+-]?\d+$/.test(text)) return Number(text);
  const string = /^"((?:[^"\\]|\\.)*)"$/.exec(text);
  if (string) return string[1].replace(/\\(.)/g, (_, c) => ({ n: '\n', t: '\t' }[c] || c));
  if (/^\[[\d\s,+-]*\]$/.test(text)) return text.slice(1, -1).split(',').map((item) => item.trim()).filter(Boolean).map(Number);
  return undefined;
}

// the part of TOML gametype files use: {values: {table: {key: value}} ('' the
// top level), errors: [{line, message}]}
function parseToml(text) {
  const values = { '': {} };
  const errors = [];
  let table = '';
  String(text).split('\n').forEach((raw, index) => {
    const line = stripComment(raw).trim();
    const fail = (message) => errors.push({ line: index + 1, message });
    const header = /^\[\s*([A-Za-z_]\w*)\s*\]$/.exec(line);
    const pair = /^([A-Za-z_]\w*)\s*=\s*(.+)$/.exec(line);
    if (!line) return;
    if (header) {
      table = header[1];
      if (values[table]) fail(`[${table}] twice`);
      values[table] = values[table] || {};
    } else if (!pair) {
      fail('not a key = value line');
    } else {
      const value = tomlValue(pair[2].trim());
      if (value === undefined) fail(`${pair[2].trim().slice(0, 40)} is not a value`);
      else if (pair[1] in values[table]) fail(`${pair[1]} twice`);
      else values[table][pair[1]] = value;
    }
  });
  return { values, errors };
}

function formatTomlValue(value) {
  if (typeof value === 'boolean' || typeof value === 'number') return String(value);
  if (Array.isArray(value)) return `[${value.join(', ')}]`;
  return '"' + String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n') + '"';
}

// the text with key set in table (undefined: taken out), and the rest as it
// was: its comments, its order, its other keys
function setTomlValue(text, table, key, value) {
  const lines = String(text).split('\n');
  let current = '';
  let start = table === '' ? 0 : -1;
  let end = -1;
  let found = -1;
  lines.forEach((line, index) => {
    const header = /^\s*\[\s*([A-Za-z_]\w*)\s*\]\s*(#.*)?$/.exec(line);
    const pair = /^\s*([A-Za-z_]\w*)\s*=/.exec(line);
    if (header) {
      if (current === table && start >= 0 && end < 0) end = index;
      current = header[1];
      if (current === table && start < 0) start = index + 1;
    } else if (current === table && pair && pair[1] === key && found < 0) {
      found = index;
    }
  });
  if (start >= 0 && end < 0) end = lines.length;
  const line = value === undefined ? null : `${key} = ${formatTomlValue(value)}`;
  if (found >= 0) {
    const comment = lines[found].slice(stripComment(lines[found]).length);
    if (line === null) lines.splice(found, 1);
    else lines[found] = line + (comment ? ' ' + comment : '');
  } else if (line === null) {
    return text;
  } else if (start >= 0) {
    let at = end;
    while (at > start && lines[at - 1].trim() === '') at--;
    lines.splice(at, 0, line);
  } else {
    while (lines.length && lines[lines.length - 1].trim() === '') lines.pop();
    lines.push(...(lines.length ? [''] : []), `[${table}]`, line, '');
  }
  return lines.join('\n');
}

// /api/gametypes's builtin[]: names, or {name, engine} where the server says
// each one's engine
const builtinNames = (builtin) => (Array.isArray(builtin) ? builtin : [])
  .map((item) => (typeof item === 'string' ? item : item && item.name)).filter(Boolean);

// /api/gametypes's ae_builtin[] (Arena Evolved's built-in gametypes: name =
// the rotation word, display = the name the menus show, style and group as
// CHANGE GAMETYPE sorts them) in folders: [{style, count, groups: [{group,
// entries: [{name, display}]}]}], styles and groups in the menus' order, any
// other after them as the server lists it; a server without the list: []
const AE_STYLE_ORDER = ['AE CASUAL', 'AE COMP', 'CLASSIC COMP', 'TRAINING'];
const AE_GROUP_ORDER = ['TEAM ARENA', 'TEAM', 'DOUBLES', 'BIG TEAM', 'FREE FOR ALL', '1V1 / FFA', 'OBJECTIVE', 'ACTION SACK'];
function aeBuiltinFolders(list) {
  const rank = (order, word, seen) => {
    const at = order.indexOf(word);
    return at >= 0 ? at : order.length + seen;
  };
  const styles = [];
  for (const item of Array.isArray(list) ? list : []) {
    if (!item || typeof item !== 'object' || !item.name) continue;
    const styleName = String(item.style || '').trim() || 'OTHER';
    const groupName = String(item.group || '').trim();
    let style = styles.find((entry) => styleName === entry.style);
    if (!style) styles.push(style = { style: styleName, count: 0, groups: [], seen: styles.length });
    let group = style.groups.find((entry) => entry.group === groupName);
    if (!group) style.groups.push(group = { group: groupName, entries: [], seen: style.groups.length });
    group.entries.push({ name: String(item.name), display: String(item.display || item.name) });
    style.count++;
  }
  styles.sort((a, b) => rank(AE_STYLE_ORDER, a.style, a.seen) - rank(AE_STYLE_ORDER, b.style, b.seen));
  for (const style of styles) {
    style.groups.sort((a, b) => rank(AE_GROUP_ORDER, a.group, a.seen) - rank(AE_GROUP_ORDER, b.group, b.seen));
    delete style.seen;
    for (const group of style.groups) delete group.seen;
  }
  return styles;
}

// a base gametype's engine: the server's word for it, else ENGINES's
function engineOf(base, builtin) {
  const named = (Array.isArray(builtin) ? builtin : []).find((item) => item && typeof item === 'object' && item.name === base);
  return (named && named.engine) || ENGINES[String(base || '').toLowerCase()] || null;
}

// the schema's fields for a base gametype (every engine's and its own's);
// table: where a file keeps the key (the schema's, else universal or the
// engine's own)
function gametypeFields(schema, base, builtin) {
  const engine = engineOf(base, builtin);
  return (schema && Array.isArray(schema.fields) ? schema.fields : [])
    .filter((field) => field.engine === 'all' || field.engine === engine)
    .map((field) => ({ ...field, table: field.table || (field.engine === 'all' ? 'universal' : field.engine),
      choices: Array.isArray(field.choices) ? field.choices : null }));
}

// a control's text as a field's value (undefined: the base's)
function fieldValue(field, text) {
  if (text === '' || text === undefined || text === null) return undefined;
  if (field.type === 'boolean') return text === 'true';
  if (field.type === 'integer') return /^-?\d+$/.test(String(text).trim()) ? Number(text) : undefined;
  return String(text);
}

function newGametypeText(base, title) {
  // (a file's top level takes base and name alone: server_toml.c)
  return `base = ${formatTomlValue(base)}\nname = ${formatTomlValue(String(title || base).slice(0, 11))}\n`;
}

// ---------- the console: completion and history

// help's lines: a heading (ending in a colon), then each command indented by
// two spaces, its name, then its description ("  sv_status          ...")
function commandsFromHelp(lines) {
  return [...new Set((lines || []).map(String).filter((line) => !/:\s*$/.test(line))
    .map((line) => (/^ {0,2}([a-z_][a-z0-9_]*)(?=\s|$)/.exec(line) || [])[1]).filter(Boolean))].sort();
}

function commonPrefix(words) {
  let prefix = words[0] || '';
  for (const word of words) while (!word.startsWith(prefix)) prefix = prefix.slice(0, -1);
  return prefix;
}

// a Tab: the line made longer, and what it could become
function complete(line, commands, players) {
  const words = String(line).split(' ');
  let head = '';
  let options = [];
  if (words.length === 1) {
    options = commands.filter((name) => name.startsWith(words[0]));
  } else if (words.length === 2 && PLAYER_COMMANDS.includes(words[0])) {
    head = words[0] + ' ';
    options = (players || []).map((player) => player.name).filter((name) => name.toLowerCase().startsWith(words[1].toLowerCase()))
      .map((name) => (/\s/.test(name) ? `"${name}"` : name));
  } else {
    return { line, options };
  }
  if (options.length === 1) return { line: head + options[0] + ' ', options };
  return { line: options.length ? head + commonPrefix(options) : line, options };
}

function historyStep(history, index, direction) {
  const next = Math.max(0, Math.min(history.length, index + direction));
  return { index: next, text: next >= history.length ? '' : history[next] };
}

// ---------- elements without HTML

// ---------- (0.7.7) the stop's countdown, keys, admins, the whitelist,
// backups, the bans' import (docs/dedicated_server.md: "Restart warnings",
// "API keys", "Admins, access and the whitelist", "Backups")

// a stop or restart the server is waiting for, in words, from the status:
// {kind: 'countdown'|'drain', restart, text, clock (m:ss, a countdown's),
// ends (ms)}; null when none. A countdown's seconds were read at readAt ms
function stopView(status, nowMs = Date.now(), readAt = nowMs) {
  const s = status || {};
  const warning = s.restart_warning;
  if (warning && Number.isFinite(Number(warning.seconds))) {
    const ends = readAt + Number(warning.seconds) * 1000;
    const left = Math.max(0, Math.ceil((ends - nowMs) / 1000));
    const word = warning.restart ? 'Restarting' : 'Stopping';
    return { kind: 'countdown', restart: !!warning.restart, ends, left, clock: formatClock(left),
      text: `${word} in ${formatClock(left)}`, reason: String(warning.reason || '') };
  }
  const drain = s.drain;
  if (drain && drain.after) {
    const word = drain.restart ? 'Restarts' : 'Stops';
    const when = { game: 'when this game ends', empty: 'once nobody plays', now: 'now' }[drain.after] || drain.after;
    return { kind: 'drain', restart: !!drain.restart, text: `${word} ${when}`, reason: '', clock: '', left: null };
  }
  return null;
}

// the body of POST /api/stop for the page's choice: when is 'now', 'seconds'
// (with seconds), 'game' or 'empty'; restart or stop; a notice for the
// players. Throws a sentence for a bad number of seconds
function stopBody(when, seconds, restart, notice) {
  const body = { restart: !!restart };
  const text = String(notice || '').trim();
  if (text) body.notice = text;
  if (when === 'now') body.after = 'now';
  else if (when === 'game' || when === 'empty') body.after = when;
  else {
    const number = Number(seconds);
    if (!Number.isInteger(number) || number < 1 || number > 3600) throw new Error('Give the seconds: a whole number from 1 to 3600.');
    body.seconds = number;
  }
  return body;
}

// what the page's choice says before it is sent: "Restart the server in 60
// seconds?", and what the players will read
function stopQuestion(body) {
  const verb = body.restart ? 'Restart' : 'Stop';
  const when = body.seconds ? `in ${formatDuration(body.seconds)}` : { now: 'now', game: 'when this game ends',
    empty: 'once nobody plays' }[body.after];
  const told = body.seconds ? 'The players read a notice now, then at 60, 30, 10 and 5 seconds left.' :
    body.after === 'now' ? 'The game in progress ends at once, without its scores.' :
      'The server leaves the listings and takes no new players until then.';
  return { title: `${verb} the server ${when}?`, text: told };
}

// the roles a key may be made with: up to the caller's own, never owner
function keyRoles(role) {
  return ['viewer', 'moderator', 'admin'].filter((item) => roleAtLeast(role, item));
}

// a key's or an admin row's end, in words: never, in 12 days, ended
function formatExpiry(expires, expired, nowMs = Date.now()) {
  if (!expires) return 'Never';
  const left = expires - nowMs / 1000;
  if (expired || left <= 0) return 'Ended';
  return left >= 2 * 86400 ? `In ${Math.round(left / 86400)} days` : 'In ' + formatDuration(left);
}

// an allow-list (addresses and a.b.c.d/n ranges, comma separated) checked
// as the server checks it, spaces taken out: the text to send, or throws
function allowText(text) {
  const kept = String(text || '').replace(/\s+/g, '');
  if (!kept) return '';
  const range = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})(?:\/(\d{1,2}))?$/;
  for (const item of kept.split(',')) {
    const match = range.exec(item);
    if (!match || match.slice(1, 5).some((part) => Number(part) > 255) || (match[5] !== undefined && Number(match[5]) > 32)) {
      throw new Error(`${item || 'An empty item'} is not an address or a range (192.168.1.0/24).`);
    }
  }
  return kept;
}

// an admin row (GET /api/access) for people: a login by its name, a network
// by its ranges; {kind: 'login'|'network', title, identity}
function adminView(row) {
  const identity = String((row && row.identity) || '');
  if (identity.startsWith('name:')) return { kind: 'login', title: identity.slice(5), identity };
  if (identity.startsWith('cidr:')) return { kind: 'network', title: identity.slice(5).split(',').join(', '), identity };
  return { kind: 'login', title: identity, identity };
}

// a command's word as the server reads it: quoted when it has a space; a
// quote or a control character refused (the server refuses them too)
function commandWord(text) {
  const word = String(text ?? '');
  // eslint-disable-next-line no-control-regex
  if (!word || /["\u0000-\u001f\u007f]/.test(word)) throw new Error('That has a character the server refuses (a quote or a control character).');
  return /\s/.test(word) ? `"${word}"` : word;
}

// sv_admin_add's line for the page's form: kind 'login' (a name) or
// 'network' (ranges); role, immunity 0-99, days 0-3650, an allow-list (a
// login's). Throws a sentence for what is wrong
function adminAddLine(form) {
  const kind = form.kind === 'network' ? 'network' : 'login';
  let identity;
  if (kind === 'login') {
    const name = String(form.name || '').trim();
    if (!/^[a-z0-9_-]{1,32}$/.test(name)) throw new Error('A login is 1 to 32 of a-z, 0-9, _ and -.');
    identity = 'name:' + name;
  } else {
    const ranges = allowText(form.name);
    if (!ranges) throw new Error('Give the network: an address or a range (192.168.1.0/24).');
    identity = 'cidr:' + ranges;
  }
  if (!['viewer', 'moderator', 'admin'].includes(form.role)) throw new Error('Pick a role.');
  const immunity = Number(form.immunity || 0);
  if (!Number.isInteger(immunity) || immunity < 0 || immunity > 99) throw new Error('Immunity is a whole number from 0 to 99.');
  const days = Number(form.days || 0);
  if (!Number.isInteger(days) || days < 0 || days > 3650) throw new Error('Days is 0 (never) or 1 to 3650.');
  const allow = kind === 'login' ? allowText(form.allow) : '';
  return ['sv_admin_add', identity, form.role, String(immunity), String(days), allow].filter(Boolean).join(' ');
}

// the secret sv_admin_add shows once: its 40 hex characters' line
function secretFromLines(lines) {
  return [].concat(lines || []).map((line) => String(line).trim()).find((line) => /^[0-9a-f]{40}$/.test(line)) || '';
}

// a whitelist.txt entry for people: {kind (its words), value, entry}
const WHITELIST_KINDS = { ip: 'Address', hwid: 'Hardware id', discord: 'Discord account', name: 'Player name' };
function whitelistView(entry) {
  const text = String(entry || '');
  const colon = text.indexOf(':');
  const kind = colon > 0 ? text.slice(0, colon) : '';
  return { kind: WHITELIST_KINDS[kind] || humanize(kind) || 'Entry', value: colon > 0 ? text.slice(colon + 1) : text,
    entry: text, mono: kind === 'ip' || kind === 'hwid' || kind === 'discord' };
}

// a size in bytes for reading: 412 B, 17.8 KB, 2.4 MB
function formatBytes(size) {
  const number = Number(size);
  if (!Number.isFinite(number) || number < 0) return '–';
  if (number < 1024) return `${number} B`;
  if (number < 1024 * 1024) return `${parseFloat((number / 1024).toFixed(number < 10240 ? 1 : 0))} KB`;
  return `${parseFloat((number / 1048576).toFixed(1))} MB`;
}

// GET /api/backups's list by file, in the order the server backs them up,
// each file's newest first: [{file, backups}]
function backupGroups(backups) {
  const groups = new Map();
  for (const backup of [].concat(backups || [])) {
    if (!backup || !backup.file) continue;
    if (!groups.has(backup.file)) groups.set(backup.file, []);
    groups.get(backup.file).push(backup);
  }
  return [...groups].map(([file, list]) => ({ file, backups: list.slice().sort((a, b) => (b.time || 0) - (a.time || 0)) }));
}

// a ban of an export (GET /api/bans/export's shape): what it bans, as the
// server matches a duplicate
const banKey = (ban) => [ban.ip || '', String(ban.hwid || '').toLowerCase(), ban.discord_id || '', ban.name_pattern || ''].join('|');

// a file's bans before they are imported: those new, those already here,
// those over, and those the server would refuse (its checks, roughly: it
// checks every field again). text is the file's; existing the export's
function importPreview(text, existing, nowMs = Date.now()) {
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    throw new Error('The file is not JSON: an export of this page or of sv_bans_export is.');
  }
  const bans = Array.isArray(data) ? data : data && Array.isArray(data.bans) ? data.bans : null;
  if (!bans) throw new Error('The file has no list of bans ({"bans": [...]}).');
  const have = new Set([].concat(existing || []).map(banKey));
  const result = { bans, fresh: [], have: [], over: [], wrong: [] };
  const range = /^(\d{1,3}\.){3}\d{1,3}(\/\d{1,2})?$/;
  for (const ban of bans) {
    const fields = ban && typeof ban === 'object' ? ban : {};
    const good = (fields.ip || fields.hwid || fields.discord_id || fields.name_pattern) &&
      (!fields.ip || range.test(fields.ip)) && (!fields.hwid || /^[0-9a-fA-F]{1,32}$/.test(fields.hwid)) &&
      (!fields.discord_id || /^\d{1,20}$/.test(fields.discord_id)) &&
      (fields.until === null || fields.until === undefined || Number.isInteger(fields.until));
    if (!good) result.wrong.push(fields);
    else if (have.has(banKey(fields))) result.have.push(fields);
    else if (Number.isInteger(fields.until) && fields.until * 1000 <= nowMs) result.over.push(fields);
    else {
      have.add(banKey(fields));
      result.fresh.push(fields);
    }
  }
  return result;
}

// a ban of an import in one line: who, what, until
function banLine(ban, nowMs = Date.now()) {
  const what = ban.name_pattern ? `names like ${ban.name_pattern}` : ban.ip || (ban.hwid ? `hardware ${ban.hwid}` :
    ban.discord_id ? `Discord ${ban.discord_id}` : '');
  const line = [ban.players, what, formatUntil(ban.until, nowMs).toLowerCase()].filter(Boolean).join(', ');
  return line.charAt(0).toUpperCase() + line.slice(1);
}

let doc = typeof document !== 'undefined' ? document : null;
const setDocument = (value) => { doc = value; };
const PROPERTIES = new Set(['value', 'checked', 'disabled', 'type', 'title', 'hidden', 'id', 'name', 'placeholder', 'min',
  'max', 'step', 'readOnly', 'selected', 'htmlFor', 'maxLength', 'required', 'autocomplete']);
const FORBIDDEN = /^on|^(style|srcdoc)$|HTML$/i;

// h('button', {class, text, data: {slot: 3}, attrs: {colspan: 2}, listen: {click}}, children):
// text is textContent, a string child a text node; a style or a handler
// attribute is refused
function h(tag, props, children) {
  const node = doc.createElement(tag);
  for (const [key, value] of Object.entries(props || {})) {
    if (FORBIDDEN.test(key)) throw new Error(`h: ${key} is not allowed`);
    if (value === undefined || value === null || (value === false && key !== 'checked')) continue;
    if (key === 'class') node.className = value;
    else if (key === 'text') node.textContent = String(value);
    else if (key === 'listen') for (const [event, handler] of Object.entries(value)) node.addEventListener(event, handler);
    else if (key === 'data' || key === 'attrs') {
      for (const [name, item] of Object.entries(value)) {
        if (FORBIDDEN.test(name)) throw new Error(`h: ${name} is not allowed`);
        node.setAttribute((key === 'data' ? 'data-' : '') + name, String(item));
      }
    } else if (PROPERTIES.has(key)) node[key] = value;
    else throw new Error(`h: unknown property ${key}`);
  }
  for (const child of [].concat(children === undefined ? [] : children)) {
    if (child !== null && child !== undefined && child !== false) {
      node.appendChild(typeof child === 'object' ? child : doc.createTextNode(String(child)));
    }
  }
  return node;
}

const option = (value, text, selected, disabled) => h('option', { value, text, selected, disabled });

// ===================================================================== the page

function startPage(root) {
  const $ = (id) => doc.getElementById(id);
  const api = createApi(root.fetch.bind(root));
  const page = { role: null, status: null, players: [], maps: [], gametypes: null, rotation: null, dirty: false,
    settings: [], log: { cursor: 0, lines: [] }, seq: 0, history: [], historyIndex: 0, commands: [], samples: [], feed: [],
    dossiers: new Set(), editor: null, timers: [] };
  const listen = (id, event, handler) => $(id).addEventListener(event, handler);

  function toast(text, bad) {
    const box = $('toast');
    box.textContent = text;
    box.className = 'toast' + (bad ? ' bad' : '');
    box.hidden = false;
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => { box.hidden = true; }, bad ? 6000 : 3500);
  }

  // a call; a refused session shows the sign-in again
  async function call(method, path, body) {
    const result = await api.call(method, path, body);
    if (result.status === 401) signedOut('The session ended: sign in again.');
    reach(result);
    return result;
  }

  // the server's answers, said: no answer (or a proxy's 502/504) and "too many requests" show a line under the tabs
  // with when the page last heard from the server, until an answer comes again
  function reach(result) {
    const down = result.status === 0 || result.status === 502 || result.status === 504;
    const slow = result.status === 429;
    if (!down && !slow) {
      if (result.status) page.heardAt = Date.now();
      if (page.trouble) { page.trouble = null; renderReach(); }
      return;
    }
    if (!page.trouble || page.trouble.kind !== (down ? 'down' : 'slow')) page.trouble = { kind: down ? 'down' : 'slow', since: Date.now() };
    page.trouble.retry = result.error && result.error.retry_after;
    renderReach();
  }

  function renderReach() {
    const banner = $('conn-banner');
    const trouble = page.trouble;
    banner.hidden = !trouble || !page.role;
    if (!trouble) return;
    const heard = page.heardAt ? new Date(page.heardAt).toLocaleTimeString() : null;
    banner.className = 'conn-banner conn-' + trouble.kind;
    banner.textContent = trouble.kind === 'down' ?
      `The server isn't answering. ${heard ? `What you see is from ${heard}; ` : ''}the page keeps trying.` :
      `The server asks the page to slow down (too many requests)${trouble.retry ? `: it tries again in ${trouble.retry} s` : ''}. ${heard ? `What you see is from ${heard}.` : ''}`;
  }

  // what the role cannot do: disabled, and saying so
  function gate() {
    for (const node of doc.querySelectorAll('[data-needs]')) {
      const needed = node.getAttribute('data-needs');
      node.disabled = !roleAtLeast(page.role, needed);
      node.title = node.disabled ? `Needs the ${needed} role` : '';
      // (a tab the role can't open is not shown at all)
      if (node.hasAttribute('data-tab')) node.hidden = node.disabled;
    }
    // (a form or a block of controls the role can't use is not shown; a line says who can)
    for (const node of doc.querySelectorAll('[data-needs-form], [data-needs-block]')) {
      node.hidden = !roleAtLeast(page.role, node.getAttribute('data-needs-form') || node.getAttribute('data-needs-block'));
    }
    for (const node of doc.querySelectorAll('[data-below]')) node.hidden = roleAtLeast(page.role, node.getAttribute('data-below'));
  }

  // a select with nothing to pick yet shows why, in a disabled first option, never an empty box
  function fillSelect(select, options, emptyText) {
    if (options.length) {
      select.replaceChildren(...options);
      select.classList.remove('waiting');
    } else {
      select.replaceChildren(option('', emptyText, true, true));
      select.classList.add('waiting');
    }
  }

  // (a title in the title face, unless digits or a question would read as letters in it)
  const titleText = (node, text) => {
    node.textContent = text;
    node.classList.toggle('face-plain', /[0-9?]/.test(text));
  };

  // a label and its value (text, or nodes), a muted note under the value;
  // no value shows a muted dash
  const fact = (label, value, kind, note) => {
    const empty = value === '' || value === null || value === undefined || value === '–';
    return h('div', { class: kind }, [h('dt', { text: label }),
      h('dd', { class: empty ? 'empty' : null }, [].concat(empty ? '–' : keepUnits(value), note ? h('span', { class: 'fact-note', text: note }) : []))]);
  };
  // (a number keeps its unit on its line: "9 min", "43 ms", "2302/udp")
  const keepUnits = (value) => (typeof value === 'string' ? value.replace(/(\d) (?=[a-z%°×]|min\b)/gi, '$1\u00a0') : value);
  // a map's name and where it comes from (the word for an admin, in the title)
  const mapNode = (view) => h('span', { class: 'map-name', title: roleAtLeast(page.role, 'admin') ? view.word : null }, [view.title,
    view.tag && h('span', { class: 'tag', text: view.tag, title: view.pack && !/^maps_/.test(view.pack) ? view.pack : null })]);
  // a rotation entry for people: the map, the gametype, the rules as chips;
  // the entry itself in a small line for an admin (raw: true)
  const entryNode = (entry, raw) => {
    const view = entryView(entry, page.maps, page.gametypes);
    return h('span', { class: 'entry-view', title: !raw && roleAtLeast(page.role, 'admin') ? view.text : null }, [
      h('span', { class: 'entry-main' }, [mapNode(view.map), view.gametype && h('span', { class: 'entry-gametype', text: view.gametype })]),
      view.rules.length > 0 && h('span', { class: 'rule-chips' }, view.rules.map((rule) => h('span', { class: 'rule', text: rule }))),
      raw && roleAtLeast(page.role, 'admin') && h('code', { class: 'entry-raw', text: view.text })]);
  };
  const entryWords = (entry) => entryLabel(entryView(entry, page.maps, page.gametypes));
  // a long word that may break only at its dots, colons, pluses and slashes
  const breakable = (text) => breakParts(text).flatMap((part, index, all) => (index < all.length - 1 ? [part, h('wbr')] : [part]));
  const cell = (label, content, kind) => h('td', { data: { label }, class: kind }, keepUnits(content));
  const small = (text, enabled, action, kind, title) => h('button', { class: 'small' + (kind ? ' ' + kind : ''), text, title,
    disabled: !enabled, listen: { click: action } });
  const para = (text) => h('p', { text });

  // ---------- a question before an action, whose answer it shows

  // (options.only: one button, no Cancel: a notice to read, as a key's secret)
  function ask(title, body, okText, action, danger, options = {}) {
    $('dialog-title').textContent = title;
    $('dialog-body').replaceChildren(...[].concat(body));
    $('dialog-error').textContent = '';
    $('dialog-ok').textContent = okText;
    $('dialog-ok').className = danger ? 'danger' : 'primary';
    $('dialog-cancel').hidden = !!options.only;
    $('dialog').hidden = false;
    ($('dialog-body').querySelector('input, select') || $('dialog-ok')).focus();
    ask.action = action;
  }

  function closeDialog() {
    $('dialog').hidden = true;
    ask.action = null;
  }

  listen('dialog-cancel', 'click', closeDialog);
  listen('dialog', 'keydown', (event) => { if (event.key === 'Escape') closeDialog(); });
  listen('dialog-form', 'submit', async (event) => {
    event.preventDefault();
    if (!ask.action) return;
    $('dialog-ok').disabled = true;
    const result = await ask.action();
    $('dialog-ok').disabled = false;
    if (result && !result.ok) {
      $('dialog-error').textContent = errorText(result);
      return;
    }
    closeDialog();
    const lines = result && result.data && result.data.lines;
    if (lines && lines.length) toast(lines.join(' · ').slice(0, 300));
  });

  // ---------- the status

  function renderStatus() {
    const s = page.status;
    const listing = s.listing || {};
    const next = s.rotation && s.rotation.next;
    const admin = roleAtLeast(page.role, 'admin');
    const masters = s.masters || [];
    titleText($('server-name'), s.name);
    doc.title = `${s.name}: Arena Evolved server`;
    $('server-state').textContent = stateLabel(s.state);
    $('server-state').className = 'badge state-' + s.state;
    $('status-facts').replaceChildren(...[
      fact('Map', s.map ? mapNode(mapView(s.map, page.maps)) : ''),
      fact('Gametype', gametypeText(s, page.gametypes)),
      fact('State', stateLabel(s.state) + (s.countdown >= 0 ? `: starts in ${s.countdown} s` : '')),
      /^(playing|postgame)$/.test(s.state) && fact('Game time', formatClock(s.game_seconds)),
      s.teams && (s.team_scores || []).length === 2 && fact('Score', h('span', { class: 'scores' }, [0, 1].map((team) =>
        h('span', { class: `score score-${team}` }, [h('span', { class: 'team team-' + team }), teamName(team), ' ',
          h('strong', { text: String(s.team_scores[team]) })])))),
      fact('Next', next ? entryNode(next) : 'None'),
    ].filter(Boolean));
    $('status-server').replaceChildren(...[
      fact('Listing', `${listing.online ? 'Online' : 'Offline'}, ${listing.public ? 'public' : 'private'}${listing.password ? ', with a password' : ''}`),
      fact('Machines', String(s.machines ?? '')),
      fact('Uptime', formatDuration(s.uptime)),
      fact('Games hosted', String(s.games_hosted ?? '')),
      fact('Build', s.build ? breakable(s.build) : ''),
      fact('Network version', String(s.network_version ?? '')),
      fact('Frame time', s.frame_ms_max === undefined ? '' : `${formatMs(s.frame_ms_max)} at most`),
      fact('Tick time', s.tick_ms_avg === undefined ? '' : `${formatMs(s.tick_ms_avg)} average`),
      s.update && s.update.available && fact('Update', `${s.update.build} is out`, 'notice'),
    ].filter(Boolean));
    $('status-count').textContent = String(s.players);
    $('status-max').textContent = String(s.max_players);
    const invite = admin ? listing.invite || '' : '';
    $('invite').value = invite;
    $('invite-copy').disabled = !invite;
    $('invite-row').hidden = !invite;
    $('invite-none').hidden = !!invite;
    $('invite-note').hidden = !invite;
    $('invite-none').textContent = !admin ? 'Admins see the invite link here.' :
      'No invite yet: the server makes one once it is online and listed.';
    // (each master its host, then how it stands and since when, under it)
    $('masters').replaceChildren(...masters.map((master) => {
      const view = masterView(master);
      const when = formatAgo(master.updated);
      return h('li', { class: 'master-' + view.kind }, [
        h('span', { class: 'master-host', title: view.url }, breakable(view.host)),
        h('span', { class: 'master-line' }, [h('span', { class: 'master-state', text: view.text }),
          when !== '–' && h('span', { class: 'master-when', text: when })]),
      ]);
    }));
    $('masters').hidden = !masters.length;
    $('masters-empty').hidden = masters.length > 0;
    page.statusAt = Date.now();
    renderStop();
    renderAbout();
  }

  // ---------- stop or restart: a countdown, a drain, the owner's choice

  function renderStop() {
    const view = page.status ? stopView(page.status, Date.now(), page.statusAt) : null;
    const owner = roleAtLeast(page.role, 'owner');
    $('stop-live').hidden = !view;
    $('stop-badge').hidden = !view;
    $('stop-form').hidden = !owner || !!view;
    $('stop-owner').hidden = owner;
    if (!view) return;
    $('stop-badge').textContent = view.kind === 'countdown' ? `${view.restart ? 'Restart' : 'Stop'} ${view.clock}` :
      view.restart ? 'Restart waiting' : 'Stop waiting';
    $('stop-what').textContent = view.kind === 'countdown' ? (view.restart ? 'Restarting in' : 'Stopping in') : view.text;
    $('stop-clock').textContent = view.kind === 'countdown' ? view.clock : '';
    $('stop-reason').textContent = view.reason ? `The players read: ${view.reason}` :
      view.kind === 'drain' ? 'Not listed meanwhile, and no new players.' : '';
    $('stop-cancel').hidden = !owner;
  }

  listen('stop-form', 'submit', (event) => {
    event.preventDefault();
    const restart = !event.submitter || event.submitter.value !== 'stop';
    const choice = (doc.querySelector('input[name="stop-when"]:checked') || {}).value || '60';
    let body;
    try {
      body = choice === '60' ? stopBody('seconds', 60, restart, $('stop-notice').value) :
        stopBody(choice, $('stop-seconds').value, restart, $('stop-notice').value);
    } catch (error) {
      toast(error.message, true);
      return;
    }
    const question = stopQuestion(body);
    ask(question.title, para(question.text), restart ? 'Restart' : 'Stop', async () => {
      const result = await call('POST', '/api/stop', body);
      if (!result.ok) return result;
      $('stop-notice').value = '';
      refreshStatus();
      // (in words, not the command's own line)
      toast(body.seconds ? `The server ${restart ? 'restarts' : 'stops'} in ${formatDuration(body.seconds)}. Cancel stops the countdown.` :
        body.after === 'now' ? `The server ${restart ? 'restarts' : 'stops'} now.` :
          `The server ${restart ? 'restarts' : 'stops'} ${body.after === 'game' ? 'when this game ends' : 'once nobody plays'}.`);
      return { ok: true };
    }, !restart || body.after === 'now');
  });
  listen('stop-seconds', 'focus', () => { doc.querySelector('input[name="stop-when"][value="seconds"]').checked = true; });
  listen('stop-cancel', 'click', async () => {
    const result = await call('POST', '/api/stop', { cancel: true });
    toast(result.ok ? 'Cancelled: the server goes on.' : errorText(result), !result.ok);
    refreshStatus();
  });

  // (the status now, not at its next push: a change the page made shows at once)
  async function refreshStatus() {
    const status = await call('GET', '/api/status');
    if (status.ok) takeStatus(status.data);
  }

  function sample(players) {
    pushSample(page.samples, Date.now(), players);
    const max = Math.min(page.status.max_players || 0, Math.max(4, ...page.samples.map((item) => item.value)));
    const points = sparkPoints(page.samples, Date.now(), 300, 64, max);
    $('spark-line').setAttribute('points', points);
    $('spark-fill').setAttribute('points', points ? `${points.split(',')[0]},64 ${points} 300,64` : '');
    // (the graph once it has something to show: a player seen, a minute
    // watched; before that, a line that says so)
    const anyone = page.samples.some((item) => item.value > 0);
    const watched = page.samples.length > 1 && page.samples[page.samples.length - 1].time - page.samples[0].time >= 60000;
    $('spark').toggleAttribute('hidden', !(anyone && watched));
    $('spark-note').textContent = anyone && watched ? 'Players over the last 10 minutes, as this page saw them.' :
      anyone ? 'The graph of the last 10 minutes starts after a minute on this page.' : 'No one has played since this page opened.';
  }

  function takeStatus(status) {
    page.status = status;
    renderStatus();
    sample(status.players);
  }

  function addFeed(event) {
    const said = describeEvent(event, page.players, { map: (word) => mapView(word, page.maps).title,
      gametype: (word) => gametypeTitle(word, page.gametypes) });
    if (!said || said.quiet) return;
    page.feed.unshift({ time: event.time, ...said });
    page.feed.length = Math.min(page.feed.length, 40);
    $('feed').replaceChildren(...page.feed.map((item) => h('li', { class: 'feed-' + item.kind }, [
      h('time', { text: formatTime(item.time) }), ' ', item.text])));
  }

  // ---------- players: each action names the slot and the player

  function playerAction(player, action) {
    const who = `slot ${player.slot}, ${player.name}`;
    const send = (body) => call('POST', `/api/players/${player.slot}/${action}`, { name: player.name, ...body });
    if (action === 'kick') {
      ask(`Kick ${who}?`, para('Their machine is dropped; they may join again.'), 'Kick', () => send(), true);
    } else if (action === 'ban') {
      const duration = h('select', {}, banPresets(page.role).map((preset, index) =>
        option(preset.value, preset.label + (preset.allowed ? '' : ' (admins)'), index === 1, !preset.allowed)));
      const reason = h('input', { maxLength: 127, placeholder: 'Why (kept in bans.txt)' });
      ask(`Ban ${who}?`, [para('Their machine is dropped, and its address and hardware id are banned.'),
        h('label', {}, ['For', duration]), h('label', {}, ['Reason', reason])], 'Ban',
      () => send({ duration: duration.value, reason: reason.value.trim() }), true);
    } else if (action === 'mute') {
      const minutes = h('select', {}, [option('10', '10 minutes', true), option('30', '30 minutes'), option('60', '1 hour'),
        option('0', 'Until unmuted')]);
      ask(`Mute ${who}?`, [para('Their chat and voice, for everyone.'), h('label', {}, ['For', minutes])], 'Mute',
        () => send(Number(minutes.value) ? { minutes: Number(minutes.value) } : {}));
    } else if (action === 'unmute') {
      ask(`Unmute ${who}?`, para('Their chat and voice reach everyone again.'), 'Unmute', () => send());
    } else {
      const text = h('input', { maxLength: 127, required: true, placeholder: 'A notice on their screen' });
      ask(`Tell ${who}`, h('label', {}, ['Notice', text]), 'Send', () => send({ text: text.value }));
    }
  }

  function renderPlayers() {
    const moderator = roleAtLeast(page.role, 'moderator');
    const hidden = 'Only for the owner, or with server.log_addresses';
    const rows = [];
    for (const player of page.players) {
      const key = `${player.slot}:${player.name}`;
      const act = (action, label, kind) => small(label, moderator, () => playerAction(player, action), kind,
        moderator ? `${label} slot ${player.slot}, ${player.name}` : 'Needs the moderator role');
      rows.push(h('tr', { class: player.muted ? 'muted' : null }, [
        cell('Slot', String(player.slot), 'num'),
        cell('Name', h('span', {}, [h('span', { class: 'team team-' + player.team }), player.name,
          player.muted && h('span', { class: 'badge', text: 'muted' })])),
        cell('Team', teamName(player.team)), cell('Score', String(player.score), 'num'),
        cell('K/D/A', `${player.kills}/${player.deaths}/${player.assists}`, 'num'),
        cell('Ping', Number.isFinite(player.ping) ? `${player.ping} ms` : '–', 'num'),
        cell('Played', formatDuration(Date.now() / 1000 - player.joined), 'num'),
        cell('Actions', h('span', { class: 'actions' }, [
          small(page.dossiers.has(key) ? 'Less' : 'Dossier', true, () => {
            if (!page.dossiers.delete(key)) page.dossiers.add(key);
            renderPlayers();
          }),
          // (what a viewer can't do is not offered: the Dossier only)
          ...(moderator ? [act('kick', 'Kick', 'danger'), act('ban', 'Ban', 'danger'),
            player.muted ? act('unmute', 'Unmute') : act('mute', 'Mute'), act('tell', 'Tell')] : []),
        ]), 'row-actions'),
      ]));
      if (page.dossiers.has(key)) {
        rows.push(h('tr', { class: 'dossier' }, h('td', { attrs: { colspan: 8 } }, h('dl', { class: 'facts' }, [
          fact('Machine', String(player.machine)), fact('Joined', new Date(player.joined * 1000).toLocaleString()),
          fact('Hardware id', player.hwid || hidden),
          fact('Address', player.address || hidden)]))));
      }
    }
    $('players').querySelector('tbody').replaceChildren(...rows);
    $('players').hidden = !page.players.length;
    $('players-empty').hidden = page.players.length > 0;
  }

  // a list that didn't load says so in its empty state (and shows no stale rows), never "nothing here"; loaded,
  // the empty state says its own sentence again
  function listLoaded(listId, emptyId, result) {
    const empty = $(emptyId);
    if (!empty.dataset.text) empty.dataset.text = empty.textContent;
    if (result.ok) {
      empty.textContent = empty.dataset.text;
      empty.classList.remove('failed');
      return true;
    }
    if (result.status === 401) return false;
    empty.textContent = `This list didn't load. ${errorText(result).replace(/([^.!?])$/, '$1.')} Open the tab again to retry.`;
    empty.classList.add('failed');
    empty.hidden = false;
    if (listId) $(listId).hidden = true;
    return false;
  }

  async function loadPlayers() {
    const result = await call('GET', '/api/players');
    if (!listLoaded('players', 'players-empty', result)) return;
    page.players = result.data.players || [];
    renderPlayers();
  }

  // ---------- the rotation: edited here, saved whole

  async function loadRotation(force) {
    if (page.dirty && !force) return;
    const result = await call('GET', '/api/rotation');
    if (!result.ok) return;
    page.rotation = result.data;
    page.dirty = false;
    renderRotation();
  }

  function editRotation(entries) {
    page.rotation.entries = entries;
    page.dirty = true;
    renderRotation();
  }

  function renderRotation() {
    const rotation = page.rotation;
    const admin = roleAtLeast(page.role, 'admin');
    const moderator = roleAtLeast(page.role, 'moderator');
    const entries = rotation.entries;
    $('rotation').replaceChildren(...entries.map((entry, index) => {
      const current = !page.dirty && index === rotation.index;
      const play = () => ask(`Play ${entryWords(entry)} now?`, para('The game in progress ends without its scores; the rotation goes on after this one.'),
        'Play now', () => call('POST', '/api/rotation/play', playBody(entry)));
      // (the current tag beside the entry's name, not after its longest line)
      const view = entryNode(entry, true);
      if (current) view.querySelector('.entry-main').append(h('span', { class: 'badge current-tag', text: 'current' }));
      return h('li', { class: (entry.rejected ? 'rejected' : '') + (current ? ' current' : '') }, h('div', { class: 'entry-row' }, [
        h('span', { class: 'entry' }, [view, entry.rejected && h('span', { class: 'reason', text: entry.rejected })]),
        // (only what the role can do: an admin edits, a moderator plays an entry now, a viewer reads)
        (admin || moderator) && h('span', { class: 'actions' }, [
          admin && small('Up', index > 0, () => editRotation(rotationMove(entries, index, -1))),
          admin && small('Down', index < entries.length - 1, () => editRotation(rotationMove(entries, index, 1))),
          admin && small('Remove', true, () => editRotation(rotationRemove(entries, index)), 'danger'),
          small('Play now', !entry.rejected, play, null, entry.rejected ? 'This entry is rejected' : 'The game in progress ends'),
        ]),
      ]));
    }));
    $('rotation-note').textContent = page.dirty ? 'Changed: Save writes it to server.rotation.' : rotation.next ? `Next: ${entryWords(rotation.next)}` : '';
    $('rotation-save').disabled = !admin || !page.dirty;
    $('rotation-revert').disabled = !page.dirty;
  }

  const waitingGametypes = () => (page.gametypesFailed ? "The gametypes didn't load: open this tab again" : 'Loading the gametypes…');
  // (Add and New only with something picked)
  function pickersReady() {
    const admin = roleAtLeast(page.role, 'admin');
    $('rotation-add').querySelector('button').disabled = !admin || !$('add-map').value || !$('add-gametype').value;
    $('gametype-new').querySelector('button').disabled = !admin || !$('new-base').value;
  }

  // the maps, once at the start and again if that failed
  async function loadMaps() {
    const maps = await call('GET', '/api/maps');
    page.mapsFailed = !maps.ok;
    if (maps.ok) page.maps = (Array.isArray(maps.data.maps) ? maps.data.maps : []).filter((map) => map && map.name);
    renderPickers();
  }

  function renderPickers() {
    const gametypes = page.gametypes || { builtin: [], ae_builtin: [], files: [] };
    const builtin = builtinNames(gametypes.builtin);
    // (each map by its name and where it comes from; the word is the value)
    const mapLabel = (map) => {
      const view = mapView(mapToken(map), page.maps);
      return view.title + (view.tag ? ` · ${view.tag}` : map.kind === 'custom' ? ' · CE' : '');
    };
    fillSelect($('add-map'), page.maps.slice().map((map) => [mapLabel(map), map]).sort((a, b) => a[0].localeCompare(b[0]))
      .map(([label, map]) => option(mapToken(map), label)), page.mapsFailed ? "The maps didn't load: open this tab again" : 'Loading the maps…');
    // (Arena Evolved's first, a folder a style and group: the rotation word is
    // the value, the menus' name the label)
    const aeGroups = [];
    for (const style of aeBuiltinFolders(gametypes.ae_builtin)) {
      for (const group of style.groups) {
        aeGroups.push(h('optgroup', { attrs: { label: group.group ? `${style.style} · ${group.group}` : style.style } },
          group.entries.map((entry) => option(entry.name, entry.display))));
      }
    }
    const fileOptions = (gametypes.files || []).map((file) =>
      option(file.name, `${file.description || titleCase(file.name)} · ${gametypeTitle(file.base)}`));
    fillSelect($('add-gametype'), builtin.length || aeGroups.length ? [...aeGroups,
      h('optgroup', { attrs: { label: aeGroups.length ? 'Stock' : 'Built-in' } }, builtin.map((name) => option(name, gametypeTitle(name)))),
      fileOptions.length && h('optgroup', { attrs: { label: 'Files' } }, fileOptions)].filter(Boolean) : [], waitingGametypes());
    fillSelect($('new-base'), builtin.map((name) => option(name, gametypeTitle(name))), waitingGametypes());
    pickersReady();
  }

  listen('rotation-add', 'submit', (event) => {
    event.preventDefault();
    try {
      editRotation(rotationInsert(page.rotation.entries, makeEntry($('add-map').value, $('add-gametype').value, $('add-rules').value)));
      $('add-rules').value = '';
    } catch (error) {
      toast(error.message, true);
    }
  });
  listen('rotation-save', 'click', async () => {
    const result = await call('PUT', '/api/rotation', { entries: page.rotation.entries.map((entry) => entry.text) });
    if (result.ok) {
      page.rotation = result.data;
      page.dirty = false;
      toast('The rotation is saved.');
    } else {
      if (result.error.code === 'rejected') page.rotation.entries = markRejected(page.rotation.entries, result.error.rejected || result.data.rejected);
      toast(errorText(result), true);
    }
    renderRotation();
  });
  listen('rotation-revert', 'click', () => loadRotation(true));

  // ---------- gametypes: a form from the schema beside the file's text

  async function loadGametypes() {
    const result = await call('GET', '/api/gametypes');
    page.gametypesFailed = !result.ok;
    if (!listLoaded('gametype-files', 'gametype-files-empty', result)) {
      $('gametype-builtins-head').hidden = !$('gametype-builtins').children.length;
      renderPickers();
      return;
    }
    $('gametype-builtins-head').hidden = false;
    page.gametypes = { ...result.data, builtin: Array.isArray(result.data.builtin) ? result.data.builtin : [],
      ae_builtin: Array.isArray(result.data.ae_builtin) ? result.data.ae_builtin : [],
      files: Array.isArray(result.data.files) ? result.data.files : [] };
    const admin = roleAtLeast(page.role, 'admin');
    $('gametype-files').replaceChildren(...page.gametypes.files.map((file) => h('li', {}, [
      h('span', { class: 'gt-file' }, [h('strong', { text: file.description || titleCase(file.name) }),
        h('span', { class: 'gt-file-meta' }, [h('code', { text: `${file.name}.toml` }), h('span', { text: `based on ${gametypeTitle(file.base)}` })]),
        file.error && h('span', { class: 'reason', text: file.error })]),
      small(admin ? 'Edit' : 'View', true, () => openGametype(file.name))])));
    $('gametype-files').hidden = !page.gametypes.files.length;
    $('gametype-files-empty').hidden = page.gametypes.files.length > 0;
    $('gametype-builtins').replaceChildren(...builtinNames(page.gametypes.builtin).map((name) => h('li', {}, [
      h('span', { text: gametypeTitle(name) }), h('code', { text: name })])));
    // (Arena Evolved's: a folder a style, opened by its summary; in each, a
    // folder a group, each gametype's name with the word a rotation names it by)
    const folders = aeBuiltinFolders(page.gametypes.ae_builtin);
    $('gametype-ae-block').hidden = !folders.length;
    $('gametype-ae').replaceChildren(...folders.map((style) => h('details', { class: 'ae-folder' }, [
      h('summary', {}, [h('span', { text: style.style }), h('span', { class: 'ae-count', text: String(style.count) })]),
      ...style.groups.map((group) => {
        const names = h('ul', { class: 'ae-names' }, group.entries.map((entry) => h('li', {}, [
          h('span', { text: entry.display }), h('code', { text: entry.name })])));
        // (a style without groups shows its names; a group is a folder in it)
        return group.group ? h('details', { class: 'ae-folder ae-subfolder' }, [
          h('summary', {}, [h('span', { text: group.group }), h('span', { class: 'ae-count', text: String(group.entries.length) })]),
          names]) : h('div', { class: 'ae-loose' }, names);
      }),
    ])));
    renderPickers();
    // (the status and the rotation name the gametypes as the game does, now known)
    if (page.status) renderStatus();
    if (page.rotation) renderRotation();
  }

  async function openGametype(name) {
    const result = await call('GET', `/api/gametypes/${encodeURIComponent(name)}`);
    if (result.ok) showEditor(name, result.data.text, false);
    else toast(errorText(result), true);
  }

  function showEditor(name, text, isNew) {
    page.editor = { name, isNew };
    $('gametype-editor').hidden = false;
    titleText($('editor-title'), (isNew ? 'New gametype: ' : 'Gametype: ') + gametypeTitle(name, page.gametypes));
    $('editor-file').textContent = name;
    $('gametype-text').value = text;
    $('gametype-text').readOnly = !roleAtLeast(page.role, 'admin');
    $('gametype-delete').hidden = isNew;
    renderGametypeForm();
    $('gametype-editor').scrollIntoView({ block: 'start' });
  }

  function showTomlErrors() {
    const parsed = parseToml($('gametype-text').value);
    $('gametype-error').textContent = parsed.errors.map((error) => `Line ${error.line}: ${error.message}`).join('; ');
    return parsed;
  }

  // a control's change goes into the text, the rest of the text as it was
  function renderGametypeForm() {
    const parsed = parseToml($('gametype-text').value);
    const top = parsed.values[''];
    const disabled = !roleAtLeast(page.role, 'admin');
    const change = (table, key, value) => {
      $('gametype-text').value = setTomlValue($('gametype-text').value, table, key, value);
      if (key === 'base') renderGametypeForm();
      else showTomlErrors();
    };
    const control = (tag, props, children, table, key, field) => h(tag, { ...props, disabled, attrs: { 'data-table': table, 'data-key': key },
      listen: { change: (event) => change(table, key, field ? fieldValue(field, event.target.value) : event.target.value || undefined) } }, children);
    const groups = [h('fieldset', {}, [h('legend', { text: 'The file' }),
      h('label', {}, ['Base', control('select', {}, builtinNames(page.gametypes.builtin).map((name) => option(name, gametypeTitle(name), name === top.base)), '', 'base')]),
      h('label', {}, ['Name', control('input', { value: top.name || '', maxLength: 11 }, [], '', 'name')]),
    ])];
    const fields = gametypeFields(page.gametypes.schema, top.base, page.gametypes.builtin);
    for (const table of new Set(fields.map((field) => field.table))) {
      const values = parsed.values[table] || {};
      groups.push(h('fieldset', {}, [h('legend', { text: humanize(table) }), ...fields.filter((field) => field.table === table).map((field) => {
        const value = values[field.key];
        const label = humanize(field.key);
        const range = field.type === 'integer' && field.min != null ? `${field.min}–${field.max}` : '';
        const choices = field.type === 'boolean' ? [['true', 'On'], ['false', 'Off']] : (field.choices || []).map((choice) => [choice, humanize(choice)]);
        // (the rule's word, and its short word, in the title: as a rotation entry writes it)
        return h('label', {}, [h('span', { title: field.key + (field.alias && field.alias !== field.key ? ` (${field.alias})` : '') }, [label,
          range && h('span', { class: 'range', text: range })]), field.type === 'integer' ?
          control('input', { type: 'number', step: '1', min: field.min, max: field.max, placeholder: "Base's",
            value: value === undefined ? '' : String(value) }, [], table, field.key, field) :
          control('select', {}, [option('', "Base's", value === undefined),
            ...choices.map(([item, text]) => option(item, text, value !== undefined && String(value) === item))], table, field.key, field)]);
      })]));
    }
    $('gametype-form').replaceChildren(...groups);
    showTomlErrors();
  }

  // the text typed: the form follows it
  listen('gametype-text', 'input', () => {
    const parsed = showTomlErrors();
    if (parsed.errors.length) return;
    if ($('gametype-form').querySelector('[data-key="base"]').value !== parsed.values[''].base) {
      renderGametypeForm();
      return;
    }
    for (const node of $('gametype-form').querySelectorAll('[data-key]:not([data-key="base"])')) {
      const value = (parsed.values[node.getAttribute('data-table')] || {})[node.getAttribute('data-key')];
      node.value = value === undefined ? '' : String(value);
    }
  });
  listen('gametype-new', 'submit', (event) => {
    event.preventDefault();
    const name = $('new-name').value.trim();
    if (page.gametypes.files.some((file) => file.name === name)) openGametype(name);
    else showEditor(name, newGametypeText($('new-base').value, name), true);
  });
  listen('gametype-save', 'click', async () => {
    const editor = page.editor;
    const result = await call('PUT', `/api/gametypes/${encodeURIComponent(editor.name)}`, { text: $('gametype-text').value });
    if (!result.ok) {
      $('gametype-error').textContent = errorText(result);
      return;
    }
    toast(`gametypes/${editor.name}.toml is saved.`);
    editor.isNew = false;
    $('gametype-delete').hidden = false;
    titleText($('editor-title'), 'Gametype: ' + gametypeTitle(editor.name, page.gametypes));
    loadGametypes();
  });
  listen('gametype-delete', 'click', () => {
    const name = page.editor.name;
    ask(`Delete gametypes/${name}.toml?`, para('A rotation entry that plays it is then rejected.'), 'Delete', async () => {
      const result = await call('DELETE', `/api/gametypes/${encodeURIComponent(name)}`);
      if (result.ok) {
        $('gametype-editor').hidden = true;
        loadGametypes();
      }
      return result;
    }, true);
  });
  listen('gametype-close', 'click', () => { $('gametype-editor').hidden = true; });

  // ---------- bans

  async function loadBans() {
    if (!roleAtLeast(page.role, 'admin')) return;
    const result = await call('GET', '/api/bans');
    if (!listLoaded('bans', 'bans-empty', result)) return;
    const bans = result.data.bans || [];
    $('bans').querySelector('tbody').replaceChildren(...bans.map((ban) => h('tr', {}, [
      cell('#', String(ban.n)), cell('When', ban.when), cell('Players', ban.players || '–'), cell('Address', ban.ip || '–', 'mono'),
      cell('Hardware id', ban.hwid || '–', 'mono'), cell('Reason', ban.reason || '–'), cell('Until', formatUntil(ban.until)),
      cell('', small('Unban', true, () => ask(`Unban ${ban.players || ban.ip}?`, para(`Ban ${ban.n} is taken out of bans.txt.`), 'Unban',
        async () => {
          const answer = await call('DELETE', `/api/bans/${ban.n}`);
          if (answer.ok) loadBans();
          return answer;
        })), 'row-actions'),
    ])));
    $('bans').hidden = !bans.length;
    $('bans-empty').hidden = bans.length > 0;
  }

  // ---------- the console: the log, and commands

  const logNode = (line) => h('div', { class: `line level-${line.level} cat-${line.cat}` }, [
    h('time', { text: formatTime(line.time) }), ' ', h('span', { class: 'cat', text: line.cat }), ' ', line.text]);

  function follow() {
    if ($('follow').checked) $('log').scrollTop = $('log').scrollHeight;
  }

  function addLog(lines) {
    const merged = mergeLog(page.log.lines, lines);
    const view = $('log');
    page.log.lines = merged.lines;
    if (!merged.appended) view.replaceChildren(...merged.lines.map(logNode));
    else {
      for (let index = 0; index < merged.dropped && view.firstChild; index++) view.firstChild.remove();
      view.append(...merged.fresh.map(logNode));
    }
    follow();
  }

  function addOwn(lines, kind) {
    const view = $('log');
    view.append(...lines.map((text) => h('div', { class: 'line mine ' + (kind || '') }, text)));
    while (view.childNodes.length > LOG_LINES + 200) view.firstChild.remove();
    follow();
  }

  listen('log', 'scroll', () => {
    const view = $('log');
    $('follow').checked = view.scrollHeight - view.scrollTop - view.clientHeight < 24;
  });
  listen('follow', 'change', follow);
  listen('log-clear', 'click', () => $('log').replaceChildren());
  $('categories').replaceChildren(...CATEGORIES.map((category) => h('label', { class: 'chip' }, [
    h('input', { type: 'checkbox', checked: true, listen: { change: (event) => {
      $('log').classList.toggle('hide-' + category, !event.target.checked);
      follow();
    } } }), category])));

  async function pollLog() {
    for (let round = 0; round < 8; round++) {
      const result = await call('GET', `/api/log?after=${page.log.cursor}&limit=512`);
      if (!result.ok) return;
      const taken = takeLog(page.log, result.data);
      if (taken.reset) {
        page.log.lines = [];
        $('log').replaceChildren();
        addOwn(['(the server started again)'], 'note');
      }
      if (taken.missed && page.log.lines.length) addOwn([`(${taken.missed} lines left the server's log before they were read)`], 'note');
      const before = page.log.cursor;
      page.log.cursor = taken.cursor;
      addLog(taken.lines);
      // (a whole page read: more may wait; chat's lines left out of a
      // moderator's answer count, by the server's next)
      if (taken.reset || taken.cursor - before < 512) return;
    }
  }

  async function runCommand(line) {
    addOwn(['> ' + line], 'command-line');
    const result = live.state === 'live' ? await live.command(line) : await call('POST', '/api/command', { line });
    const lines = (result.data && result.data.lines) || [];
    if (lines.length) addOwn(lines);
    // (a command that failed says why in its lines)
    if (!result.ok && !(lines.length && result.error && result.error.code === 'failed')) addOwn([errorText(result)], 'bad');
  }

  listen('command-form', 'submit', (event) => {
    event.preventDefault();
    const line = $('command').value.trim();
    if (!line) return;
    if (page.history[page.history.length - 1] !== line) page.history.push(line);
    if (page.history.length > 100) page.history.shift();
    page.historyIndex = page.history.length;
    $('command').value = '';
    $('follow').checked = true;
    runCommand(line);
  });
  listen('command', 'keydown', (event) => {
    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      const step = historyStep(page.history, page.historyIndex, event.key === 'ArrowUp' ? -1 : 1);
      page.historyIndex = step.index;
      $('command').value = step.text;
    } else if (event.key === 'Tab' && $('command').value) {
      const done = complete($('command').value, page.commands, page.players);
      $('command').value = done.line;
      $('command-hint').textContent = done.options.length > 1 ? done.options.join('  ') : 'Tab completes, Up and Down go through what was run.';
    } else {
      return;
    }
    event.preventDefault();
  });

  // ---------- settings: a form from the server's table

  async function loadSettings() {
    if (!roleAtLeast(page.role, 'admin')) return;
    const result = await call('GET', '/api/settings');
    if (!result.ok) return;
    page.settings = (Array.isArray(result.data.settings) ? result.data.settings : []).map(normalizeSetting);
    renderSettings();
  }

  const settingId = (name) => 'setting-' + name.replace(/\W/g, '-');

  // each setting a menu row: its name in words, what it does, its key and
  // environment variable; the control in one column with the default under
  // it. Sections by table and topic, a list of them to jump to.
  function renderSettings() {
    const tables = new Map();
    for (const setting of page.settings) {
      const c = settingControl(setting, page.role);
      const id = settingId(setting.name);
      const { table, topic } = settingTopic(setting.name);
      let input;
      if (c.kind === 'checkbox') {
        input = h('input', { type: 'checkbox', class: 'toggle', id, checked: c.value === true, disabled: c.disabled,
          attrs: { role: 'switch' } });
        // (a choice in words; its word is the value sent)
      } else if (c.kind === 'select') input = h('select', { id, disabled: c.disabled }, c.choices.map((choice) => option(choice, humanize(choice) || 'Not set', choice === String(c.value))));
      else if (c.kind === 'secret') {
        input = h('span', { class: 'secret' }, [
          h('input', { type: 'password', id, placeholder: c.placeholder, disabled: c.disabled, autocomplete: 'new-password' }),
          h('label', { class: 'check' }, [h('input', { type: 'checkbox', id: id + '-clear', disabled: c.disabled }), 'Clear'])]);
      } else {
        // (an empty one says so: "Not set", never a blank box)
        input = h('input', { type: c.kind, id, value: String(c.value ?? ''), min: c.min, max: c.max, step: c.step, disabled: c.disabled,
          placeholder: 'Not set' });
      }
      const range = c.min !== null && c.max !== null ? `${c.min} to ${c.max}` : c.min !== null ? `${c.min} or more` : c.max !== null ? `${c.max} or less` : '';
      // (the default as the control shows it: a choice in words, an empty one a dash)
      const shown = c.defaultText === '(empty)' ? 'Not set' : c.kind === 'select' || c.kind === 'checkbox' ? humanize(c.defaultText) : c.defaultText;
      const row = h('div', { class: 'setting' + (c.disabled ? ' pinned' : ''), data: { name: setting.name } }, [
        h('div', { class: 'setting-text' }, [
          h('label', { htmlFor: id, class: 'setting-name', text: settingLabel(setting.name) }),
          settingHelp(setting.name, c.comment) && h('p', { class: 'setting-help', text: settingHelp(setting.name, c.comment) }),
          h('p', { class: 'setting-key' }, [h('code', { text: setting.name }), c.env && h('code', { text: c.env })])]),
        h('div', { class: 'setting-control' }, [input,
          (c.defaultText || range) && h('p', { class: 'setting-default' }, [
            c.defaultText && h('span', {}, [h('span', { class: 'k', text: 'Default' }), ' ', shown]),
            range && h('span', {}, [h('span', { class: 'k', text: 'Range' }), ' ', range])]),
          c.reason && h('p', { class: 'setting-lock' }, [h('span', { class: 'badge', text: 'Locked' }), ' ', c.reason]),
          h('p', { class: 'error' })]),
      ]);
      if (!tables.has(table)) tables.set(table, new Map());
      const topics = tables.get(table);
      if (!topics.has(topic)) topics.set(topic, []);
      topics.get(topic).push(row);
    }
    const sectionId = (table, topic) => `settings-${table}-${topic.toLowerCase().replace(/\W+/g, '-')}`;
    const tableTitle = (table) => table.charAt(0).toUpperCase() + table.slice(1);
    $('settings-fields').replaceChildren(...[...tables].map(([table, topics]) => h('section', { class: 'settings-table' }, [
      h('h2', { text: tableTitle(table) }),
      ...[...topics].map(([topic, rows]) => h('section', { class: 'settings-topic', id: sectionId(table, topic) }, [
        h('h3', { text: topic }), ...rows])),
    ])));
    // (buttons, not links: the address's # names the tab)
    $('settings-nav').replaceChildren(...[...tables].map(([table, topics]) => h('div', { class: 'settings-nav-group' }, [
      h('span', { class: 'settings-nav-table', text: tableTitle(table) }),
      ...[...topics.keys()].map((topic) => h('button', { type: 'button', text: topic, listen: { click: () => {
        const target = $(sectionId(table, topic));
        target.scrollIntoView({ block: 'start' });
        const first = target.querySelector('input, select');
        if (first && !first.disabled) first.focus({ preventScroll: true });
      } } })),
    ])));
  }

  function settingsValues() {
    const values = {};
    for (const setting of page.settings) {
      const input = $(settingId(setting.name));
      if (!input) continue;
      values[setting.name] = setting.secret ? { text: input.value, clear: $(settingId(setting.name) + '-clear').checked } :
        input.type === 'checkbox' ? input.checked : input.value;
    }
    return values;
  }

  listen('settings-form', 'submit', async (event) => {
    event.preventDefault();
    const { body, errors } = settingsChanges(page.settings, settingsValues(), page.role);
    const showErrors = (list) => {
      for (const error of list) {
        const row = $('settings-fields').querySelector(`[data-name="${error.name}"] .error`);
        if (row) row.textContent = error.message;
      }
    };
    for (const node of $('settings-fields').querySelectorAll('.error')) node.textContent = '';
    showErrors(errors);
    if (errors.length || !Object.keys(body).length) {
      $('settings-note').textContent = errors.length ? 'Nothing was saved: some values are wrong.' : 'Nothing changed.';
      return;
    }
    const result = await call('PUT', '/api/settings', body);
    if (!result.ok) {
      $('settings-note').textContent = errorText(result);
      return;
    }
    const refused = result.data.errors || [];
    await loadSettings();
    showErrors(refused);
    $('settings-note').textContent = `Saved: ${(result.data.applied || []).join(', ') || 'nothing'}.` +
      (refused.length ? ` Refused: ${refused.map((error) => error.name).join(', ')}.` : '');
  });
  listen('settings-revert', 'click', renderSettings);

  // ---------- about

  function renderAbout() {
    const setting = (name) => (page.settings.find((item) => item.name === name) || {}).value;
    const s = page.status || {};
    $('about-ports').replaceChildren(
      fact('Internet play', `${setting('network.tunnel_port') || 2302}/udp`, null, 'Every player (network.tunnel_port): open it.'),
      fact('System link', '5150/udp+tcp, 5151/udp, 5153/tcp', null, 'On the LAN only.'),
      fact('This page', `${setting('server.web_port') || location.port || 80}/tcp`, null,
        'The page, the API and the RCON (server.web_port): not for the internet without TLS.'),
      fact('Going out', '443/tcp, 1883/tcp', null, 'The master servers, and the message brokers of invites and listings.'));
    $('about-facts').replaceChildren(fact('Name', s.name || '–'), fact('Build', s.build || '–'),
      fact('Network version', String(s.network_version ?? '–')), fact('Signed in as', page.role || '–'), fact('Address', location.host));
  }

  // ---------- a command of the server's, from a screen (its lines, or why not)

  // a refusal as a sentence: a capital first, a full stop last
  const sentence = (text) => {
    const said = String(text || '').trim();
    return said ? said.charAt(0).toUpperCase() + said.slice(1) + (/[.!?)]$/.test(said) ? '' : '.') : '';
  };

  // (a refusal's line without the command's name in front: "sv_whitelist: 1.2.3.4 is not listed" reads "1.2.3.4 is
  // not listed")
  async function runLine(line) {
    const result = await call('POST', '/api/command', { line });
    if (!result.ok && result.error && result.error.message) {
      const message = result.error.message.replace(/^[a-z_]+: /, '');
      result.error = { ...result.error, message: message.charAt(0).toUpperCase() + message.slice(1) };
    }
    return result;
  }

  // a secret shown once: its text to copy, and that it will not be seen again
  function showSecret(title, intro, secret) {
    // (the whole secret in view, wrapped: a one-line field showed its end alone)
    const field = h('textarea', { readOnly: true, value: secret, class: 'secret-text', attrs: { rows: 2, 'aria-label': title,
      spellcheck: 'false' } });
    const copied = h('p', { class: 'note copied', attrs: { role: 'status' } });
    const copy = h('button', { type: 'button', class: 'primary', text: 'Copy', listen: { click: async () => {
      try {
        await navigator.clipboard.writeText(secret);
        copied.textContent = 'Copied.';
      } catch {
        field.select();
        copied.textContent = 'Selected: copy it with Ctrl+C.';
      }
    } } });
    ask(title, [para(intro), h('div', { class: 'inline secret-row' }, [field, copy]), copied,
      h('p', { class: 'once', text: "You won't see this again: copy it now. The server keeps only its hash." })],
    'Done', async () => ({ ok: true }), false, { only: true });
    field.focus();
    field.select();
  }

  // ---------- API keys

  async function loadKeys() {
    if (!roleAtLeast(page.role, 'admin')) return;
    const result = await call('GET', '/api/keys');
    if (!listLoaded('keys', 'keys-empty', result)) return;
    const keys = result.data.keys || [];
    $('keys').querySelector('tbody').replaceChildren(...keys.map((key) => h('tr', { class: key.expired ? 'ended' : null }, [
      cell('Name', h('span', { class: 'key-name' }, [key.name, key.expired && h('span', { class: 'tag', text: 'Ended' })])),
      cell('Role', titleCase(key.role)), cell('Ends', formatExpiry(key.expires, key.expired)),
      cell('Only from', key.allow ? breakable(key.allow.split(',').join(', ')) : 'Anywhere'),
      cell('Last used', key.last_used ? formatAgo(key.last_used) : 'Never'),
      cell('Actions', small('Revoke', true, () => ask(`Revoke the key ${key.name}?`,
        para('Its next request is refused, and a console it has open closes. Whatever uses it stops working.'), 'Revoke',
        async () => {
          const answer = await call('DELETE', `/api/keys/${encodeURIComponent(key.name)}`);
          if (answer.ok) {
            loadKeys();
            toast(`The key ${key.name} is revoked.`);
          }
          return answer;
        }, true), 'danger'), 'row-actions'),
    ])));
    $('keys').hidden = !keys.length;
    $('keys-empty').hidden = keys.length > 0;
    fillKeyRoles();
  }

  // (a key's role: at most the signer's own; known from the session, so filled at the start too)
  function fillKeyRoles() {
    const chosen = $('key-role').value || 'viewer';
    fillSelect($('key-role'), keyRoles(page.role).map((role) => option(role, titleCase(role), role === chosen)), 'No role to give');
  }

  listen('key-add', 'submit', async (event) => {
    event.preventDefault();
    let allow;
    try {
      allow = allowText($('key-allow').value);
    } catch (error) {
      toast(error.message, true);
      return;
    }
    const name = $('key-name').value.trim();
    const result = await call('POST', '/api/keys', { name, role: $('key-role').value, days: Number($('key-days').value), allow });
    if (!result.ok) {
      toast(sentence(errorText(result)), true);
      return;
    }
    $('key-name').value = '';
    $('key-allow').value = '';
    loadKeys();
    const role = result.data.role === 'admin' ? 'an admin' : `a ${result.data.role}`;
    showSecret('New key', `${name} signs in with it as ${role}: in the Authorization header as Bearer on the API, or on the RCON.`,
      result.data.key);
  });

  // ---------- access: admins, command roles, who may join

  async function loadAccess() {
    if (!roleAtLeast(page.role, 'admin')) return;
    const result = await call('GET', '/api/access');
    if (!listLoaded('admins', 'admins-empty', result)) return;
    page.access = result.data;
    renderAccess();
  }

  function renderAccess() {
    const data = page.access || {};
    const owner = roleAtLeast(page.role, 'owner');
    const admin = roleAtLeast(page.role, 'admin');
    const admins = data.admins || [];
    $('admins').replaceChildren(...admins.map((row) => {
      const view = adminView(row);
      return h('li', { class: row.expired ? 'ended' : null }, [
        h('span', { class: 'row-main' }, [
          h('span', { class: 'row-title' }, [...(view.kind === 'network' ? breakable(view.title) : [view.title]),
            h('span', { class: 'tag', text: view.kind === 'network' ? 'Network' : 'Login' })]),
          h('span', { class: 'row-meta' }, [h('span', { text: titleCase(row.role) }), h('span', { text: `Immunity ${row.immunity}` }),
            h('span', { class: row.expired ? 'bad' : null, text: row.expires ? `Ends: ${formatExpiry(row.expires, row.expired).toLowerCase()}` : 'Never ends' }),
            row.allow && h('span', {}, ['Only from ', ...breakable(row.allow.split(',').join(', '))])]),
        ]),
        owner && small('Remove', true, () => ask(`Remove ${view.kind === 'network' ? 'the network' : 'the login'} ${view.title}?`,
          para(view.kind === 'network' ? 'Players joining from it lose its role and immunity.' :
            'Its secret stops working at once.'), 'Remove', async () => {
            const answer = await runLine(`sv_admin_remove ${view.identity}`);
            if (!answer.ok) return answer;
            loadAccess();
            toast(`${view.title} is no longer an admin.`);
            return { ok: true };
          }, true), 'danger'),
      ]);
    }));
    $('admins').hidden = !admins.length;
    $('admins-empty').hidden = admins.length > 0;
    $('admin-add-block').hidden = !owner;
    $('admin-owner-note').hidden = owner;
    const access = data.access || [];
    $('access').replaceChildren(...access.map((item) => h('li', {}, [h('code', { class: 'row-code', text: item.command }),
      h('span', { class: 'row-value', text: `Needs ${item.role === 'admin' ? 'an' : 'a'} ${item.role}` })])));
    $('access').hidden = !access.length;
    $('access-empty').hidden = access.length > 0;
    const whitelist = data.whitelist || { on: false, entries: [] };
    $('whitelist-on').checked = !!whitelist.on;
    const entries = whitelist.entries || [];
    $('whitelist-state').textContent = whitelist.on ? (entries.length ? 'On: only the machines listed here, and admins, join.' :
      'On, with nothing listed: only admins join.') : 'Off: anyone may join. Turn it on to let in only those listed here.';
    $('whitelist').replaceChildren(...entries.map((entry) => {
      const view = whitelistView(entry);
      return h('li', {}, [h('span', { class: 'row-main' }, [h('span', { class: 'row-kind', text: view.kind }),
        h('span', { class: 'row-title' + (view.mono ? ' mono' : '') }, view.mono ? breakable(view.value) : view.value)]),
      small('Remove', admin, () => ask(`Take ${view.value} off the whitelist?`, para(whitelist.on ?
        'It can no longer join (it stays if it is in the game now).' : 'It is taken off the list.'), 'Remove', async () => {
        const answer = await runLine(`sv_whitelist remove ${commandWord(entry)}`);
        if (!answer.ok) return answer;
        loadAccess();
        toast(`${view.value} is off the whitelist.`);
        return { ok: true };
      }, true), 'danger')]);
    }));
    $('whitelist').hidden = !entries.length;
    $('reserved-slots').value = String(data.reserved_slots ?? 0);
  }

  listen('admin-kind', 'change', () => {
    const network = $('admin-kind').value === 'network';
    $('admin-name-label').textContent = network ? 'Network' : 'Login';
    $('admin-name').placeholder = network ? '192.168.1.0/24' : 'ops';
    $('admin-allow-label').hidden = network;
  });
  listen('admin-add', 'submit', async (event) => {
    event.preventDefault();
    let line;
    try {
      line = adminAddLine({ kind: $('admin-kind').value, name: $('admin-name').value, role: $('admin-role').value,
        immunity: $('admin-immunity').value, days: $('admin-days').value, allow: $('admin-allow').value });
    } catch (error) {
      toast(error.message, true);
      return;
    }
    const result = await runLine(line);
    if (!result.ok) {
      toast(sentence(errorText(result)), true);
      return;
    }
    const name = $('admin-name').value.trim();
    $('admin-name').value = '';
    $('admin-allow').value = '';
    loadAccess();
    const secret = secretFromLines(result.data.lines);
    if (secret) {
      showSecret('New admin login', `${name} signs in with this secret, as Basic auth on the API (the name, then the secret), or on the RCON.`,
        secret);
    } else {
      toast(`${name} is an admin network now.`);
    }
  });

  async function saveSetting(name, value, done) {
    const result = await call('PUT', '/api/settings', { [name]: value });
    const refused = result.ok && (result.data.errors || []).find((error) => error.name === name);
    if (!result.ok || refused) toast(sentence(refused ? refused.message : errorText(result)), true);
    else if (done) toast(done);
    loadAccess();
    loadSettings();
  }

  listen('whitelist-on', 'change', () => saveSetting('server.whitelist', $('whitelist-on').checked,
    $('whitelist-on').checked ? 'The whitelist is on.' : 'The whitelist is off: anyone may join.'));
  listen('reserved-form', 'submit', (event) => {
    event.preventDefault();
    const slots = Number($('reserved-slots').value);
    if (!Number.isInteger(slots) || slots < 0) {
      toast('Reserved slots is a whole number: 0 or more.', true);
      return;
    }
    saveSetting('server.reserved_slots', slots, slots ? `${slots} slot${slots === 1 ? '' : 's'} kept for admins.` : 'No slots kept.');
  });
  listen('whitelist-kind', 'change', () => {
    $('whitelist-value').placeholder = { ip: '192.168.1.0/24', hwid: '4f1c9a0e7b2d4c6e', discord: '123456789012345678',
      name: 'Player name' }[$('whitelist-kind').value];
  });
  listen('whitelist-add', 'submit', async (event) => {
    event.preventDefault();
    const value = $('whitelist-value').value.trim();
    let line;
    try {
      line = `sv_whitelist add ${commandWord(`${$('whitelist-kind').value}:${value}`)}`;
    } catch (error) {
      toast(error.message, true);
      return;
    }
    const result = await runLine(line);
    if (!result.ok) {
      toast(sentence(errorText(result)), true);
      return;
    }
    $('whitelist-value').value = '';
    loadAccess();
  });

  // ---------- backups (About)

  async function loadBackups() {
    const admin = roleAtLeast(page.role, 'admin');
    $('backups-panel').hidden = !admin;
    if (!admin) return;
    const result = await call('GET', '/api/backups');
    if (!listLoaded('backups', 'backups-empty', result)) return;
    const groups = backupGroups(result.data.backups);
    $('backups-keep').textContent = `The server copies a file before it writes it, and keeps the newest ${result.data.keep ?? 20} of each (server.backups_keep).`;
    $('backups').replaceChildren(...groups.map((group) => h('section', { class: 'backup-group' }, [
      h('p', { class: 'backup-file' }, [h('code', { text: group.file }), h('span', { text: `${group.backups.length} kept` })]),
      h('ul', { class: 'list rows' }, group.backups.map((backup) => h('li', {}, [
        h('span', { class: 'row-main' }, [h('span', { class: 'row-title', text: backup.time > 0 ?
          new Date(backup.time * 1000).toLocaleString() : backup.name }),
        h('code', { class: 'row-code muted', text: `backups/${backup.name}` })]),
        h('span', { class: 'row-value' }, [h('span', { text: formatBytes(backup.size) }),
          backup.time > 0 && h('span', { class: 'row-ago', text: formatAgo(backup.time) })]),
      ]))),
    ])));
    $('backups-empty').hidden = groups.length > 0;
  }

  // ---------- the bans' export and import

  listen('bans-export', 'click', async () => {
    const result = await call('GET', '/api/bans/export');
    if (!result.ok) {
      toast(errorText(result), true);
      return;
    }
    const stamp = new Date().toISOString().slice(0, 10);
    const name = `bans-${(page.status && page.status.instance) || 'server'}-${stamp}.json`;
    const url = URL.createObjectURL(new Blob([JSON.stringify({ bans: result.data.bans || [] }, null, 1)], { type: 'application/json' }));
    const link = h('a', { attrs: { href: url, download: name } });
    doc.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 10000);
    const saved = (result.data.bans || []).length;
    toast(`${saved} ban${saved === 1 ? '' : 's'} saved as ${name}.`);
  });
  listen('bans-import', 'click', () => $('bans-file').click());
  listen('bans-file', 'change', async () => {
    const file = $('bans-file').files[0];
    $('bans-file').value = '';
    if (!file) return;
    if (file.size > 1024 * 1024) {
      toast('That file is larger than 1 MB: a bans export is much smaller.', true);
      return;
    }
    const [text, existing] = await Promise.all([file.text(), call('GET', '/api/bans/export')]);
    let preview;
    try {
      preview = importPreview(text, existing.ok ? existing.data.bans : []);
    } catch (error) {
      toast(error.message, true);
      return;
    }
    const count = (list, one, many) => `${list.length} ${list.length === 1 ? one : many}`;
    const shown = preview.fresh.slice(0, 8);
    ask(preview.fresh.length ? `Import ${count(preview.fresh, 'ban', 'bans')}?` : 'Nothing new to import',
      [h('dl', { class: 'facts' }, [fact('New', String(preview.fresh.length)), fact('Already here', String(preview.have.length)),
        fact('Over', String(preview.over.length)), fact('Look wrong', String(preview.wrong.length))]),
      shown.length > 0 && h('ul', { class: 'import-list' }, shown.map((ban) => h('li', { text: banLine(ban) }))),
      preview.fresh.length > shown.length && para(`And ${preview.fresh.length - shown.length} more.`),
      para('Only the new ones are added; the server checks each again and skips what it already has.')].filter(Boolean),
      preview.fresh.length ? 'Import' : 'Close', async () => {
        if (!preview.fresh.length) return { ok: true };
        const result = await call('POST', '/api/bans/import', { bans: preview.fresh });
        if (result.ok) {
          loadBans();
          toast(`${result.data.added} added, ${result.data.skipped} skipped, ${result.data.refused} refused.`);
        }
        return result;
      }, false, { only: !preview.fresh.length });
  });

  // ---------- the RCON's WebSocket, and polling while it is down

  const live = createLive(root.WebSocket, `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`, {
    after: () => page.seq,
    onState: (state) => {
      $('link-state').textContent = { live: 'Live', connecting: 'Connecting' }[state] || 'Polling every 3 s';
      $('link-state').className = 'badge link-' + state;
    },
    onLive: () => Promise.all([pollLog(), loadPlayers(), loadEvents()]),
    onDenied: () => signedOut('The session ended: sign in again.'),
    onOverflow: () => toast('The page read the server too slowly: it polls until it connects again.', true),
    onEvent: (event) => {
      if (event.event === 'status') return takeStatus(event.status);
      if (event.event === 'log') {
        // (the log's own numbering, n: not the events' seq; mergeLog keeps
        // a line once)
        if (!Number.isInteger(event.n)) return;
        page.log.cursor = Math.max(page.log.cursor, event.n);
        return addLog([{ n: event.n, time: event.time, level: event.level, cat: event.cat, text: event.text }]);
      }
      // (an event seen already, replayed after a reconnection or read by
      // /api/events meanwhile: once)
      if (Number.isInteger(event.seq) && event.seq > 0) {
        if (event.seq <= page.seq) return;
        page.seq = event.seq;
      }
      addFeed(event);
      if (event.event === 'score' && Array.isArray(event.players)) {
        page.players = event.players;
        renderPlayers();
      } else if (/^(player_.*|kick|ban|mute|unmute|game_started)$/.test(event.event)) {
        clearTimeout(page.playersTimer);
        page.playersTimer = setTimeout(loadPlayers, 400);
      }
      if (/^(ban|unban)$/.test(event.event)) loadBans();
      if (/^(lobby_opened|reloaded)$/.test(event.event)) loadRotation();
    },
  });

  // the events missed (at the start, the last ones: the feed)
  async function loadEvents() {
    const result = await call('GET', `/api/events?after=${page.seq}`);
    for (const event of (result.ok && result.data.events) || []) {
      if (!Number.isInteger(event.seq) || event.seq <= page.seq) continue;
      page.seq = event.seq;
      addFeed(event);
    }
  }

  async function poll() {
    if (live.state === 'live' || !page.role) return;
    const status = await call('GET', '/api/status');
    if (status.ok) takeStatus(status.data);
    await Promise.all([loadPlayers(), pollLog(), loadEvents()]);
  }

  // ---------- tabs

  function showTab(name) {
    const button = doc.querySelector(`[data-tab="${name}"]`);
    if (!button || button.disabled || button.hidden) name = 'status';
    for (const tab of doc.querySelectorAll('[data-tab]')) {
      tab.setAttribute('aria-selected', String(tab.getAttribute('data-tab') === name));
      tab.classList.toggle('current', tab.getAttribute('data-tab') === name);
    }
    for (const section of doc.querySelectorAll('.tab')) section.hidden = section.id !== 'tab-' + name;
    if (name === 'console') follow();
    if ((name === 'rotation' || name === 'gametypes') && page.role) {
      if (page.mapsFailed) loadMaps();
      if (page.gametypesFailed) loadGametypes();
    }
    if (name === 'rotation') loadRotation();
    if (name === 'bans') loadBans();
    if (name === 'access') loadAccess();
    if (name === 'keys') loadKeys();
    if (name === 'about') loadBackups();
  }

  for (const tab of doc.querySelectorAll('[data-tab]')) {
    tab.addEventListener('click', () => {
      history.replaceState(null, '', '#' + tab.getAttribute('data-tab'));
      showTab(tab.getAttribute('data-tab'));
    });
  }
  root.addEventListener('hashchange', () => showTab(location.hash.slice(1)));

  // ---------- the status tab's actions

  listen('next-map', 'click', () => ask('Play the next map?', para("The game in progress ends without its scores, and the rotation's next starts."),
    'Next map', () => call('POST', '/api/rotation/next', {}), true));
  listen('end-game', 'click', () => ask('End the game?', para('The game ends with its scores; the postgame shows them, then the next game starts.'),
    'End game', () => call('POST', '/api/command', { line: 'sv_end_game' }), true));
  listen('say-form', 'submit', async (event) => {
    event.preventDefault();
    const text = $('say-text').value.trim();
    if (!text) return;
    const result = await call('POST', '/api/say', { text });
    if (result.ok) $('say-text').value = '';
    toast(result.ok ? 'Said.' : errorText(result), !result.ok);
  });
  listen('invite-copy', 'click', async () => {
    try {
      await navigator.clipboard.writeText($('invite').value);
      toast('The invite link is copied.');
    } catch {
      $('invite').select();
    }
  });

  // ---------- signing in and out

  function showLogin(message) {
    $('app').hidden = true;
    $('login').hidden = false;
    $('login-error').textContent = message || '';
    $('login-password').focus();
  }

  function signedOut(message) {
    if (!page.role) return;
    page.role = null;
    live.stop();
    page.timers.forEach(clearInterval);
    page.timers = [];
    closeDialog();
    showLogin(message);
  }

  listen('login-form', 'submit', async (event) => {
    event.preventDefault();
    $('login-button').disabled = true;
    const result = await api.call('POST', '/api/login', { password: $('login-password').value });
    $('login-button').disabled = false;
    $('login-password').value = '';
    if (result.ok) start(result.data);
    else $('login-error').textContent = errorText(result);
  });
  listen('logout', 'click', async () => {
    await call('POST', '/api/logout', {});
    signedOut('Signed out.');
  });

  async function start(session) {
    page.role = session.role;
    api.setCsrf(session.csrf);
    $('login').hidden = true;
    $('app').hidden = false;
    $('role').textContent = page.role;
    gate();
    fillKeyRoles();
    showTab(location.hash.slice(1) || 'status');
    renderPickers();
    const [status] = await Promise.all([call('GET', '/api/status'), loadMaps()]);
    if (status.ok) takeStatus(status.data);
    // (each on its own: one that fails leaves the others, and the
    // connection, to start)
    const results = await Promise.allSettled([loadPlayers(), loadRotation(true), loadGametypes(), loadBans(), loadSettings(),
      pollLog(), loadEvents(), loadAccess(), loadKeys(), loadBackups(),
      call('POST', '/api/command', { line: 'help' }).then((help) => { if (help.ok) page.commands = commandsFromHelp(help.data.lines); })]);
    for (const result of results) if (result.status === 'rejected') console.error(result.reason);
    if (!page.role) return;
    renderAbout();
    live.connect();
    // (the session ends after 30 minutes without an HTTP request: the
    // WebSocket's traffic does not count, so the page asks now and then)
    page.timers.push(setInterval(poll, 3000), setInterval(live.ping, 25000), setInterval(() => page.status && renderStop(), 1000),
      setInterval(() => page.status && sample(page.status.players), 15000),
      setInterval(() => call('GET', '/api/session'), SESSION_REFRESH));
  }

  api.call('GET', '/api/session').then((result) => {
    if (result.ok) start(result.data);
    else showLogin(result.error.code === 'network' ? errorText(result) : '');
  });
}

// the RCON's WebSocket for the page: signed in by the cookie, subscribed to
// every group (with after: the events since the last one seen), commands
// answered by id, and connected again after a loss (2 s, doubling to 30)
function createLive(WebSocketClass, url, handlers) {
  const live = { state: 'off', socket: null, next: 1, pending: new Map(), retry: 2000, timer: null };
  const settle = (id, result) => {
    const resolve = live.pending.get(id);
    if (live.pending.delete(id)) resolve(result);
  };
  const setState = (state) => {
    live.state = state;
    handlers.onState(state);
  };
  live.connect = () => {
    if (live.socket) return;
    try {
      live.socket = new WebSocketClass(url, 'opence-rcon-1');
    } catch {
      live.lost();
      return;
    }
    const socket = live.socket;
    setState('connecting');
    socket.addEventListener('message', (event) => {
      let message;
      try {
        message = JSON.parse(event.data);
      } catch {
        return;
      }
      if (message.type === 'auth' && !message.ok) {
        handlers.onDenied(message.error || {});
      } else if (message.type === 'auth') {
        live.retry = 2000;
        setState('live');
        socket.send(JSON.stringify(handlers.after() ? { type: 'subscribe', events: GROUPS, after: handlers.after() } :
          { type: 'subscribe', events: GROUPS }));
        handlers.onLive();
      } else if (message.type === 'event') {
        handlers.onEvent(message);
      } else if (message.type === 'reply' || message.type === 'error') {
        // (a command that ran and failed: a reply, ok false, its lines and no
        // error; one refused: an error)
        settle(message.id, message.ok ? { ok: true, data: message } : { ok: false, data: message,
          error: message.error || (message.type === 'reply' ? { code: 'failed', message: '' } : { code: 'unknown' }) });
      } else if (message.type === 'overflow') {
        handlers.onOverflow();
      }
    });
    // (a refused handshake: a browser says error, then close; Node error alone)
    for (const name of ['close', 'error']) {
      socket.addEventListener(name, () => {
        if (live.socket === socket) live.lost();
      });
    }
  };
  live.lost = () => {
    live.socket = null;
    for (const id of [...live.pending.keys()]) settle(id, { ok: false, error: { code: 'network', message: 'the connection was lost' } });
    if (live.state === 'off') return;
    setState('polling');
    clearTimeout(live.timer);
    live.timer = setTimeout(live.connect, live.retry);
    live.retry = Math.min(30000, live.retry * 2);
  };
  live.command = (line, timeout = 30000) => new Promise((resolve) => {
    const id = live.next++;
    live.pending.set(id, resolve);
    try {
      live.socket.send(JSON.stringify({ type: 'command', id, line }));
    } catch {
      settle(id, { ok: false, error: { code: 'network', message: 'the connection was lost' } });
    }
    setTimeout(() => settle(id, { ok: false, error: { code: 'timeout', message: 'no answer in time' } }), timeout);
  });
  live.ping = () => {
    if (live.state === 'live') live.socket.send('{"type":"ping"}');
  };
  live.stop = () => {
    live.state = 'off';
    clearTimeout(live.timer);
    const socket = live.socket;
    live.socket = null;
    if (socket) socket.close();
  };
  return live;
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    ROLES, CATEGORIES, GROUPS, roleAtLeast, banPresets, formatDuration, formatClock, formatTime, formatAgo, formatUntil,
    teamName, stateLabel, formatMs, gametypeText, humanize, mapView, gametypeTitle, ruleText, entryView, entryLabel, breakParts, masterView, settingLabel, settingHelp, SETTING_HELP, settingTopic, errorText, createApi, takeLog, mergeLog, pushSample, sparkPoints, describeEvent, parseEntryText,
    rulesText, mapToken, makeEntry, rotationMove, rotationRemove, rotationInsert, markRejected, settingControl,
    settingsChanges, settingValue, normalizeSetting, settingEditable, settingRole, OWNER_SETTINGS, parseToml, setTomlValue,
    formatTomlValue, engineOf, builtinNames, aeBuiltinFolders, gametypeFields, fieldValue, newGametypeText, titleOf, playBody,
    commandsFromHelp, complete, historyStep, h, setDocument, createLive,
    stopView, stopBody, stopQuestion, keyRoles, formatExpiry, allowText, adminView, commandWord, adminAddLine, secretFromLines,
    whitelistView, formatBytes, backupGroups, banKey, importPreview, banLine,
  };
} else {
  startPage(window);
}
