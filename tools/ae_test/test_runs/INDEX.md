# TEST RUN lists

Written by `tools/ae_test/list_index.py --write`; do not edit by hand. Look here (or run
`list_index.py --find WORD...`) for a feature's list before writing a new one, and copy the lists into a test
install with `list_index.py --sync <install>/test-runs`.

## 073-batch: 0.7.3 BATCH, ROUND 1

File `073-batch.txt`, 8 tests.

0.7.3 BATCH, on testbox (its weapons and vehicles are on it): pickup text under KILL FEED, MCC style's long captions, AE's own medal

- `pickup-feed-on`: KILL FEED on: no pickup text
- `pickup-feed-off`: KILL FEED off: the old list, whole
- `mcc-captions`: MEDAL STYLE MCC: long captions
- `own-pictures`: Pictures for medals MCC lacks
- `own-pictures-modern`: UNIVERSAL: the same pictures
- `no-mcc-pictures`: MCC not used: names only
- `straight-balling`: Straight Balling: once a hold
- `seat-roll`: The seat panel past 3 seats

## 074-batch: 0.7.4 BATCH, ROUND 1

File `074-batch.txt`, 10 tests.

0.7.4 BATCH, on testbox (its weapons and vehicles are on it): the item prompt under the reticle and in the split views, the pad buttons'

- `item-prompt-one`: Item prompt, one view, HUD STYLE AE
- `item-prompt-size`: WAYPOINT SIZE 50 and 100 %
- `item-prompt-empty`: Out of ammo: switch reminder
- `item-prompt-split`: Item prompt, 2 views
- `item-prompt-quarter`: Item prompt, 4 views
- `item-prompt-classic`: HUD STYLE CLASSIC: today's line
- `mcc-captions-small`: MCC captions in a 720 line view
- `pad-glyphs-one`: Pad buttons, one view, both HUD STYLES
- `pad-glyphs-dpad`: Stick and d-pad hints
- `pad-glyphs-split`: Pad buttons, 2 views on two pads

## bots-090: BOTS 0.9.0: DRIVING, NORMAL, ELITE

File `bots-090.txt`, 3 tests.

The 0.9.0 bots in one short round. Three set-ups, four checks:

- `normal`: BLOOD GULCH: NORMAL bots, fight them up close with your shields down
- `elite`: BLOOD GULCH: ELITE bots, play a few fights
- `drive`: SNOW GROVE: watch the bots drive (two minutes)

## bots-balance-090: BOTS: EVERY LEVEL EVENLY SPACED (TEMPLE 2V2)

File `bots-balance-090.txt`, 4 tests.

Bot balance pass (0.9.0): every level from EASY to DEMON is evenly spaced. Each step up reacts, aims, settles and turns

- `temple-easy`: TEMPLE 2V2: EASY bots, you and one bot against two
- `temple-heroic`: TEMPLE 2V2: HEROIC bots, you and one bot against two
- `temple-elite`: TEMPLE 2V2: ELITE bots, you and one bot against two
- `temple-legendary`: TEMPLE 2V2: LEGENDARY bots, you and one bot against two

## bots-drive-090: BOTS DRIVING: CLASSIC / PLANNED

File `bots-drive-090.txt`, 6 tests.

0.9.0 PLANNED DRIVING: bots at the wheel plan their speed ahead of corners (slower the sharper and nearer the turn) and of

- `snow-classic`: Snow Grove: bots drive on the old rules
- `snow-planned`: Snow Grove: bots drive on the new rules
- `gulch-classic`: Blood Gulch: bots drive on the old rules
- `gulch-planned`: Blood Gulch: bots drive on the new rules
- `sidewinder-classic`: Sidewinder: bots drive on the old rules
- `sidewinder-planned`: Sidewinder: bots drive on the new rules

## bots-elite-090: BOTS: ELITE BETWEEN HEROIC AND LEGENDARY

File `bots-elite-090.txt`, 9 tests.

Bot level ELITE (0.9.0): HEROIC, ELITE and LEGENDARY side by side, the same map and the same two bots against you, one level a

- `temple-heroic`: TEMPLE: two HEROIC bots against you
- `temple-elite`: TEMPLE: two ELITE bots against you
- `temple-legendary`: TEMPLE: two LEGENDARY bots against you
- `sali-heroic`: SALI CREEK: two HEROIC bots against you
- `sali-elite`: SALI CREEK: two ELITE bots against you
- `sali-legendary`: SALI CREEK: two LEGENDARY bots against you
- `octagon-heroic`: OCTAGON: two HEROIC bots against you
- `octagon-elite`: OCTAGON: two ELITE bots against you
- `octagon-legendary`: OCTAGON: two LEGENDARY bots against you

## bots-fix-078: BOTS: THE FIX ROUND

File `bots-fix-078.txt`, 6 tests.

The bot fix round of 0.7.8 (owner run 2 of bots-vantage-078): NEED A RIDE from a damaged vehicle's seat, fireteams split by

- `free-team`: With no order your squad plays on its own
- `regroup`: REGROUP brings them and they keep firing
- `regroup-again`: REGROUP again lets them play free
- `ride-damaged`: NEED A RIDE from a damaged warthog
- `tanks-holds`: Tanks and high ground in Slayer
- `arena-near`: Arena: your squad stays loosely near

## bots-gaps-090: BOTS: SPAWN GRACE, WEAPON BY RANGE, HIGH GROUND

File `bots-gaps-090.txt`, 4 tests.

0.9.0 bot round: four changes to how the bots play.

- `spawn-grace`: HEROIC bots don't wait after you spawn
- `high-ground`: Bots with rifles hold the high spots a while
- `teleporter-fight`: A bot fighting you stays in the fight
- `weapon-range`: Bots pick sniper rifle or pistol by distance

## bots-powerup-090: BOTS: SHOOTING DOWN HUNG ITEMS

File `bots-powerup-090.txt`, 4 tests.

