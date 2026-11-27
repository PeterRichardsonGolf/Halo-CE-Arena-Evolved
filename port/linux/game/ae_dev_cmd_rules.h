/*
AE_DEV_CMD_RULES.H

DEV MODE's commands: one set of commands for the console, the telnet console,
TEST RUN set-up lines and scripts (ae_dev_cmd.c runs them in the game). This
part is the grammar alone: a line to a command and its arguments, checked, and
the names for completion. Pure: string and stdlib only, no engine headers.

A line is words split on spaces; text in double quotes is one word. A dev
command is a line whose first word is a command's name (taken only when the
caller allows bare names: DEV MODE on) or "dev" followed by one (always taken;
"dev" alone is help). help is "dev help" only: a bare "help" is the script
console's own. The script functions' names never clash with the bare names
(tools/test_ae_dev_cmd.py).
The command's name and its fixed words are lowercase; matching ignores case.

	give <weapon>|all              a weapon of the map (by its tag's last name)
	spawn <vehicle> [here|ahead|door|passdoor|<spot>]
	                               door, passdoor: the vehicle's driver (passenger) side faces you, 1.6 units ahead
	bots <0-127> [even|against] [recruit|easy|normal|heroic|elite|legendary|demon]
	powerup <item> [now] [<player>] now: picked up at once; <player> (me, enemy, mate, dummy, an index or the start of a
	                               name): its effect given to that player's unit at once, nothing left lying
	powerup <item> ahead           the item left lying 3 units in front of you, where a player has room to stand: not
	                               picked up until somebody walks there (refused when there is no room ahead)
	respawn [me|all]
	kill [me|all|bots]
	kill <victim> by <killer>      two players by name or number, the killer's weapon's lethal blow on the victim through the
	                               damage path (kill credit, feed and medals as in play); works with no player of its own
	kill me by <bot|name> [after <1-600> | every <1-600> [times <2-20>]]
	                               (rc2) a kill with a killer (the death camera looks at it): at once, or pending: after
	                               that many game seconds (once, made when you are next alive), or every that many seconds,
	                               times kills in all (2 when not given). A new game clears it
	end round|game
	score red|blue|me <0-9999>
	time <seconds>|<m:ss>          the round time left (0 to 9999 s)
	goto <spot> [facing <spot>] | start <n> | <x> <y> <z> [facing <degrees>]
	                               a spot or start faces as the map sets it, or toward the other spot; in a CTF game flag_ours
	                               and flag_theirs are the two flags' stands. A point with no room for a player is refused
	                               and the player stays where they are
	goto bot <x> <y> <z>           (1.0 rc2) the nearest living bot (an enemy first) moved there: a staged scene's
	gametype <name ...>            the rest of the line
	option <row> <value>           a GAME OPTIONS row of the game being played, as the pause menu sets it (option
	                               needler mcc, option fall_damage halo_2: words matched without case, spaces or "_");
	                               only the rows the game reads as it plays (NEEDLER, FALL DAMAGE, TIMERS), and not
	                               with other players in the game
	config <key> <value ...>       a config key for this run; the rest of the line is its value
	agent check <1-3> pass|fail|look|waiting [reason ...]
	agent say <text ...> | agent clear
	                               the live agent's verdict on a check of the TEST RUN card, its message, or neither
	mark <text ...>                a line in the log and the event stream
	shot                           a screenshot now
	archive status | list [<text>] | get <row> | cancel
	                               (1.0) the community map archive's library, without its screen: status: the setting,
	                               the list's state and the download's, one line; list [<text>]: starts reading the signed
	                               list (game.ce_archive and game.ce_archive_consented on: set them with `config`), then
	                               the maps that match and the first five rows (a * marks one installed); get <row>: that
	                               row of the last list downloaded into custom_maps (never over a map there); cancel: the
	                               download stops. AE_CE_ARCHIVE_BASE=http://127.0.0.1:<port> with AE_CE_ARCHIVE_TEST_KEY=<64
	                               hex digits> in the environment points it at a made-up archive on this machine
	forge start <map> | mode monitor|play | status
	                               (rc9) Content Evolved's Forge: start: a Forge session on a level alone from the menus (<map>: a
	                               level's leaf name, bloodgulch or b30, or its path; Forge's tags must be built); mode: the
	                               Monitor entered or left; status: the mode, what is placed, the base map's tag count and the
	                               network version. Only the 64-bit Linux build has Forge
	forge place <tag path> [x y z [yaw]] | copy <n> | delete <n> | grab <n> | snap off|15|45|90 | list
	                               (rc9) Forge's editing as its tools do it, carried out at the next Forge update once the Monitor
	                               flies (the result is a "forge dev:" line in the log): place stands the palette's tag at x y z
	                               (yaw in degrees), else 5 world units ahead of the Monitor, as the instance #n of forge list;
	                               copy makes a second one beside #n; delete removes #n; grab holds #n as a click on it does;
	                               snap sets the rotation snap (off, 15, 45 or 90 degrees); list names what is placed
	forge save <name> | play <name> <gametype> | gametype <n|last> <all|ctf|slayer|oddball|koth|race>
	                               (rc9) save writes the session's map as Forge's SAVE MAP does (a name of a-z, 0-9 and _, up to 48;
	                               it is MY MAPS' list's next map, and its picture and text are in the file); play starts a game of
	                               the saved map with the gametype (a built-in one's name, or a saved one's) from the menus, as a
	                               TEST RUN's setup starts one: the map's objects of that gametype only; gametype sets which
	                               gametype the placed object #n (forge list; last: the one placed last) is for (all: every gametype)
	forge move <n> <x> <y> <z> | send <line>
	                               (rc9) move puts instance #n of forge list at x y z as a hand would (and tells the other machines); send sends the Forge
	                               line (!FM:.. as typed, up to 63 letters, digits, : . -) to the host as this machine's own, to see what the host does with it
	                               (a hosted game's joined machine only)
	forge host <map> [variant] | join <a.b.c.d:port>
	                               (rc9) a hosted Forge game on one machine, as the network test starts them: host hosts a Forge game on the level
	                               (a multiplayer map's name, bloodgulch; the variant a built-in gametype, slayer when none), join joins the Forge game
	                               hosted at that address; both from the menus (a Forge session's flag is on, its version is Forge's own, and the game
	                               starts from the main menu, 25 seconds after for a host, so that a machine can join first); the host's address is that of its network (HALO_NET_ADDRESS) and port 5150
	forge ready                    (rc9) answers ok once a staged Forge scene is up: the session on, Forge up on the level, every edit carried out and the
	                               last mode asked for taken; until then it answers that it is being made (a TEST RUN setup line waits on it)
	forge face | where <n>|last|all|ai
	                               (rc9) face turns the Monitor to the way the level leaves the most free ahead (a "forge dev: facing" line), so that
	                               what is placed ahead is in the open; where says in a "forge dev: where" line where instance #n of forge list (last: the one
	                               placed last; all: each thing placed; ai: each living AI unit) stands: its place, whether it rests on the ground or hovers over it, its distance and
	                               bearing from the viewer (the Monitor, or the player on foot; 0 degrees ahead, positive to the left) and whether the level's
	                               walls leave it in sight; it waits for a mode asked for to be taken. Things placed with no position rest on the ground
	forge ai <actor> <team> [none|patrol|route|hunt [seconds]] | route <points>
	                               (rc9) Forge's AI, carried out as the other edits (a "forge dev:" line): ai places a spawn of the actor (a part of its variant's
	                               name: elite, grunt) 5 world units ahead of the Monitor, on a team (own, player, human, covenant, flood, sentinel,
	                               players: ALL PLAYERS, red, blue), with a goal (patrol and route go to the lowest numbered AI PATROL volume or route
	                               there is), kept alive by respawning that many seconds after a death (1 to 600) when the seconds are given; route makes an AI route of 2 to 8
	                               points, 5 world units apart, in a line across the view 8 ahead of the Monitor; a hunting spawn stands 20 world units ahead, in sight
	forge ff infantry|vehicles|vehicle|ghost|heavy|zone|pad|player [all|1-5 [1-3]] | generator | rack | drop | ammo <0-100>
	                               (rc9) Forge's Firefight markers and props, carried out as the other edits (a "forge dev:" line): a marker of that spawn
	                               type for that round and wave (all when none): infantry, infantry and vehicles, a vehicle, a Ghost, a heavy vehicle, an
	                               enemy drop point (zone), a player vehicle pad, a player spawn; generator is a generator's marker; rack the props'
	                               resupply rack and drop the props' drop area. A marker stands 9 world units ahead of the Monitor, the next beside it (to its left and right in turn, 2.5 apart),
	                               the drop area 5 ahead and 2.5 aside, the rack 1.2 ahead (within its reach), all on the ground; ammo sets the weapon in hand's ammunition to that percentage, once forge mode play
	                               has put the Spartan on foot
	forge kill ai                  (rc9) every AI unit dies once the Spartan's mode is on and the AI stands (a RESPAWN is judged from there)
	forge volume gravity|kill|damage|hurt|oneway|patrol [value [x y z]] | settings biped|hands red|blue|ffa <name> | settings fog default|clear|light|heavy|flood | testspawn red|blue|ffa
	                               (rc9) volumes and map settings, carried out at the next Forge update once the Monitor flies (a "forge dev:" line
	                               in the log; forge mode play after them waits for them): volume makes a box of that kind where the Monitor's feet
	                               would stand, or with its feet at x y z (gravity a column over the feet, its value the strength in hundredths of the
	                               world's gravity, 0 to 1000, 25 when not given; kill; damage CONSTANT DAMAGE and hurt APPLY DAMAGE, their value a
	                               percentage of health, 1 to 100, 25; oneway a slab whose top is a floor from above); settings sets a team's
	                               player biped or hands to the level's tag with <name> in its last name, or the atmosphere (default is the map's own);
	                               testspawn is MAP SETTINGS' TEST SPAWN for the team: the player spawns as its biped once forge mode play leaves the Monitor
	help [command]                 (dev help only)
	list weapons|vehicles|items|spots|options
	biped [list|use <1-4>|off|check <1-4>]
	                               the biped packs this PC has (slot, name, added to the map or not); use and off set the
	                               PLAYER MODEL the next map load takes (the config debug.player_model as a TEST RUN's
	                               set line); check says the slot's pack's fitness on the map loaded or why it was refused
	view bots|spawns|items|net [on|off]
	viewmodel [<x> <y> <z> <scale>|place <right> <up> <forward> <yaw> <pitch> <roll> <scale>|probe|save|clear]
	                               the held weapon's first-person placement (offset along the gun's forward, left,
	                               up in model units, uniform scale), live; alone it prints the numbers; save keeps
	                               them in view_models.txt; clear is the shipped numbers again
	ttk [on|off]                   every kill answered with its time: the killer's first damage to the victim (this life) to the
	                               death, the hits and the headshots; alone it says whether it is on
	botskill [<bot> <shooting> <movement>|<bot> clear|clear]
	                               (0.9.3) one bot's own SHOOTING and MOVEMENT skill (recruit .. demon each), whatever BOT
	                               SKILL, SHOOTING SKILL and MOVEMENT SKILL say; <bot> is its number (bot=<n> in the
	                               log, 1 the first bot); clear: back to the settings (one bot or every bot); alone:
	                               the pairs set. Each bot's pair is logged once a game ("bots: skill: bot 3 ...")
	botrecord [demo [seed <n>] [bots <1-60>] | demo off | set <name> <field> <value> ... | page <name> [game] | forget]
	          [player <1-4>]
	                               (0.9.4) the bots' SERVICE RECORDS (AE TOURNAMENT): demo: a made-up store from the
	                               seed (pool names, varied totals, versus lines for the player's profile with a RIVAL,
	                               a TEAMMATE and a FIRST MEETING), never written; demo off: bot_records.txt again; set
	                               (made-up store only; 3 pairs a line): games|wins|ties|kills|deaths|with <n>,
	                               vs <you>-<it>; page: the BOT RECORD page of a bot (game: with a made-up THIS
	                               GAME, as from the post-game; else LAST GAME TOGETHER, as from CAREER); forget:
	                               the made-up store empty; alone: the store's counts
	botstyle [<bot> <style> [<weapon>]|<bot> clear|clear]
	                               (0.9.3) one bot's PLAY STYLE: steady, pusher, anchor, hopper or careful (how bold, how
	                               jumpy, how patient in a fight; its skill unchanged), and a favourite weapon (none,
	                               assault_rifle, battle_rifle, shotgun, sniper, pistol, rockets, plasma_rifle, needler);
	                               clear: no style (one bot or every bot); alone: the styles set
	dummy [<n>] [here|<distance>] [away] [mate]  <n> (1-8, one by default) bots that stand still and never shoot,
	                               <distance> (1-200 world units, 10 by default) ahead of you and facing you, or here;
	                               each heals to full 3 s after it was last hurt and respawns where it stood 1 s after it
	                               dies (a local game's bots); away: facing the way you face, its back to you
	dummy walk here|<x> <y> <z> [to <x> <y> <z>]
	                               (0.9.3) one walking dummy on your team, at your feet or at x y z: it walks to the goal by the
	                               bots' own routes (no goal: it roams) and never shoots, and its log says when it left its start,
	                               reached the goal or was still near its start after 10 s (a TEST RUN's staged spot check)
	dummy run <x> <y> <z> to <x> <y> <z> [loop]
	                               one runner of the other team, put at the first point: it runs to the second by the bots' own
	                               routes and never shoots; with loop it turns back at each end, again and again, so a staged
	                               scene opens on a player already moving on a known path (a flag carrier: `flag ours carry`
	                               after it). Refused when the first point has no room for a player. Its log says where it
	                               is once a second and when it turns
	dummy reach [<n>]              (0.9.3) the can't-reach report: n walking dummies (1-4, 4 by default) walk from every spawn
	                               to every flag stand, ball spawn, hill, race checkpoint and power item, by the bots' own
	                               routes; each walk's line in the log (reached, stuck, lost, slow or died, and where), the
	                               report in the config folder's reach/<map>.txt at the end; play alone (bots.count 0)
	dummy clear                    the dummies leave (a reach run stops, its walks so far written)
	hud                            the held weapon's HUD elements (the weapon HUD tag's own, HUD STYLE CE): number, kind, bitmap
	hud <i> <dx> <dy> [<scale>]    element i moved by dx, dy (HUD units, 640 x 480, right and down) and scaled, live
	hud <i> hide|show              element i not drawn, or drawn again
	hud reset [<i>]                element i (or all of the weapon's) as the tag has it
	hud save                       keeps the numbers in hud_offsets.txt
	tune                           the held weapon's numbers (ae_weapon_tune_rules.h's keys), each with its present value
	tune <key> <value>             one number changed live in the loaded tags (trigger0.rate 9, trigger0.spread 1..3,
	                               magazine0.size 30, projectile0.speed 400, projectile0.impact.damage 50, melee.damage
	                               80, weapon.zoom 2, weapon.autoaim 4): a bounds key takes a or a..b; only the numbers
	                               the engine cannot hold are refused, and not with other players in the game
	tune save                      writes the changed numbers to tune/<weapon key>.json in the config folder (a recipe's
	                               tune layer: tools/weapon_pack/apply_tune.py)
	tune reset                     the held weapon's changed numbers back as the map loaded them
	reset                          the group's start state back (TEST RUN)
	fireteam follow|regroup|attack|defend|watch|go|clear|status|mount
	fireteam attack|defend|watch|go ahead <1-60>   the order's point that many units ahead of the host, on the ground there (none:
	                               at the host's feet); mount: the campaign squad's MOUNT (the nearest vehicle with a seat, or out
	                               of its seats); regroup in a campaign game is the squad's FALL IN
	fireteam recruit [<bot>]       the host player's fireteam takes an order (attack, defend, watch and go at the host's
	                               feet) or a bot (its number in the logs, 1 up; none: the first of the team in another fireteam) from
	                               another fireteam; status logs the roles. In a campaign game with the AI squad the squad is
	                               the host player's fireteam and takes the orders the same way
	round end|next|live            a round mode's round (ELIMINATION: ae_round.h): end it now (the side with the most
	                               standing wins it, level is a tie), end it tied and start the next at once, or end
	                               its countdown now (live)
	zone now|small|off             the round's danger zone: its first closing starts now, it stands at its small ring
	                               now (a tenth, the last closing in its wait), or none this match
	zone look a|b                  how this machine draws the zone's edge for this run: a the energy wall, b the light
	                               curtain (game.zone_look is the setting)
	down <player> [here]           a living player goes down (dies, a revive spot where they fall); here: put beside
	                               you first. <player>: me, mate (the nearest teammate), enemy, dummy (the nearest dummy), an absolute index, or the
	                               start of a name
	revive <player>                a downed player back at their spot at once (mate: the nearest downed teammate)
	flag ours|theirs here|ahead|stand|carry|home
	                               CTF, a local game: the host's team's flag (ours) or the other's (theirs) dropped loose at
	                               the host's feet, 3 units ahead or at its own stand (as if its carrier died there), held by the host (carry,
	                               theirs) or by the nearest enemy (carry, ours: a dummy or runner first, waited for while it is
	                               still on its way to its spot), or back on its stand
	god [me|<player>|off]          that player takes no damage (me when none named; off ends it; their vehicle still does; works in a campaign level)
	unseen [me|<player>|off]       the bots neither see nor target that player, nor count them as an enemy near (me when
	                               none named; off ends it); a bot they hit reacts to the hurt (cover,
	                               unscoping) but never turns on them. In a campaign level the level's AI
	                               sees no player at all (off ends it)
	mods list | on <name> | off <name> | order <name> <1-256>
	                               the mods folder's mods (ae_mods_list.h): the list in one line (top first: priority 1
	                               plays over every other), a mod (its folder or mod.toml id) turned on or off, or moved to a
	                               priority; kept in load_order.toml and played at once when the map takes mods
	maps list [<source>:] [<text>] | missing | scan | delete <map>[@<pack>] | move <map>[@<pack>] <folder>|.
	                               the installed maps library: the maps whose name, title or pack has each word (a source
	                               first keeps only its maps: stock ce xbox downloaded pack mcc workshop); the Custom
	                               Edition and MCC files found or missing; a new scan; a Custom Edition or downloaded map
	                               deleted, or moved to a folder (. its top); one answer line, a refusal's reason
	campaign_score status|on|off|reset
	                               (1.0.x) the campaign's score (game.campaign_score): status answers its points, kills, deaths,
	                               time and par; on / off set the setting; reset empties this level's log. Each award is logged
	                               ("score: player 0, killed ..., +25")
	campaign_score add <enemy> <style>
	                               one kill of that enemy type (elite, jackal, grunt, hunter, engineer, assassin, combat_form,
	                               infection_form, carrier_form, sentinel; marine and crew are allies: a penalty) scored for
	                               player 1 with that style (regular, one_shot, headshot, melee, backsmack; joined with +), as
	                               a real kill is: needs game.campaign_score on and a campaign level
	inject status|maps|open <level>|list <category>|take <category> <number|name>|body <own|campaign|taken> <spartan|own>|close|on|off|clear
	                               tags taken out of a Custom Edition map (its file's name, in maps\ce) or an MCC map
	                               (its identity, <name>@mcc or <name>@ws<item id>) into the profile ae_dev: open a
	                               map, list a category (0 weapons, 1 AI, 2 equipment, 3 vehicles, 4 bipeds, 5
	                               scenery, 6 devices, 7 skies, 8 hands; the paths go to the log), take one by its
	                               number or a part of its path; on adds the profile's tags to each level loaded after (a biped taken is
	                               the player's), off stops that, clear empties the profile; body makes the profile a set whose YOUR BODY is the biped last
	                               taken, with the hands (own, campaign, or the hands last taken) and the movement (spartan or own) the set's file keeps
	profile_move list | sample [<name> [<games>]] | export <name> | preview|import <file> <name> [merge|replace] | undo <name>
	                               a profile's progress file (PROFILES > EXPORT / IMPORT PROGRESS): list the exports folder's
	                               files; sample: a made-up profile's file (never the record files) to import; export: the
	                               named profile of this PC to a file; preview: what an import would do; import: it, with a
	                               backup (merge keeps the record with more games, replace the file's); undo: the last
	                               import of this run put back. menu profiles progress export|files|import stages the screens
	randomizer status|on|off|seed <n>|reroll|save|pack
	                               the campaign randomizer (game.randomizer): status says what the loaded level was
	                               given; on, off and seed <n> (0: a new roll each level) set the settings for the next
	                               level; reroll forgets the seed and the roll; save writes the level's saved game as SAVE AND QUIT does (a
	                               campaign level played alone); pack queues the tag pack's writing now (status says how far)
	seat <driver|gunner|passenger> [<player>]
	companion [<1-4>|off|reset|kill|dry|down|revert|won] (campaign level) that many allied marines (2 by default) made beside the host: they follow the players
	                               (hold 3-6 wu away, catch up past 8) and fight with the level's own AI; off sends them away, reset keeps them and puts
	                               them beside the players, kill kills their bodies (they come back), dry empties the
	                               weapons they carry (loaded and reserve), down kills the human players (PLAYER DOWN);
	                               revert and won
	                               send the level back to its checkpoint / on to the next level, as the scripts do
	campaign_weapons status|on|off|share <0-100>|squad standard|ae|goto [<n>] (campaign level) AE weapons swapped into the level (game.campaign_ae_weapons):
	                               status says what the level was given; on, off and share set the next level's; squad sets the AI squad's
	                               gear (standard or ae) for the bodies that spawn next; goto puts you 2 steps from the nth swapped weapon (else the next with room) on
	                               the ground, facing it
	campaign <1-10> [<0-3>] [won <seconds>] (from the menus) the level starts alone at the difficulty (0 easy to 3 legendary, 1 by default),
	                               as the CAMPAIGN screen's A does; won: the level is won that many seconds into it, as its end does
	intro play                     (a TEST RUN campaign test's setup line) the test plays the level's intro: it loads the level again and
	                               does not open at the level's start point (ae_test_start.h); a campaign test without it opens at first control
	ride [warthog|ghost|scorpion] [circle|line]
	                               puts the host on top of the nearest one of that kind, making one when none exists
	roundcard won|lost|tied [reason]
	                               the ROUND WON / LOST card (ae_round_card.h) on every local view for 3 s with sample data
	                               (RED 2, BLUE 1, MVP Kestrel with 3 kills, round 4 of 7); reason: red, blue, bomb, defused,
	                               time, exterminat, standing or level picks the reason line (none: the verdict's usual one)
	record_medals <0-118> [<seed>] (0.8.1) SERVICE RECORD opened on a made-up player with that many medal kinds earned,
	                               random counts from the seed (0-99999999, 0 by default): a record of its own,
	                               never written; the record file is not touched. In the menus or a game.
	                               [page 1-7]: 1-3 the record's pages, 4-7 the detail window's MODES, COMBAT,
	                               WEAPONS & MAPS or MEDALS over it (botrecord page takes the same); [kinds <1-7>]:
	                               the games spread over that many game kinds (three by default); [ring <kind>]
	                               (slayer, flag, ball, hill, race, bomb, other) rings that kind's column on MODES
	lobby_fill <1-4>               (menus phase 1) the main menu's party at that size: player 1 and test guests on the
	                               controllers after (signed in as guests, no pad needed), the rest signed out
	party code | party online <status|start|stop|autojoin [off]|steam fake|send|list|off|name <text>|invite <code>|answer <n> [no]|leave|kick <code>|promote <code>|
	      privacy <invite|friends|closed>|link <text>|newlink|seats <1-4>|follow server|hosted|lan <address>|
	      quickplay <playlist> [<address>=<ms>,...] [again]|cancel>
	                               (online party) this install's friend code; the online party service's link driven from the
	                               console (network.party_service must name a service; `config network.party_service <url>`):
	                               every call answers at once with "queued", the service's answer is the `last:` of status
	motd_reset                     MESSAGE OF THE DAY forgets what was seen (ae_motd.txt): the next main menu opens it first
	motd_offline                   MESSAGE OF THE DAY asks the master nothing until the game restarts (its saved message,
	                               else the release's notes)
	camera third|first [<1-4>]     (rc2) the dev third-person camera on or off for a view (1 when not given), as the D-PAD
	                               DOWN key does it, in a running game (the debug.dev_camera setting is read only at a start)
	hitch <50-5000> [after <1-600>]
	                               (0.9.0) this machine's game stops that many milliseconds once, a long frame's hitch
	                               (CLOCK SLEW's TEST RUN check), now or that many seconds later in a game (a TEST RUN's
	                               joiner sets it before it joins); a client of a network game may run it, the only one
	menu game_options <filter> [change <option>=<value> ...]
	                               (0.9.3) CREATE GAME's GAME OPTIONS opened on the gametype being edited (else the last
	                               used one), each change applied to its copy (change kills_to_win=25 change
	                               time_limit=10: the row's name without case, spaces or "_"; the value's text, or a number
	                               alone for a text that starts with it), and the chip on (all, changed, classic, community,
	                               ae, classiccomp); up to three changes. Logs "dev: menu game_options <filter> opened, N changed"
	join_lan                       (0.9.0) from the menus: search the LAN and join the first game found, as JOIN LAN
	                               GAMES would (a TEST RUN's joiner joins the host's game by its setup line)
	menu settings [player <1-4>] find [<text>]
	                               (0.9.3) SETTINGS' top level, for that local player (1 by default), with <text> in FIND A
	                               SETTING's field (the field has the focus, the keyboard closed; no text: the field
	                               empty). From the menus, or over a SETTINGS that is open already; answers with how many
	                               settings were found
	social exp <id>|all <0-99999> [player <1-4>]
	                               (0.9.3) the offline social career: that playlist's EXP (all: every playlist's) set for that
	                               local player's profile (1 by default); career.txt is written
	social reset [<id>|all] [player <1-4>]
	                               the profile's EXP, hidden rating and pending game of that playlist (all or none named: every
	                               playlist) taken away
	social match <id> <0-4999> | tier <1-19> [player <1-4>]
	                               the hidden match rating, in points or at the start of a bot tier's level
	social mode <id> auto|recruit|easy|normal|heroic|elite|legendary|demon
	                               the playlist's bots: AUTO (matched by the hidden rating) or one skill for all; this run
	social roster <id> [seed <0-99999999>]
	                               a roster drawn as AUTO would (or the pinned skill), logged ("ae social: roster ...") and sent
	                               as the social_roster event; no game
	social pending <id> [crashed] [player <1-4>]
	                               writes a pending game for the profile, as a game not ended before a restart: found at the next read of
	                               career.txt and counted as a quit (-1 EXP, logged); with crashed the crash mark is written too, so
	                               the game is forgiven (logged, nothing counted)
	menu profiles [edit [colours|plain]]
	                               PROFILES opened; edit: the first profile's EDIT page over it (a PC with no profile:
	                               one named DEV is made first); colours: its colours have the focus, on the one
	                               after the profile's; plain: drawn as on a PC with no Spartan model. From the menus
	menu tournament [focus <id>] [party]
	                               (0.9.3) AE TOURNAMENT opened, the playlist <id> focused; party: the PARTY manager open over it (rc5). From the menus
	menu matchmaking [online] [focus <id>]
	                               MATCHMAKING (online only). `menu matchmaking social ...` is the old form of `menu tournament`
	menu career [player <1-4>] [focus <id>]
	                               the social CAREER screen of that local player (1 by default), the playlist <id> focused
	menu campaign [seat | players <2-4>] [error] [workshop [demo|fault|none|saved|damaged]] [save <1-10> <0-3>] [focus skip|continue|squad|down|dialog] [difficulty <0-3>]
	                               (0.9.4) CAMPAIGN (replacing an open one); seat: player 2 seated as a guest on controller 2,
	                               players: that many players (guests on controllers 2 up), so a level starts split-screen
	                               co-op; error: the co-op start's error dialog shown; workshop: the first installed custom
	                               campaign shown, its first mission focused (a made-up one, marked so, when none is
	                               installed; demo: the made-up one always), fault: a made-up fault line focused, none: no
	                               custom campaign, the line saying where they come from focused; saved: the made-up one
	                               with a made-up saved game, its CONTINUE focused; damaged: its saved game cannot go on
	                               (CONTINUE says why). save: a made-up saved game of that level and difficulty on the stock campaign; focus: the first row of
	                               that kind focused (dialog: CONTINUE focused and its SOLO ONLY dialog open); difficulty: the screen's difficulty. From the menus
	nav dump|status                (1.0 BOT NAV, ae_bot_nav.h) dump: the loaded map's ground for the nav tools into the
	                               config folder's bot_nav_geo/<map>.json (local only); status: the map's nav file, where it
	                               came from and its counts
	netext status                  this machine's network capability names and, per connected machine, the names it told
	nav show [off|routes|all]      (1.0 BOT NAV, ae_bot_nav_overlay.h) the bots' vehicle navigation drawn in the world (setting
	                               debug.bot_nav_show): routes: the nav file's routes; all (the default): also the obstacle
	                               rings, closed ground, drive troubles and each driver's path; off
	playlists demo|forget|real     (0.9.4) CREATE GAME's PLAYLISTS staged for a TEST RUN, held in memory (the player's own
	                               playlists.txt and playlist.txt are never read or written meanwhile): demo: 3 playlists (4V4
	                               ARENA, shuffle, 7 games, in use; FFA NIGHT, random, 6; SWAT TIME, in turn, 4); forget: the
	                               migration's scene (no playlists.txt, one old-style playlist.txt: it shows as MY PLAYLIST, not
	                               in use); real: the files again
	menu changemap [folder] [all] [version <n>] [<map words>]
	                               (rc5) CHANGE MAP on the filter ALL with the first map whose name has the words in it focused (none
	                               given: the map used last), or that map's folder row with `folder`. A map held in several copies
	                               is one row (its shown copy); `all` lists every copy as a row, `version <n>` puts the row's
	                               version choice on its n-th copy (1 = the shown one). From the menus
	menu playlist [edit <name>]    (0.9.4) CREATE GAME offline with the PLAYLISTS board open (a playlist's games when named: the
	                               name's words as typed, any case). From the menus
	menu coop [players <1-4>]
	                               (0.9.4) the CO-OP LOBBY: the first level hosted on this PC as a LAN game on NORMAL, that many
	                               players of this PC in it (guests on controllers 2 up). From the menus
	menu hostvote [lan] [now|join]
	                               (rc2) CREATE GAME hosting a lobby (this PC only; lan: on the LAN) with the playlist in use (stage it
	                               with `playlists demo`), its MAP VOTE opened as if a game had just ended: now at once, join when a
	                               second person is in the lobby, neither: the lobby only. Logs "map vote (host): ...". From the menus
	menu lan                       (0.9.4) LAN GAMES, searching this network. From the menus
	menu maps [installed|required|browse|workshop|mods] [<state>]
	                               (1.0) MAPS & MODS staged from made-up data (no files read or changed, nothing sent to the
	                               network), from the menus, on a page (INSTALLED by default) in a state: installed: list,
	                               options, folder, delete, in-use, loading, empty, unreadable; required: missing, found; browse:
	                               consent, results, download, offline, disk-full, bad-list, failed; workshop: list, signed-out;
	                               mods: list, moving, restart
	menu forge [hub|building|needs|maps|keys|mymaps]
	                               (rc9) the FORGE doors, from the menus: hub (the real hub, as the FORGE row opens it); the made-up
	                               scenes start and write nothing: building (the pack at 40 %), needs (the game's maps missing),
	                               maps (NEW MAP's list), keys (FORGE KEYS); mymaps: MY MAPS from this PC's saved maps, as they are. Only the 64-bit Linux build has Forge
	menu steam <state> [<percent>]
	                               (1.0) STEAM ACCOUNT staged with no Steam traffic, from the menus: signed-out, typing (a name
	                               and a password typed), guard-code, guard-app, missing (signed in, MCC's shared files not
	                               here), downloading [<1-100>] (37 by default), ready, workshop (a made-up page of items),
	                               wrong-password, not-owned, logout (the LOG OUT dialog open); first-start: the first-start
	                               page that offers the sign-in; open: the real screen on the real client, as SETTINGS opens it
	social start <id> [map <name>] [seed <n>] [now]
	                               the TOURNAMENT LOBBY of that playlist, its map the pick's unless one is named (<file> or
	                               <file>@smp), the roster seeded (the clock's when none); now: the game loads at once. From
	                               the menus
	social start <id> [map <name>] hold <0-30 | starting>
	                               (rc5) the lobby run that many seconds and then held there (its clock stops), so a card, a bar or the STARTING
	                               stage can be looked at (starting: one second into STARTING): no game loads. From the menus
	social start <id> vote [seed <n>] [votes <a,b,c>] [left <0-10>] [party <1-4>]
	                               the TOURNAMENT LOBBY of that playlist opened at its MAP VOTE: <left> seconds of the 10 left
	                               (all of it when none), the bots already voted <a,b,c> for the three maps (they vote by
	                               themselves when none), that many local players in the party (the signed in ones when none). Logged
	                               "ae social vote:"; event social_vote. From the menus
	social start <id> postgame [online] [still] [bots <0-7>] [left <0-20>] [votes <a,b,c>] [party <1-4>] [seed <n>]
	                               (rc5) the post-game lobby (the REMATCH) of that playlist on a made-up last game: its roster kept,
	                               <bots> of them in your party, the vote (20 s) with SAME AGAIN first; online: the fake online
	                               states (ping, STAYING, an open seat, an invite sent); still: the "Still there?" question up
	social party offline|online|toobig|full|empty|clear
	                               (rc5) MY PARTY of AE TOURNAMENT staged from made-up members (the online states too, no
	                               network); `menu tournament <id>` then shows it; clear: the real party
	social postgame win|tie|loss|rankup [<id>] [exp <n>] [player <1-4>]
	social postgame place <p> <n> [<id>] [exp <n>] [player <1-4>]
	                               a made-up social post-game (rankup: a win that crosses a rank; place p of n players in a free
	                               for all), the player's EXP before it n (the profile's own when none); career.txt is not
	                               touched. Each player's own command adds a view: player 2's after player 1's makes it split
	                               screen. A closes it. From the menus
	social recent show|clear <id|all>
	social recent set <id> <map,map,...>
	                               (rc5) the playlist's recent-maps memory (the maps its picks and map votes leave out): shown,
	                               cleared, or set to those maps, the newest first (<file>@smp as the playlist names it)
	social calibration <profile|me> unset|new|some|alot [player <1-4>]
	                               (0.9.4) the profile's calibration answer (me: that local player's profile) set, so a TEST RUN
	                               stages each answer; unset asks again at the next AE TOURNAMENT; career.txt is written. The
	                               hidden ratings stay as they are (CAREER's EXPERIENCE row moves them)
	item respawn <entry> in <1-3600>
	                               (1.0 rc2) a power item's next respawn: the item on its spot taken off the map, the entry (a part of its
	                               label, one word: rockets, sniper; or its number in the log's item timers lines) spawns that many
	                               seconds on and every period after; the timers, waypoint countdown and log run as in a game
	split force <2-4>              the request join makes, sent to the host with none of this machine's own checks first (the host's
	                               refusal, logged "split join: machine ..."). Answers that it was sent
	split card <2-4>               the join card a pad with no player gets from its START, opened for that controller (no pad needed)
	split pad <2-4>                that controller counts as a pad, so the pause menu offers ADD PLAYER on a PC with no second pad
	split join|leave <2-4>         (1.0) controller 2-4's split screen player joins the game in progress, or leaves it, as
	                               its pad's START or the pause menu's ADD PLAYER / REMOVE PLAYER do (no pad needed: the
	                               command stages it). Answers "split join refused: <why>" when the game would not take it
	                               (the gametype's JOIN IN PROGRESS: SPLIT SCREEN off, a full game ...). Logged "split join:"
	social maps hide smp|<file>|none
	                               this run only: the social playlists treat the pack's maps (smp), or one map (<file>, a stock
	                               map's name or a pack map's file), as absent; none shows them again
	weaponset casual|comp|btb|off|auto
	                               the weapon spawners' set from the next spawn (auto = the gametype's;
	                               debug.weapon_set is the setting)
	ammo <0-100>                   (rc2) the weapon in hand's rounds (loaded and total) set to that percent of its full ammunition
	spawner wait <1-600>|red <1-600>|ready
	                               (rc2) the weapon spawner nearest the first player (within 8 world units): its weapon
	                               gone and returning in that many seconds (the hologram fills as it counts), RED for that
	                               long (a POWER weapon taken and in play), or its next weapon now; this game only
	medal <key> [me|<player>]      the host gives that player the medal (its key in ae_medal_ids.h, with or without
	                               "medal_": double_kill) as its own rules do: counted for the postgame and shown on
	                               the player's machine (another machine's over the network). A host only; a player
	                               as god names one (me when none named)
	range [<profile>]              the firing range (Content Evolved's, firing_range.c) played alone from the menus: a plane
	                               on Blood Gulch's map with a mirror, an arena and a bench of each weapon and a stand of each AI
	                               the profile holds (a Tag Injector profile's folder, ae_dev when none is named: the one
	                               inject fills)
	skip_training status|on|off    the first level (a10) starts after its training, as in co-op: the setting game.campaign_skip_training for the
	                               next level (alone, on normal; status says the setting and how often the level was told heroic)
	pad status | pad world a|b|c|other|dualsense|keyboard|off | pad battery <0-100>|charging [<0-100>]|charged|wired|off |
	               pad mode wired|bluetooth|puck|off | pad hold on|off | pad reset | pad open settings|buttons | pad open controls [trackpad|extra|gyro] |
	               pad pulse <event>
	                               what the game takes each local player's pad to be (status: ids, world, connection, battery),
	                               and the staging of a scene without the pad: world a (Steam Input drives a Steam Controller), b (the
	                               game reads it), c (Linux has not given access), other (Steam Input, pad not known), dualsense,
	                               keyboard; the battery and the connection; hold keeps the controller notice on screen; reset clears
	                               it all; open shows SETTINGS, its CONTROLS page (the focus on that row) or the EXTRA BUTTONS screen (from the menus).
	                               pulse fires that event's motor pulse (pistol, rifle, shotgun, br, plasma, plasma-charged, mag-in, chamber, grenade, plasma-grenade, charge-half, charge-full) on that player's pad, as the game would, at its TRIGGER STRENGTH.
	                               Each takes a last "player <1-4>" (player 1 when not given)
	script_refuse status|off|<script>  the next MCC campaign level's script check refuses that script, as if its calls were not allowed (debug.script_refuse),
	                               so the notice of switched off scripts shows; status says the setting, the loaded level's count and whether the line was shown
	firefight start [<map>] [<variant>]
	                               a Firefight game of this machine alone on the map (else the map being played),
	                               from the main menu: the variant a built-in name (firefight, generator_defense,
	                               gruntpocalypse, rocketfight, floodfight, ...), "firefight" when none
	firefight wave <1-5>           the round's wave begun now (the countdown skipped); firefight clear: the wave's
	                               enemies gone, so it is cleared; firefight skull <name>: that Firefight skull on
	                               for the round (iron, mythic, grunt_birthday_party ...)
*/

