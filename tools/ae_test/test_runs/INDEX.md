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

## bots-tank-1: BOTS IN THE TANK, ROUND 1

File `bots-tank-1.txt`, 3 tests.

Bots in the Scorpion: its driver drives it and fires its cannon and chain gun. Only a map's tanks; it drives on the open maps.

- `tank-board`: A bot takes the tank
- `tank-guns`: Cannon and chain gun
- `tank-aim`: Turret and hull

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