0.9.0: bots shoot down hung items (powerups, a better weapon, a health pack while hurt). Digsite's workshop maps (Steam item 3477809506) hang some powerups in the air;

- `hotbox-camo`: A bot shoots down Hotbox's hung camouflage
- `hotbox-fight`: A bot in a fight leaves the powerup alone
- `gothra-both`: Gothra's hung camouflage and overshield come down
- `chillout-sword`: A bot shoots down Chill Out's hung sword

## bots-score-arena: BOT SCORECARD: ARENA

File `bots-score-arena.txt`, 4 tests.

Bot scorecard, arena 4v4 (owner 11-07: arena tests never on Blood Gulch): Temple and Sali Creek, AE TEAM SLY, 7 bots.

- `temple-1`: The bots play
- `temple-2`: The bots play on
- `sali-1`: The bots play
- `sali-2`: The bots play on

## bots-score-btb: BOT SCORECARD: BIG TEAM

File `bots-score-btb.txt`, 5 tests.

Bot scorecard, big team: Blood Gulch, AE BTB SLY, 15 bots. Headless only (the box), not an owner list:

- `play-1`: The bots play
- `play-2`: The bots play on
- `play-3`: The bots play on
- `ride-1`: NEED A RIDE
- `ride-2`: NEED A RIDE from the gunner's seat (a bot comes to drive)

## bots-score-canyon: BOT SCORECARD: DANGER CANYON

File `bots-score-canyon.txt`, 4 tests.

Bot scorecard, a map's stuck spots: Danger Canyon, AE TEAM SLY, 11 bots (the servers' worst stuck spot, 11-12: bots in

- `canyon-1`: The bots play
- `canyon-2`: The bots play on
- `canyon-3`: The bots play on
- `canyon-4`: The bots play on

## bots-score-ctf: BOT SCORECARD: CTF

File `bots-score-ctf.txt`, 4 tests.

Bot scorecard, objective: Blood Gulch, AE BTB CTF, 15 bots (tanks on a long trip, flag carriers, seats). Headless only:

- `play-1`: The bots play
- `play-2`: The bots play on
- `play-3`: The bots play on
- `play-4`: The bots play on

## bots-score-drive: BOT SCORECARD: DRIVING

File `bots-score-drive.txt`, 4 tests.

Bot scorecard, driving: Snow Grove (a Custom Edition map with vehicles), AE BTB SLY, 15 bots, bots drive (debug.bot_vehicles 2).

- `drive-1`: The bots play
- `drive-2`: The bots play on
- `drive-3`: The bots play on
- `drive-4`: The bots play on

## bots-score-stuck: BOT SCORECARD: STUCK SPOTS

File `bots-score-stuck.txt`, 4 tests.

Bot scorecard, a map's stuck spots: Blamite Mines, AE TEAM SLY 4v4, 7 bots (the 11-11 Comp trap: blue bots stuck low).

- `mines-1`: The bots play
- `mines-2`: The bots play on
- `mines-3`: The bots play on
- `mines-4`: The bots play on

## bots-tank-1: BOTS IN THE TANK, ROUND 1

File `bots-tank-1.txt`, 3 tests.

Bots in the Scorpion: its driver drives it and fires its cannon and chain gun. Only a map's tanks; it drives on the open maps.

- `tank-board`: A bot takes the tank
- `tank-guns`: Cannon and chain gun
- `tank-aim`: Turret and hull

## bots-vantage-078: BOTS: VANTAGE SPOTS AND SIDE LANES

File `bots-vantage-078.txt`, 5 tests.

Bot vantage spots and side lanes (0.7.8): bots with the sniper rifle or a rocket

- `gulch-plan`: The map's spots and lanes are found
- `gulch-holds`: Bots hold the overlooks
- `gulch-lanes`: Squads walk the side lanes
- `follow-me`: FOLLOW ME pulls a holder off its spot
- `prisoner-none`: Indoors: no holds, no stalls

## camera-smooth-081: CAMERAS: DEATH HOLD, AIM FOLLOW, GLIDES

File `camera-smooth-081.txt`, 2 tests.