#ifndef AE_DEV_CMD_RULES_H
#define AE_DEV_CMD_RULES_H

#define AE_DEV_CMD_WORDS 10
#define AE_DEV_CMD_WORD 64
#define AE_DEV_CMD_REST 192

enum ae_dev_cmd_id
{
	AE_DEV_CMD_NONE = 0,
	AE_DEV_CMD_GIVE,
	AE_DEV_CMD_SPAWN,
	AE_DEV_CMD_BOTS,
	AE_DEV_CMD_POWERUP,
	AE_DEV_CMD_RESPAWN,
	AE_DEV_CMD_KILL,
	AE_DEV_CMD_END,
	AE_DEV_CMD_SCORE,
	AE_DEV_CMD_TIME,
	AE_DEV_CMD_GOTO,
	AE_DEV_CMD_GAMETYPE,
	AE_DEV_CMD_OPTION,
	AE_DEV_CMD_MARK,
	AE_DEV_CMD_SHOT,
	AE_DEV_CMD_HELP,
	AE_DEV_CMD_LIST,
	AE_DEV_CMD_VIEW,
	AE_DEV_CMD_RESET,
	AE_DEV_CMD_CONFIG,
	AE_DEV_CMD_AGENT,
	AE_DEV_CMD_VIEWMODEL,
	AE_DEV_CMD_FIRETEAM,
	AE_DEV_CMD_ROUND,
	AE_DEV_CMD_ZONE,
	AE_DEV_CMD_DOWN,
	AE_DEV_CMD_REVIVE,
	AE_DEV_CMD_GOD,
	AE_DEV_CMD_SEAT,
	AE_DEV_CMD_RIDE,
	AE_DEV_CMD_TTK,
	AE_DEV_CMD_DUMMY,
	AE_DEV_CMD_HUD,
	AE_DEV_CMD_TUNE,
	AE_DEV_CMD_CALL,
	AE_DEV_CMD_ROUNDCARD,
	AE_DEV_CMD_FLAG,
	AE_DEV_CMD_RECORD_MEDALS,
	AE_DEV_CMD_LOBBY_FILL,
	AE_DEV_CMD_MOTD_RESET,
	AE_DEV_CMD_MOTD_OFFLINE,
	AE_DEV_CMD_HITCH,
	AE_DEV_CMD_JOIN_LAN,
	AE_DEV_CMD_CAMERA,
	AE_DEV_CMD_MENU,
	AE_DEV_CMD_BOTSKILL,
	AE_DEV_CMD_SOCIAL,
	AE_DEV_CMD_BOTSTYLE,
	AE_DEV_CMD_BOTRECORD,
	AE_DEV_CMD_PLAYLISTS,
	AE_DEV_CMD_NAV,
	AE_DEV_CMD_ITEM,
	AE_DEV_CMD_PGCR_DEMO,
	AE_DEV_CMD_UNSEEN, /* the bots neither see nor target a player */
	AE_DEV_CMD_NETEXT,
	AE_DEV_CMD_MODS, /* the mods list */
	AE_DEV_CMD_ARCHIVE,
	AE_DEV_CMD_MAPS, /* the installed maps library: list, required files, delete, move */
	AE_DEV_CMD_SPLIT,  /* a split screen player joins or leaves the game in progress */
	AE_DEV_CMD_BIPED, /* the biped packs: list, use, off, check */
	AE_DEV_CMD_CAMPAIGN_SCORE,
	AE_DEV_CMD_REPLAY, /* the recordings: list, status, info, and the playback's verbs */
	AE_DEV_CMD_INJECT, /* tags taken out of a Custom Edition or MCC map into the DEV profile, and added to levels */
	AE_DEV_CMD_RANDOMIZER, /* the campaign randomizer: status, on, off, seed, reroll */
	AE_DEV_CMD_PROFILE_MOVE, /* a profile's progress file: list, sample, export, preview, import, undo */
	AE_DEV_CMD_WEAPONSET, /* weapon spawners (ae_spawners.h) */
	AE_DEV_CMD_AMMO, /* the weapon in hand's ammunition, a percent */
	AE_DEV_CMD_SPAWNER, /* stage the nearest weapon spawner (ae_spawners.h) */
	AE_DEV_CMD_MEDAL, /* a medal given to a player as the host's rules give it */
	AE_DEV_CMD_PARTY, /* the online party service's link: code, online ... */
	AE_DEV_CMD_COMPANION, /* an allied squad of the campaign level's own AI, following the players */
	AE_DEV_CMD_CAMPAIGN, /* a campaign level started alone from the menus, as the CAMPAIGN screen's A does */
	AE_DEV_CMD_CAMPAIGN_WEAPONS, /* AE weapons in campaign levels and the AI squad's gear */
	AE_DEV_CMD_FIREFIGHT, /* firefight start [<map>] [<variant>]: a Firefight game of one machine on a map */
	AE_DEV_CMD_RANGE, /* range [<profile>]: the firing range played alone, with a Tag Injector profile's weapons and AI */
	AE_DEV_CMD_SKIP_TRAINING, /* skip_training status|on|off: the first level starts after its training (game.campaign_skip_training) */
	AE_DEV_CMD_SCRIPT_REFUSE, /* script_refuse status|off|<script>: the next MCC level's script check refuses that script (debug.script_refuse) */
	AE_DEV_CMD_PAD, /* pad status|world|battery|mode|hold|reset|open: what the game takes a pad to be, and a staged scene without the pad */
	AE_DEV_CMD_ACTOR, /* actor <variant> [<2-60>] | actor kill [blast]: an AI of the map's actor variant placed ahead, facing you, or those placed killed */
	AE_DEV_CMD_DOOR, /* door power [on|off]: the doors within 15 units of the host get power (they open for a body near them) or lose it */
	AE_DEV_CMD_INTRO, /* a TEST RUN campaign test's setup line: the level's intro plays (ae_test_start.h) */
	AE_DEV_CMD_PLAYER, /* player route <x y z [jump] [crouch] [walk <0.1-1>] [teleport] [save] [revert]>... | go | look <deg> | stop: the host's player walks a route by itself */
	AE_DEV_CMD_POINT, /* point save|load <name>: the level's state saved to a named file, or loaded from it (a test's start) */
	AE_DEV_CMD_FORGE, /* forge start <map> | mode monitor|play | status: a Forge session on a level, the Monitor entered, what is placed */
	AE_DEV_CMD_JOIN_CLOSED, /* join_closed [<1-4>]: a join refused as closed (code 5, a host changing maps) staged that many tries running, in the menus */
	AE_DEV_CMD_COUNT
};

/* view bots <n>: a bot's number, as the logs' "bot N" (bots.h's BOTS_BRAINS) */
#define AE_DEV_CMD_VIEW_BOTS_MOST 127

struct ae_dev_cmd
{
	int id;                  /* enum ae_dev_cmd_id; AE_DEV_CMD_NONE when the line was not a dev command */
	int words;               /* the arguments after the command's name */
	char word[AE_DEV_CMD_WORDS][AE_DEV_CMD_WORD];
	char rest[AE_DEV_CMD_REST]; /* the line after the name (gametype, mark), after the first argument (config), after
	                             "say" or after "check <n> <verdict>" (agent), after "settings find" (menu) */
	long number;             /* bots' count, score's points, time's seconds, goto start's index, dummy's count, hud's element,
	                            record_medals' kinds, menu's local player (0-3) */
	unsigned long seed;      /* record_medals' seed; hitch's seconds after (0: now) */
	float position[3];       /* goto <x> <y> <z>; dummy's distance and here (1 or 0); hud's dx, dy and scale (0: not given);
	                            dummy walk's and dummy run's start; record_medals' page, counted and kinds */
	int has_position;        /* (dummy walk: 1 at x y z, 0 here) */
	float goal[3];           /* dummy walk's and dummy run's goal */
	int has_goal;
	long times;              /* kill me by ... after|every: the kills in all (1 for after); dummy run: 1 when it loops */
	int away;                /* dummy's last word "away": 1 */
	char error[96];          /* "" or why the line cannot run (a usage line) */
};