Death and follow cameras (0.8.1, owner 11-11, after play-testing: "Look at the killer, Start

- `death-hold`: Dying: eyes out to your body, your killer in view
- `follow-switch`: Following: LB and RB switch with a quick fade

## combined-076a: 0.7.6 COMBINED, ROUND A

File `combined-076a.txt`, 13 tests.

0.7.6 COMBINED TEST BUILD, round A (test/combined-076): fireteams, bot driving, the combat AR's viewmodel, the lobby,

- `lobby`: LOBBY: invite, playlist, guest guard
- `map-folders`: CHANGE MAP: foldable sections
- `roles`: FIRETEAMS: squad roles and the anchor
- `follow-me`: FIRETEAMS: FOLLOW ME
- `regroup-defend`: FIRETEAMS: DEFEND, then REGROUP
- `drive-gulch`: BOT DRIVING: Blood Gulch
- `combat-ar`: COMBAT AR: same place as the stock AR
- `moderation`: HOST MODERATION: the player card
- `pgcr`: POST-GAME REPORT: screen, record, rival
- `round-flow`: ELIMINATION: a round, frozen start, then live
- `wipe-win`: ELIMINATION: a wiped team loses the round
- `revive-hold`: ELIMINATION: you revive a teammate
- `smoke`: SMOKE GRENADE: cloud and bot sight

## comms-1: TEAM CALLS, ROUND 1

File `comms-1.txt`, 22 tests.

TEAM CALLS, the first round: the smart button, the wheel, prompts, markers, the feed and the seat panel. A controller on AE's

- `smart-foot`: The smart button on foot
- `double-tap`: A double tap calls an enemy
- `wheel-pad`: The wheel on the controller
- `wheel-mouse`: The wheel on keyboard and mouse
- `wheel-interrupted`: Things that close an open wheel
- `wheel-dead`: The wheel when dead
- `feed-kills`: The feed shares the list with kills
- `bots-place`: Bots go where you call
- `bots-enemy`: Bots check a marked enemy
- `bots-fetch`: Bots fetch and ride
- `markers`: Markers and their rows
- `place-names`: Marks name the place
- `power-timer`: A mark on an empty power spawn
- `seat-panel-driver`: The seat panel while driving
- `vehicle-marks`: Marks from and on a vehicle
- `vehicle-enemy`: An enemy's vehicle
- `flag-facts`: Flag facts
- `timers-off`: With TIMERS off, no spawn time shows
- `split-2`: Two players on one PC, keyboard is player 2
- `walls-off`: ENEMY MARKS THROUGH WALLS, off
- `split-4`: Four players on one PC
- `earlier-checks`: Checks still open from the first marks

## comms-2: TEAM CALLS, ROUND 2

File `comms-2.txt`, 4 tests.

TEAM CALLS, the fixes of round 1, played for the first time: the prompt by the reticle, the driver's crosshair, a marked

- `prompt-still`: The prompt holds still by the reticle
- `driver-crosshair`: The driver's crosshair
- `marked-vehicle`: A marked vehicle gets a driver
- `enemy-vehicle`: An enemy's vehicle

## comms-3: NEED A RIDE

File `comms-3.txt`, 3 tests.

NEED A RIDE fetches a driver. Blood Gulch, bots driving; the wheel's NEED A RIDE. Host only: a client's call is by hand.

- `ride-driver`: On foot, a teammate driving near
- `ride-passenger`: In a seat with no one driving
- `ride-empty`: On foot, no one driving

## dev-camera: DEV MODE CAMERAS

File `dev-camera.txt`, 7 tests.

DEV MODE's cameras: F2 shoulder camera, F3 other shoulder, F6 free camera.

- `shoulder-toggle`: F2 turns the shoulder camera on and off
- `other-shoulder`: F3 swaps the shoulder
- `wall-pull-in`: The camera pulls in at a wall
- `distance-300`: system.dev_camera_distance 300 is farther
- `free-camera`: F6 free camera
- `zoom-gives-back`: A zoom gives the game's camera back
- `death-gives-back`: A death gives the game's camera back

## dev-hit-markers: DEV MODE HIT MARKERS

File `dev-hit-markers.txt`, 6 tests.

DEV MODE's hit markers: brackets round the reticle when your damage lands (red on a kill).

- `hit-white`: A hit shows white brackets
- `kill-red`: A kill shows red, larger brackets
- `grenade-melee`: Grenades and melee show markers
- `fits-reticle`: The brackets fit each weapon's reticle
- `no-self-hit`: Your own damage shows no marker
- `shoulder-camera`: Markers still show in the shoulder camera

## devtest-sample: DEV TEST SAMPLE

File `devtest-sample.txt`, 5 tests.

DEV MODE's sample grouped list: one Blood Gulch load, each test set up by dev commands, the game's own checks

- `give-sniper`: A sniper rifle given
- `warthog`: A warthog made ahead
- `score`: The score set
- `kill-respawn`: Killed and back
- `reset`: Reset

## devtools-078: DEV TOOLS 0.7.8

File `devtools-078.txt`, 8 tests.

0.7.8 DEV tools, on Octagon 2: ttk (time to kill) and dummy (a target that stands still) for weapon pack tuning, the

- `benchmark-run`: The benchmark: four maps, then a file
- `ttk-dummy`: ttk and a dummy: a kill's time
- `dummy-still`: Dummies stand still and do not shoot
- `dummy-heal`: A dummy heals and comes back
- `dummy-clear`: dummy clear sends them away
- `hud-list`: hud lists the pack weapon's HUD pieces
- `hud-move`: hud moves and hides a piece
- `hud-reset`: hud reset puts a piece back

## elim-076: ELIMINATION: the danger zone

File `elim-076.txt`, 11 tests.

0.7.6 ELIMINATION: the danger zone you can see. Blood Gulch, AE ELIM TM, 7 bots. "round live" ends the countdown at once;

- `zone-wait`: The zone waits: strip clock and the line
- `zone-look-b`: Look B, the light curtain
- `zone-look-a`: Look A, the energy wall (B is the default)
- `zone-edge`: The closing: edge line, then outside
- `zone-out`: Outside: the tint, the pulse, the death
- `zone-bots`: The bots go inside as it closes
- `zone-pick`: The circle sits between the bases
- `rev-marker`: A downed teammate has a marker
- `rev-basic`: Hold X on a downed teammate
- `rev-reset`: Let go and it starts over
- `rev-self`: You are the one down

## elimination-1: ELIMINATION, ROUND 1

File `elimination-1.txt`, 13 tests.

ELIMINATION, round 1: the round modes' game logic (rounds, the wipe, sides, the danger zone, revive, late joins,

- `round-flow`: A round: frozen start, then live
- `wipe-win`: A wiped team loses the round
- `wipe-tie`: Both wiped on one tick: a tie
- `side-switch`: From round 5 the sides switch
- `revive-hold`: You revive a teammate
- `revive-growth`: The second revive takes longer
- `bot-revive`: A bot revives you
- `spectate`: Out of the round you follow a teammate
- `zone-damage`: Outside the zone you lose health
- `zone-bots`: The bots go inside as it closes
- `late-join`: A late joiner waits for the next round
- `comp-start`: ELIMINATION COMP starts with the EVO rifle
- `classic-rounds`: ELIMINATION CLASSIC/CE: free for all rounds

## evo-5shot-081: EVO RIFLE 5 SHOT / DMR 4 SHOT

File `evo-5shot-081.txt`, 2 tests.

The EVO RIFLE kills with five rounds to the head (the fifth); the DMR stays at four (the fourth). On Octagon 2 with a

- `evo-5shot`: EVO RIFLE: the fifth round to the head kills
- `dmr-4shot`: DMR: the fourth round to the head kills

## fireteam-076b: FIRETEAMS, ROUND B

File `fireteam-076b.txt`, 5 tests.

Fireteam round b: the squad you lead takes your calls and no role. BTB Blood Gulch CTF, 16 bots, one load. You lead the first

- `regroup`: REGROUP brings your squad to you
- `follow`: FOLLOW sticks
- `bot-squad`: A bot-led squad still runs its role
- `arena-roles`: Arena: your squad plays its role, no trailing
- `arena-release`: Arena: a REGROUP ends by itself and the squad goes back to its role

## fireteam-1: FIRETEAMS, ROUND 1

File `fireteam-1.txt`, 3 tests.

Fireteams: bots belong to squads that take and keep orders. BTB Blood Gulch CTF, one load: the game's squad roles with a

- `roles`: Squad roles and the held anchor
- `follow-me`: FOLLOW ME
- `recruit`: A mark recruits a bot

## fixes-076a: 0.7.6 FIXES, ROUND A

File `fixes-076a.txt`, 5 tests.

0.7.6 FIXES, round A: the HJ magnum's scope, the brute shot at 50 a hit, the sword lunge up close, small arms on

- `hj-scope`: HJ magnum: the 2x scope
- `brute-shot`: Brute shot: 50 a direct hit
- `warthog-small-arms`: Pistol on an empty Warthog
- `ghost-sniper`: Sniper rifle on an empty Ghost
- `sword-lunge`: Sword: lunges up close and after a lunge

## fixwave-0771: 0.7.7.1 fix wave

File `fixwave-0771.txt`, 6 tests.

0.7.7.1 fix wave (ae/fixwave-078): EXTERMINATION's fixes (a time-out is a tie, the dead respawn armed; the quiet round

- `exterm-armed`: EXTERMINATION: dead at the round's end, armed after
- `exterm-timeout`: EXTERMINATION: a time-out is a tied round
- `xh-red`: Custom crosshair turns red over an enemy in range
- `xh-colors`: ENEMY COLOR and ALLY COLOR
- `say-keyboard`: Typing in SAY / chat with a keyboard
- `outside-click`: A click outside a dialog closes it

## grenades-emp: EMP GRENADE

File `grenades-emp.txt`, 8 tests.

The EMP grenade (AE grenade types, catalogue id 0): the vehicle checks on Blood Gulch (helper A, engine) and the

- `emp-stall-warthog`: An EMP stalls a Warthog, then it is immune for 2 s
- `emp-ghost-gun`: A stalled Ghost's guns are locked
- `emp-banshee-sink`: A stalled Banshee sinks
- `emp-powerup-short`: An EMP shorts a powerup lying on the floor
- `emp-scramble-in-vehicle`: A player in a vehicle is scrambled too, and nobody's EMP hurts
- `emp-refusal-68`: A copy without the EMP cannot join an EMP game
- `mcc-grenades-as-before`: MCC map, no grenade types: frag and plasma as before
- `mcc-emp-after-own`: MCC map with the EMP: it follows the map's own types

## halomd-076: HALOMD PORTS

File `halomd-076.txt`, 3 tests.

0.7.6 HaloMD round: the community ports played from maps_md.

- `halomd-picker`: The HALOMD folder in CHANGE MAP
- `halomd-br`: Battle rifle on beaverc_2
- `halomd-share`: A guest without the map gets it from the host

## hj-automag-080: HJ MAGNUM 5 SHOT

File `hj-automag-080.txt`, 1 tests.

The HJ MAGNUM kills with five shots to the head: the first four take the shield down (the pop shows on the 4th) and

- `hj-5shot`: HJ MAGNUM: the fifth shot to the head kills

## killfeed-1: KILL FEED, ROUND 1

File `killfeed-1.txt`, 11 tests.

KILL FEED, on testbox: every weapon and vehicle is on it. Kills show as rows at the left, their ticks a little left of the motion

- `first-look`: Rows above the sensor
- `own-rows`: Your rows and the others'
- `overflow`: Five rows at most
- `weapon-pictures`: Weapon pictures
- `pack-pictures`: Battle rifle, SMG, carbine, DMR
- `text-tags`: No picture: a text tag
- `head-shots`: Head shots and perfect kills
- `suicide-betrayal`: Suicide and a teammate's kill
- `assists`: Kills you helped with
- `free-for-all`: Free for all
- `split-screen`: Two players: a line in the messages

## mark-names: MARKS, SOLO

File `mark-names.txt`, 3 tests.

MARKS, solo (gate 1): aim, reading and the shared button. Keyboard B marks; a controller taps the flashlight button.

- `enemy-foot`: Marking enemies on foot
- `item-names`: What a mark says
- `enemy-driving`: Marking from the driver's seat

## mark-spike: MARKS, SOLO

File `mark-spike.txt`, 11 tests.

MARKS, solo (gate 1): aim, reading and the shared button. Keyboard B marks; a controller taps the flashlight button.

- `place-far-near`: A mark on the ground lands where you aimed
- `enemy`: A mark on an enemy
- `enemy-behind-wall`: An enemy out of sight is not marked
- `item`: A mark on an item
- `from-vehicle`: Marking from inside a vehicle
- `own-mark`: One mark a player
- `off-screen`: A mark out of view
- `controller`: The controller's flashlight button
- `controller-zoom`: The button while zoomed
- `looks`: Reading the marks
- `split-screen`: A teammate on the same screen

## medal-style-1: MEDAL STYLE, ROUND 1

File `medal-style-1.txt`, 10 tests.

MEDAL STYLE and the kill feed beside it, on testbox. UNIVERSAL is the default: a stack by your reticle, your points above it, a Killed

- `modern-one`: UNIVERSAL: one medal and a Killed row
- `modern-busy`: UNIVERSAL: many medals and kills
- `modern-gaps`: UNIVERSAL: gaps to the rest of the HUD
- `pause-long`: A long pause
- `mcc-style`: MCC: the medals on the left, the feed above
- `mcc-sensor`: MCC: captions clear of the sensor
- `classic-hud`: The Classic HUD
- `split-2`: Two views
- `split-3`: Three views
- `split-4`: Four views

## medals-1: MEDALS, ROUND 1

File `medals-1.txt`, 17 tests.

MEDALS, on testbox: every weapon, vehicle and mode is on it. With the personal announcer pack on, a medal with a clip speaks.

- `captions`: A medal and its name
- `first-look`: A medal on screen
- `multikill-spree`: Multikills and sprees
- `announcer`: The announcer speaks the medals
- `grenades-melee`: Grenades and melee
- `heavy-weapons`: The fuel rod and plasma cannon
- `style`: Style medals
- `needler`: The needler
- `plasma`: Plasma grenades and weapons
- `sniper`: The sniper rifle
- `vehicles`: Vehicle medals
- `flag`: Flag medals
- `king-ball`: Hill medals
- `ball`: Ball medals
- `game-end`: Medals at the end of a game
- `setting`: The setting
- `split-2`: Split screen

## medals-2: MEDALS, ROUND 2

File `medals-2.txt`, 22 tests.

MEDALS, round 2, on testbox: 36 more medals. A medal with no picture in MCC's folder shows as its name; note any that look wrong.

- `perfect-pistol`: Perfect with the pistol
- `no-scope`: No Scope and Quick Draw
- `last-shot`: Last Shot and Yard Sale
- `off-the-rack`: Off the Rack
- `proficiency`: Five kills of a kind
- `grenade-medals`: Grenade medals
- `turn-and-shot`: 360 and Nade Shot
- `assists-reversal`: Wingman, Reversal, Bodyguard
- `guardian-angel`: Guardian Angel
- `melee-styles`: Sneak King, Achilles Spine, Ninja
- `perfect-br`: Perfect with the battle rifle
- `perfect-evo`: Perfect with the Evo rifle
- `perfect-dmr`: Perfect with the DMR
- `perfect-carbine`: Perfect with the carbine
- `vehicle-kills`: Driver, Gunner, Driveby, Pineapple
- `banshee-sniper`: Pilot and Autopilot Engaged
- `sniper-pair`: Quigley
- `flag-medals`: Flag Joust, Stopped Short, Goal Line Stand
- `ball-medals`: Fumble and Straight Balling
- `hill-medals`: Hill Guardian
- `captions-sensor`: Captions and the motion sensor
- `split-2`: Split screen colours

## menus-phase1: MENUS PHASE 1

File `menus-phase1.txt`, 8 tests.

Menus phase 1 (0.9.0 line, owner 2026-11-10 decisions): the main menu's new rows and COMING SOON group, MULTIPLAYER with

- `main-rows`: MAIN MENU: the rows and COMING SOON
- `main-keys`: MAIN MENU: B, START, Y
- `multiplayer-rows`: MULTIPLAYER: rows with their descriptions
- `matchmaking-lobby`: MATCHMAKING: the playlist lobby
- `party-two`: PARTY: two players, hold B to remove
- `party-full`: PARTY: full at 4/4
- `motd-first`: MESSAGE OF THE DAY: at launch
- `whats-new`: WHAT'S NEW: releases and the badge

## net-090-host: 0.9.0 TWO MACHINES: HOST

File `net-090-host.txt`, 2 tests.

0.9.0 two machines, the HOST's list (the Windows PC, with V-SYNC as it is): each test hosts its game and stands the host's

- `host-head-check`: HEAD CHECK: real headshots stay headshots
- `host-hit-cover`: HIT COVER: nothing through a wall

## net-090-join: 0.9.0 TWO MACHINES: JOIN

File `net-090-join.txt`, 1 tests.

0.9.0 two machines, the JOINER's list (the laptop): its one test joins the host's game (net-090-host.txt on the host,

- `join-clock-slew`: Join the host's game; a 1 s hitch 40 s in

## next-082a: 0.8.2, ROUND A

File `next-082a.txt`, 4 tests.

0.8.2 TEST BUILD, round A: the PARTY popover, the server browser's hidden-servers count, the menus' speed. Each test

- `party-popover`: PARTY: one panel, one row of buttons
- `browser-hidden`: SERVER BROWSER: hidden AE servers named
- `menu-time`: Menus on this PC: MENU and DRAW times
- `split-crosshair`: Four players: every view draws its own crosshair

## next-090-latency: 0.9.0, LOW LATENCY

File `next-090-latency.txt`, 3 tests.

0.9.0 TEST BUILD (the Windows PC, a real monitor): LOW LATENCY OFF / ON / BOOST. Same map: one load; each test sets its mode.

- `latency-off`: LOW LATENCY OFF: the feel you know
- `latency-on`: LOW LATENCY ON: aim feels quicker, no stutter
- `latency-boost`: LOW LATENCY BOOST: quickest, still smooth

## next-090a: 0.9.0, ROUND A

File `next-090a.txt`, 4 tests.

0.9.0 TEST BUILD, round A (laptop): greyed rows, the crosshair under the pause menu, four players' crosshairs on 0.9.0's

- `greyed-row`: SETTINGS: a greyed row still reads greyed
- `pause-xh-off`: Pause menu, TARGET 60 FPS off: crosshair stays put
- `pause-xh-on`: Pause menu, TARGET 60 FPS on: crosshair stays put
- `split-four`: Four players: crosshairs and split graphics

## pack-auto-shotgun: AUTO SHOTGUN PACK

File `pack-auto-shotgun.txt`, 5 tests.

The auto-shotgun pack (slot 21, mask bit 20): in hand on every life on Octagon 2, a few bots.

- `auto-shotgun-look`: Auto-shotgun: the look
- `auto-shotgun-fire`: Auto-shotgun: fire with no pump
- `auto-shotgun-reload`: Auto-shotgun: one stroke after a reload
- `auto-shotgun-draw`: Auto-shotgun: draw, cut-off reload, idle
- `auto-shotgun-kill`: Auto-shotgun: 2 shots close, 5 at range

## pack-concussion-rifle: CONCUSSION RIFLE PACK

File `pack-concussion-rifle.txt`, 6 tests.

The CONCUSSION RIFLE pack (ZTeam's arc launcher; AE's numbers: 6 bolts a magazine at 2 a second, each a 1.5-unit

- `cr-look`: Concussion rifle: how it looks and reads
- `cr-name`: Concussion rifle: its name in words, no picture
- `cr-hits`: Concussion rifle: hits to kill
- `cr-knockback`: Concussion rifle: knockback on players
- `cr-vehicles`: Concussion rifle: knockback on an empty Warthog
- `cr-feel`: Concussion rifle: pace and the bolt

## pack-focus-beam: FOCUS BEAM PACK

File `pack-focus-beam.txt`, 3 tests.

The FOCUS BEAM pack (CMT's prototype focus beam, AE's numbers: a heat beam that strips shields fast and needs a

- `beam-look`: Focus beam: how it looks and reads
- `beam-strip`: Focus beam: shields melt, health does not
- `beam-finish`: Focus beam: strip, then finish

## pack-gravity-hammer: GRAVITY HAMMER PACK

File `pack-gravity-hammer.txt`, 4 tests.

The GRAVITY HAMMER pack (the hammer of Grifball Beta, its art as the map has it; AE's classic-era numbers: one swing every

- `gh-look`: Gravity hammer: how it looks
- `gh-prompts`: Gravity hammer: picture, texts, kill feed
- `gh-swing`: Gravity hammer: the swing kills up close
- `gh-safe`: Gravity hammer: reach, and no harm to you

## pack-grenade-launcher: GRENADE LAUNCHER PACK

File `pack-grenade-launcher.txt`, 4 tests.

The GRENADE LAUNCHER pack (ZTeam's grenade launcher; 6 a magazine, 12 to start; a bouncing grenade that bursts

- `grenade-launcher-look`: Grenade launcher: how it looks and reads
- `grenade-launcher-hold`: Grenade launcher: hold, then let go
- `grenade-launcher-fire`: Grenade launcher: magazine and direct hits
- `grenade-launcher-hits`: Grenade launcher: hits to kill

## pack-pdw: BURST PDW PACK

File `pack-pdw.txt`, 3 tests.

The burst PDW pack (slot 16, mask bit 15): in hand on every life on Octagon 2, a few bots.

- `pdw-look`: Burst PDW: the look
- `pdw-burst`: Burst PDW: one pull, three rounds
- `pdw-kill`: Burst PDW: 3 bursts close, 4 at mid

## pack-ravager: RAVAGER PACK

File `pack-ravager.txt`, 3 tests.

The RAVAGER pack (CMT's prototype ravager set up as the latest-generation ravager is, tuned for CE: three lobbed,

- `ravager-look`: Ravager: how it looks and reads
- `ravager-pulls`: Ravager: three pulls kill
- `ravager-charge`: Ravager: the charged bolt and its pool

## pack-revolver: HEAVY REVOLVER PACK

File `pack-revolver.txt`, 4 tests.

The HEAVY REVOLVER pack (ZTeam's heavy revolver, dark red and metal; the role's numbers, AE's damage: one slow,

- `revolver-look`: Heavy revolver: how it looks and reads
- `revolver-dropped`: Heavy revolver: in a bot's hands
- `revolver-hits`: Heavy revolver: hits to kill
- `revolver-feel`: Heavy revolver: pace and the slug

## pack-spiker: SPIKER PACK

File `pack-spiker.txt`, 3 tests.

The SPIKER pack (CMT's spiker_mp, AE's numbers: a close-to-mid automatic): spawns with the pack's weapon on every

- `spiker-look`: Spiker: how it looks and reads
- `spiker-kill`: Spiker: kill time up close and at mid
- `spiker-feel`: Spiker: pad, melee and pickup

## pack-starter-pistol: STARTER PISTOL PACK

File `pack-starter-pistol.txt`, 5 tests.

The AE STARTER PISTOL pack (the Digsite mod's 00_e3 pistol from the official mod tools, AE's numbers: one precise

- `pistol-look`: Starter pistol: how it looks and reads
- `pistol-zoom`: Starter pistol: the small zoom
- `pistol-ttk`: Starter pistol: hits to kill
- `pistol-range`: Starter pistol: red reticle and range
- `pistol-ammo`: Starter pistol: magazine and reload

## packs-energy-sword: ENERGY SWORD PACK

File `packs-energy-sword.txt`, 2 tests.

The energy sword pack (slot 14): given, kept through weapon switches.

- `give-sword`: Energy sword in slot 14
- `keep-through-switch`: The sword stays through two switches

## packs-smg-tactical: TACTICAL SMG PACK

File `packs-smg-tactical.txt`, 2 tests.

The TACTICAL SMG pack (CMT's smg_tactical, first set of numbers: provisional): spawns with the pack's weapon on every

- `tactical-look`: Tactical SMG: how it looks in your hands and on the ground
- `tactical-feel`: Tactical SMG: fire, zoom, reload

## pop-weapon-080: SHIELD POP BY WEAPON

File `pop-weapon-080.txt`, 4 tests.

COMBAT FEEDBACK's early shield pop shows when one more headshot of the weapon in use finishes: the least of 25 points

- `pop-magnum`: Magnum: the pop after the 2nd hit, the 3rd kills
- `pop-sidekick`: Sidekick: the pop after the 5th hit, not the 4th
- `pop-dmr`: DMR: the pop after the 3rd round, the 4th kills
- `pop-evo`: EVO RIFLE: the pop as the 4th round breaks it

## ragdoll-ab-081: RAGDOLL STYLES A/B (PLAYERS ROW)

File `ragdoll-ab-081.txt`, 4 tests.

0.8.1 ragdoll A/B (players' row): the same BR kill on standing dummies, one group a style. The AI row is NONE, so a body

- `ragdoll-spaghetti`: SPAGHETTI WESTERN: a stiff woody fall
- `ragdoll-exaggerated`: EXAGGERATED: loose from the first frame, wild
- `ragdoll-modern`: MODERN: limp at once, no stock animation, no spin
- `ragdoll-mixed`: MIXED: the stock fall first, then goes loose

## rc-076a: 0.7.6 FULL BUILD, ROUND A

File `rc-076a.txt`, 17 tests.

0.7.6 round A, tests 16-31 of the first run: screens, the ELIMINATION and score strips, fireteams, bots. Each test sets itself up

- `territories-picker`: CHANGE GAMETYPE: TERRITORIES rows
- `card-glyphs`: TEST RUN card: BUTTON PROMPTS UNIVERSAL glyphs
- `elim-strip`: ELIMINATION: the rounds strip
- `elim-cells`: ELIMINATION: a cell goes dark when a player falls
- `elim-countdown`: ELIMINATION: countdown, no crosshair, no strip
- `elim-out-text`: ELIMINATION: out until the round ends
- `elim-down-status`: ELIMINATION: DOWN on the scoreboard
- `elim-zone`: ELIMINATION: a zone death says ZONE
- `snd-strip`: SEARCH AND DESTROY: the score strip and DEAD
- `sly-strip`: TEAM SLAYER: the score strip, teammate names
- `ft-roles`: FIRETEAMS: roles per team
- `ft-follow`: FIRETEAMS: FOLLOW, no bot role on your squad
- `ft-regroup`: FIRETEAMS: REGROUP holds until arrival
- `bots-tank`: BOTS: a bot takes a Scorpion on its own
- `bots-dodge`: BOTS: they dodge a Warthog
- `bots-ghost-flag`: BOTS: Ghosts, the flag carrier and the Warthog
- `bots-stucks`: BOTS: Gulch side paths and tunnels (look)

## rc-077a: 0.7.7 RELEASE, ROUND A

File `rc-077a.txt`, 25 tests.

0.7.7 round A on the release tree (ae/release-077): the server lobby and its vote on the live servers, HOST A SERVER on

- `server-join`: SERVER LOBBY: join AE Official #1 by address
- `server-vote`: SERVER LOBBY: the vote on AE Comp Test
- `server-leave`: SERVER LOBBY: leave and rejoin
- `host-server`: HOST A SERVER: start and auto-join
- `host-my-server`: HOST A SERVER: MY SERVER page and card
- `host-quit`: HOST A SERVER: the quit choices
- `xh-new`: CROSSHAIR EDITOR: NEW takes over from STOCK
- `xh-duplicate`: CROSSHAIR EDITOR: DUPLICATE and CUSTOM COLOR
- `web-admin`: SERVER WEB PAGE through the tunnel
- `pad-bluetooth`: PADS: Bluetooth pad paired after the start
- `pad-ab-swap`: PADS: Nintendo A/B swap, no menu loop
- `xh-scope-row`: RETICLE: the IN SCOPE row and its default
- `xh-stock-word`: RETICLE: the word is STOCK, never WEAPON'S OWN
- `pad-hotplug`: PADS: found with udev off, wired and plugged later
- `prompts-universal`: BUTTON PROMPTS UNIVERSAL: readable glyphs
- `xh-scope-same`: IN SCOPE = SAME CROSSHAIR (set the row in SETTINGS > HUD > RETICLE first): sniper and pistol zoom
- `xh-scope-weapon`: IN SCOPE = WEAPON'S SCOPE (set the row in SETTINGS > HUD > RETICLE first): the weapon's own scope
- `xh-vehicle`: CROSSHAIR WEAPONS: the vehicle gun rows
- `shotgun-smp`: SHOTGUN: first-person model on an @smp conversion
- `arena-roles`: ARENA 4v4: your squad plays its role
- `arena-follow-temp`: ARENA 4v4: FOLLOW ME is temporary
- `btb-follow`: BTB: your squad follows you by default
- `hud-names-ae`: AE HUD: ally names always, enemy names in sight
- `hud-names-classic`: CLASSIC HUD: names as before
- `zone-look-default`: ELIMINATION: the zone's default look is A

## record-medals-081: 0.8.1 SERVICE RECORD: MEDALS PAGE

File `record-medals-081.txt`, 2 tests.

0.8.1 SERVICE RECORD: the MEDALS page (ae/record-medals-081). Each test opens SERVICE RECORD on a made-up player

- `record-medals-36`: MEDALS page, 36 kinds: in and out
- `record-medals-104`: MEDALS page, 104 kinds: scroll and sort

## release-040: 0.4.0 CANDIDATE

File `release-040.txt`, 13 tests.

0.4.0 candidate: what landed on release-040 since 0.3.9.1 that a game can show.

- `pause-1080`: The pause menu fits at 1080p
- `change-teams-asks`: CHANGE TEAMS asks first
- `oddball-times`: Oddball scores read as times
- `turret-bots`: Bots on the rocket Warthog's gun fire steadily
- `suppressed-names`: The silenced packs are called SUPPRESSED
- `menu-timing`: The full performance line shows menu time
- `postgame-team`: Team post-game: winner's colour
- `postgame-ffa`: FFA post-game: your place spelled out
- `ball-carrier-cover`: Bot ball carriers hide and hold
- `madhouse-floors`: Madhouse: bots move between floors
- `bots-squads`: Bots squad up: one pins you, others flank
- `bots-vehicles`: Bots drive: laps, wait for you, flip back
- `wheelman`: Wheelman: your gunner's kill is your assist

## round-1111: TEST ROUND 11-11: BOTS, CAMERAS, RAGDOLLS

File `round-1111.txt`, 16 tests.

TEST ROUND 11-11 (AE Latest's combined build: the 0.8.1 line, the MODERN ragdoll pushes, the 0.9.0 bots and the death

- `temple-easy`: TEMPLE 2V2: EASY bots
- `temple-heroic`: TEMPLE 2V2: HEROIC bots
- `temple-elite`: TEMPLE 2V2: ELITE bots
- `temple-legendary`: TEMPLE 2V2: LEGENDARY bots
- `temple-grenades`: TEMPLE: bots get out of the way of your grenades
- `temple-still`: TEMPLE: no bot stands still for long
- `camera-death-hold`: CAMERA: dying, eyes out to your body, killer in view
- `camera-follow-switch`: CAMERA: LB and RB switch with a quick fade
- `spawn-no-grace`: PRISONER: HEROIC bots don't wait after you spawn
- `high-ground`: PRISONER: bots hold the high spots a while
- `teleporter-fight`: CHILL OUT: a bot fighting you stays in the fight
- `weapon-range`: CHILL OUT: bots pick sniper or pistol by distance
- `ragdoll-modern`: RAGDOLL MODERN: tips back high, swept low
- `ragdoll-mixed`: RAGDOLL MIXED: the stock fall, then loose
- `ragdoll-exaggerated`: RAGDOLL EXAGGERATED: loose and wild
- `ragdoll-spaghetti`: RAGDOLL SPAGHETTI WESTERN: a stiff woody fall

## sample: SAMPLE LIST

File `sample.txt`, 3 tests.

A sample TEST RUN list: three tests, every kind of line.

- `sling-swap`: Rifle slung after a swap
- `menu-screen`: The test screen draws
- `plain-map`

## sandbox-pipeline: SANDBOX PIPELINE: DEV TUNE

File `sandbox-pipeline.txt`, 5 tests.

DEV MODE tune on Octagon 2: change the held weapon's numbers live, see the time to kill move, keep the numbers in a

- `tune-list`: tune lists the held weapon's numbers
- `tune-ttk`: A faster trigger kills sooner
- `tune-save`: tune save writes the tune file
- `tune-reset`: tune reset puts the numbers back
- `tune-limits`: Only numbers the engine cannot hold are refused

## sound-mix-080: SOUND MIX: PISTOL SHOT, RIFLE ECHO, PAUSE

File `sound-mix-080.txt`, 4 tests.

Weapon sound mix (0.8.0): the starter pistol's new firing sound, the rifles' recorded echo faded, the reverb on pause.

- `sound-pistol`: Starter pistol: the new shot (Digsite E3 rifle's)
- `sound-br`: Battle rifle: short echo, the room does the rest
- `sound-dmr-evo`: DMR: the same short echo
- `sound-carbine`: Carbine: its 2.7 s tail cut to about 1 s

## split-090: 0.9.0 SPLIT SCREEN, TARGET 60 FPS

File `split-090.txt`, 11 tests.

0.9.0 split screen (perf step C): two and four players on one screen, the two-player layouts (top and bottom, side by

- `split-auto`: AUTO on a 16:9 screen: top and bottom
- `split-side-by-side`: SIDE BY SIDE: two tall views
- `split-side-bars-ae`: SIDE BARS, HUD STYLE AE (the default)
- `split-side-bars-classic`: SIDE BARS, HUD STYLE CLASSIC
- `split-four`: Four players, SPLIT GRAPHICS AUTO
- `split-four-same`: Four players, SPLIT GRAPHICS SAME
- `split-page`: The SPLIT SCREEN settings page
- `target60-held-one`: TARGET 60 FPS held at 50 %, one player
- `target60-camo`: Camo at 50 %: your weapon's shimmer
- `target60-held-split`: TARGET 60 FPS held at 50 %, two players
- `target60-four`: TARGET 60 FPS on its own, four players

## strip-076: SCORE STRIP, 0.7.6

File `strip-076.txt`, 8 tests.

0.7.6 SCORE STRIP: the right strip on each round mode (HUD STYLE AE). ELIMINATION shows the rounds strip: alive cells,

- `elim-strip`: ELIMINATION: the rounds strip
- `elim-cells`: ELIMINATION: a cell goes dark when a player falls
- `elim-countdown`: ELIMINATION: the countdown between rounds
- `elim-out-text`: ELIMINATION: out until the round ends
- `elim-down-status`: ELIMINATION: DOWN then DEAD on the scoreboard
- `elim-ffa`: ELIMINATION free for all: the rounds strip
- `name-no-triangle`: Teammate names without the green triangle
- `snd-dead-status`: SEARCH AND DESTROY: DEAD on the scoreboard

## vehicle-balance-1: VEHICLE BALANCE

File `vehicle-balance-1.txt`, 13 tests.

Vehicle balance, round 1: what hurts a vehicle and in how many shots (VEHICLES DESTRUCTIBLE ON, every AE gametype with

- `bullets-hurt`: Bullets now damage a vehicle
- `sniper-counts`: Sniper rifle against vehicles
- `rocket-counts`: Rockets (as before)
- `grenade-counts`: Grenades against light vehicles
- `shotgun-counts`: Shotgun against a Warthog
- `pack-rifles`: Pack rifles against a Warthog
- `tank-shell`: Tank shell against vehicles
- `vehicle-guns`: Vehicle guns against vehicles
- `rider-hull-shots`: A rider while the hull is shot
- `rider-dies-wreck`: A rider when the vehicle is destroyed
- `turret-banshee`: Gun turret and Banshee
- `small-arms-weak`: Plasma and needles hardly scratch
- `stock-unchanged`: Stock Slayer: vehicles never die

## vehicle-balance-2: VEHICLE BALANCE, ROUND 2

File `vehicle-balance-2.txt`, 5 tests.

Vehicle balance, round 2 (0.7.6): small arms now really hurt a vehicle (0.7.5 shipped it as a known issue: only

- `pistol-warthog`: Pistol on an empty Warthog
- `rifle-warthog`: Assault rifle on an empty Warthog
- `sniper-ghost`: Sniper rifle on an empty Ghost
- `small-arms-scorpion`: Small arms on the Scorpion
- `rockets-as-before`: Rockets, as before