/* the medal (ae_medals.h's number) whose key in ae_medal_ids.h is key, with or without its "medal_", any case; -1 for
none */
int ae_dev_cmd_medal(const char *key);

/* parses a line. 0: not a dev command (the caller hands it to the script evaluator); 1: a dev command, in out, whose
error is "" when it can run. bare: names without "dev" are taken (DEV MODE on) */
int ae_dev_cmd_parse(const char *line, int bare, struct ae_dev_cmd *out);

/* where a powerup line's item goes: at the host's feet (picked up as they stand there), lying ahead of the host, or as
its effect on a player, whose word is then in *player (NULL otherwise) */
enum { AE_DEV_POWERUP_FEET, AE_DEV_POWERUP_AHEAD, AE_DEV_POWERUP_PLAYER };
int ae_dev_cmd_powerup_place(struct ae_dev_cmd const *cmd, const char **player);
/* whether a parsed line runs in a campaign level's tick, which has no game engine: the companions, a move, a vehicle, a
seat, god, the test player, a saved point, and a powerup only as the item lying ahead of the host; every other form needs a
multiplayer engine */
int ae_dev_cmd_runs_in_campaign(struct ae_dev_cmd const *cmd);

/* a command's name ("give") and its usage line; "" for an id out of range */
const char *ae_dev_cmd_name(int id);
const char *ae_dev_cmd_usage(int id);
/* the command whose name is word (any case), AE_DEV_CMD_NONE when none is */
int ae_dev_cmd_find(const char *word);

/* the bare command names (not help) that start with prefix (any case), in table order, up to max into out; the number found */
int ae_dev_cmd_complete(const char *prefix, const char **out, int max);

/* who may run a command: DEV MODE's setting on this machine, and this machine a network game's client */
enum ae_dev_cmd_gate
{
	AE_DEV_CMD_ALLOWED = 0,
	AE_DEV_CMD_SETTING_OFF, /* system.dev_mode off here */
	AE_DEV_CMD_CLIENT       /* another machine hosts this game: its rules hold, never a client's */
};
/* whether the command may run here: help always; every other command only with the setting on and not as a client */
int ae_dev_cmd_gate(int id, int setting, int client);
/* the line said for a gate's refusal ("" for AE_DEV_CMD_ALLOWED) */
const char *ae_dev_cmd_gate_text(int gate);
/* while a replay plays its game is as recorded: what a command does then. ALLOWED (replay and help), NOTHING (reset:
answered done with nothing changed, so a TEST RUN's reset between tests leaves the playback alone), or REFUSED (any
other, answered with ae_dev_cmd_replay_refusal) */
enum
{
	AE_DEV_CMD_REPLAY_ALLOWED = 0,
	AE_DEV_CMD_REPLAY_NOTHING,
	AE_DEV_CMD_REPLAY_REFUSED
};
int ae_dev_cmd_during_replay(int id);
const char *ae_dev_cmd_replay_refusal(void);

/* seconds from "90" or "1:30": 1 and *seconds, or 0 when the text is neither (or past 9999) */
int ae_dev_cmd_seconds(const char *text, long *seconds);
/* "a,b,c": three whole numbers 0 .. 16 (social start's `votes`: the bots' votes for each of the three maps); 1 when it reads,
   the numbers into votes (NULL: only checked) */
int ae_dev_cmd_votes(const char *text, int *votes);

/* `seat <kind>`: whether a seat (its driver and gunner flags) is one of the kind asked ("driver", "gunner", anything else
a passenger's). A gunner's seat is never the driver's too (a ghost's one seat drives and fires: "seat gunner" puts no one
at a ghost's wheel); a passenger's is neither */
int ae_dev_cmd_seat_wanted(const char *kind, int driver, int gunner);

/* THE TEST PLAYER (player route): the host's own player walks a route of points by itself, its controls given as a pad's
would be: it faces the next point and walks to it at full speed times the point's walk, jumps as it comes within
AE_DEV_PUPPET_JUMP_NEAR of a jump point, holds crouch while it walks to a crouch point. A point is reached within
AE_DEV_PUPPET_REACHED across and AE_DEV_PUPPET_RISE up or down. */
#define AE_DEV_PUPPET_POINTS 32
#define AE_DEV_PUPPET_REACHED 0.3f
#define AE_DEV_PUPPET_RISE 1.0f
#define AE_DEV_PUPPET_JUMP_NEAR 0.6f
struct ae_dev_puppet_point
{
	float x, y, z;
	float walk; /* 0.1-1: the share of full speed */
	int jump;
	int crouch;
	int teleport; /* put there at once, not walked */
	int save;     /* reached: the level's state saved (a checkpoint) */
	int revert;   /* reached: the level reverted to its last save */
	float wait;   /* reached: seconds stood there (0.5-60), 0 none */
	int faced;    /* the wait faces face (degrees, world), not the way it came */
	float face;
};
struct ae_dev_puppet_route
{
	int count;
	struct ae_dev_puppet_point point[AE_DEV_PUPPET_POINTS];
};
/* appends text's points ("x y z [jump] [crouch] [walk <0.1-1>] [wait <0.5-60>] [face <degrees>] [teleport] [save] [revert]" each) to the route: how many, -1 when the text is bad or
the route would hold more than AE_DEV_PUPPET_POINTS (the route is then as it was) */
int ae_dev_puppet_parse(const char *text, struct ae_dev_puppet_route *route);
/* a step toward point (tx, ty, tz) from (px, py, pz): 1 when it is reached; else 0, *yaw (radians, world) toward it and the
throttle (*throttle_x ahead, *throttle_y to the left) at walk */
int ae_dev_puppet_step(float px, float py, float pz, float tx, float ty, float tz, float walk, float *yaw, float *throttle_x,
	float *throttle_y);

/* a saved point's name (point save|load): letters, digits and '-', 1 to AE_DEV_POINT_NAME long */
#define AE_DEV_POINT_NAME 24
int ae_dev_point_name_ok(const char *name);

#endif
