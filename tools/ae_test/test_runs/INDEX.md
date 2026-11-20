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

## action-sack-094: ACTION SACK

File `action-sack-094.txt`, 3 tests.

0.9.4 ACTION SACK: the casual ACTION SACK playlist (the old SWAT & SNIPERS slot and id, so the same rank): four

- `row-and-rank`: The row and the old rank
- `shotty-snipers`: SHOTTY SNIPERS start
- `team-rockets`: Team ROCKETS

## biped-packs-10x: PLAYER MODEL: A BIPED PACK'S BIPED

File `biped-packs-10x.txt`, 3 tests.

PLAYER MODEL: a host option (GAME OPTIONS, SPARTAN) that makes every player, bots too, the biped of a biped pack. Off

- `model-on`: The pack's biped, a weapon in its hands
- `model-off`: STANDARD: the map's own biped
- `model-refused`: A biped that fails the check

## bots-090: BOTS 0.9.0: DRIVING, NORMAL, ELITE

File `bots-090.txt`, 3 tests.

The 0.9.0 bots: three set-ups, four checks: planned driving on a vehicle map, NORMAL bots finishing a fight with a

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

## bots-birdcage-100: BIRDCAGE: NO SHOTS THROUGH GLASS

File `bots-birdcage-100.txt`, 1 tests.

1.0 rc2 BIRDCAGE: bots and bulletproof glass. The bots' sight test looks through two-sided surfaces (glass) as the

- `glass`: BIRDCAGE: bots don't shoot through the glass

## bots-calls-091: BOTS: WHEEL CALLS

File `bots-calls-091.txt`, 3 tests.

The bots' wheel calls (0.9.1): bot teammates mark what they already act on, as a human teammate's call does (ENEMY on

- `marks`: BLOOD GULCH CTF: play with your bot team (three minutes)
- `defend`: BLOOD GULCH CTF: wait at your own flag
- `ride`: BLOOD GULCH BTB: stay on foot near your base's vehicles

## bots-crew-093: BOTS: DRIVERS AND CREW

File `bots-crew-093.txt`, 2 tests.

0.9.3 BOTS: drivers who wait for crew. A bot who gets into the blue base's Warthog waits for crew: one stop for crew

- `drive-off`: BLOOD GULCH: a driver with no crew coming drives off
- `wait-for-you`: BLOOD GULCH: a driver waits while you walk to it

## bots-damnation-094: DAMNATION: PIT JUMPS AND SPAWNS

File `bots-damnation-094.txt`, 12 tests.

0.9.4 DAMNATION: bots and the pit. The bots do not make their own jumps across a gap with a deadly drop under it

- `pit-falls`: DAMNATION: no bots into the pit
- `bottom-mid`: DAMNATION: bots get out of bottom mid
- `ladders`: DAMNATION: bots climb the bottom-mid ladders
- `upper-rim`: DAMNATION: no drops from the upper level into the hole
- `pit-jump`: DAMNATION: no learnt jump over the hole
- `hurt-drops`: DAMNATION: hurt bots and big drops
- `low-corner`: DAMNATION: the low corner's spawns
- `rockets`: DAMNATION: bots go down for the rockets
- `small-ladder`: DAMNATION: a bot climbs out by the small ladder
- `rocket-use`: DAMNATION: a bot fires the rockets it holds
- `rocket-pickup`: DAMNATION: a bot goes for the map's rockets
- `dig-pit-falls`: DIGSITE DAMNATION: no bots into the pit

## bots-defend-100: BOTS HOLD DEFEND: COVER AND PEEK

File `bots-defend-100.txt`, 3 tests.

BOTS HOLD DEFEND, staged: a fireteam told DEFENDING HERE (the wheel's call; DEV `fireteam defend` gives the same order)

- `cover`: Your squad holds the spot in cover
- `peek`: A defender peeks from its cover
- `fallback`: A bot squad that breaks falls back and returns

## bots-drive-090: BOTS DRIVING: CLASSIC / PLANNED

File `bots-drive-090.txt`, 6 tests.

0.9.0 PLANNED DRIVING: bots at the wheel plan their speed ahead of corners (slower the sharper and nearer the turn) and of

- `snow-classic`: Snow Grove: bots drive on the old rules
- `snow-planned`: Snow Grove: bots drive on the new rules
- `gulch-classic`: Blood Gulch: bots drive on the old rules
- `gulch-planned`: Blood Gulch: bots drive on the new rules
- `sidewinder-classic`: Sidewinder: bots drive on the old rules
- `sidewinder-planned`: Sidewinder: bots drive on the new rules

## bots-drive-carrier-100: A BOT DRIVES THE FLAG HOME

File `bots-drive-carrier-100.txt`, 1 tests.

A BOT DRIVES THE FLAG HOME: a teammate bot at the wheel of a vehicle the flag's carrier rides in drives to the

- `flag-ride`: You hold their flag; a bot drives you home

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

The bot fixes of 0.7.8: NEED A RIDE from a damaged vehicle's seat, fireteams split by scale and free with no order,

- `free-team`: With no order your squad plays on its own
- `regroup`: REGROUP brings them and they keep firing
- `regroup-again`: REGROUP again lets them play free
- `ride-damaged`: NEED A RIDE from a damaged warthog
- `tanks-holds`: Tanks and high ground in Slayer
- `arena-near`: Arena: your squad stays loosely near

## bots-fractured-094: FRACTURED PEAK: THE TOP HILL, THE CRACKS, YOUR SQUAD

File `bots-fractured-094.txt`, 4 tests.

0.9.4 FRACTURED PEAK KING: bots take the hill. The top-middle hill's point sits over a gap in its floor: the goal is

- `top-hill`: FRACTURED PEAK: bots take the top-middle hill
- `cracks`: FRACTURED PEAK: no bots dying in the cracks
- `squad-hill`: FRACTURED PEAK: your squad takes the hill unasked
- `team-hill`: FRACTURED PEAK: your team's bots stay in the hill

## bots-gaps-090: BOTS: SPAWN GRACE, WEAPON BY RANGE, HIGH GROUND

File `bots-gaps-090.txt`, 4 tests.

0.9.0 bot round: four changes to how the bots play.

- `spawn-grace`: HEROIC bots don't wait after you spawn
- `high-ground`: Bots with rifles hold the high spots a while
- `teleporter-fight`: A bot fighting you stays in the fight
- `weapon-range`: Bots pick sniper rifle or pistol by distance

## bots-learnt-093: BOTS: THE MAP LEARNT FROM GAME TO GAME

File `bots-learnt-093.txt`, 3 tests.

0.9.3 BOTS: bots keep what they learnt of a map (bots.learn_map, on). Where drivers wedged and where bots on foot got

- `your-jump`: TEMPLE: a jump of yours is kept
- `bots-take-it`: TEMPLE: the bots take your jump after a reload
- `off-switch`: TEMPLE: the switch off

## bots-names-093: BOTS: LOBBY NAMES

File `bots-names-093.txt`, 1 tests.

0.9.3 AE TOURNAMENT: the bots play under their lobby names. The lobby names the bots (from the bot name pool) and the

- `names-match`: TOURNAMENT: the lobby's names in the game

## bots-nav-100: BOT NAV: BLOOD GULCH ROUTES

File `bots-nav-100.txt`, 4 tests.

1.0 rc2 BOT NAV: a map may carry a hand-placed nav file of vehicle routes (lanes base to base, base exits, crossings;

- `lanes`: Blood Gulch: drivers keep to the lanes
- `exits`: Blood Gulch: drivers get out of the bases
- `scorpion`: Blood Gulch: a tank crosses the map
- `gunner`: Blood Gulch: a bot driver waits for its gunner

## bots-nav-btb-100: BOT NAV: THE BIG TEAM BATTLE MAPS

File `bots-nav-btb-100.txt`, 9 tests.

BOT NAV ON THE BIG TEAM BATTLE MAPS: each map carries a nav file of vehicle routes (lanes base to base, base exits,

- `drive-dangercanyon`: Danger Canyon: the rocket warthog past the boulder
- `drive-dangercanyon-far`: Danger Canyon: on through the pit to the red base
- `drive-deathisland`: Death Island: a bot driver keeps to the lanes
- `drive-icefields`: Ice Fields: a bot driver keeps to the lanes
- `drive-sidewinder`: Sidewinder: a bot driver keeps to the lanes
- `drive-timberland`: Timberland: a bot driver keeps to the lanes
- `drive-gephyrophobia`: Gephyrophobia: a bot driver keeps to the lanes
- `drive-infinity`: Infinity: a bot driver keeps to the lanes
- `drive-cliffhanger`: Cliffhanger: a bot driver keeps to the lanes

## bots-nav-show-100: BOT NAV: THE ROUTES DRAWN IN THE WORLD

File `bots-nav-show-100.txt`, 1 tests.

THE BOTS' VEHICLE NAVIGATION DRAWN IN THE WORLD (DEV MODE): "nav show all" draws, over the ground near you, the routes the

- `show-dangercanyon`: Danger Canyon: routes, rings and drivers drawn

## bots-powerup-090: BOTS: SHOOTING DOWN HUNG ITEMS

File `bots-powerup-090.txt`, 4 tests.

0.9.0: bots shoot down hung items (powerups, a better weapon, a health pack while hurt). Digsite's workshop maps

- `hotbox-camo`: A bot shoots down Hotbox's hung camouflage
- `hotbox-fight`: A bot in a fight leaves the powerup alone
- `gothra-both`: Gothra's hung camouflage and overshield come down
- `chillout-sword`: A bot shoots down Chill Out's hung sword

## bots-prisoner-094: PRISONER: SHOT BOTS KEEP THE HIGH GROUND

File `bots-prisoner-094.txt`, 4 tests.

0.9.4 PRISONER: bots and drops. A bot in a fight with its shields under three quarters takes no drop of a floor's

- `top-floor`: PRISONER: no bots jump off the top floor
- `cover-retreat`: PRISONER: hurt bots fall back to cover
- `empty-magazine`: PRISONER: no fighting with an empty gun
- `chase-hop`: PRISONER: bots hop up after you

## bots-reach-093: BOTS: THE CAN'T-REACH REPORT

File `bots-reach-093.txt`, 3 tests.

0.9.3 BOTS: the can't-reach report (dummy reach). Four walking dummies on your team walk from every spawn of the map

- `reach-run`: BLOOD GULCH: four walkers try every walk
- `reach-report`: BLOOD GULCH: the report at the end
- `reach-stop`: BLOOD GULCH: dummy clear stops a run

## bots-records-094: BOT SERVICE RECORDS

File `bots-records-094.txt`, 7 tests.

0.9.4 BOT SERVICE RECORDS: every bot met in an AE TOURNAMENT game keeps a record on this PC (its own file,

- `carnage-block`: CARNAGE: a bot's SERVICE RECORD
- `carnage-new`: CARNAGE: a bot met the first time
- `page-from-postgame`: BOT RECORD from the post-game
- `page-split`: BOT RECORD: two players
- `career-bots`: CAREER > BOTS
- `career-empty`: CAREER > BOTS: none met
- `real-write`: A real game writes the records

## bots-rocks-092: BOTS: DRIVERS AND SCENERY

File `bots-rocks-092.txt`, 6 tests.

0.9.2 BOTS AND SCENERY: bot drivers know the map's rocks, trees and bushes. Routes keep off them, the driver steers round one

- `boulder`: BLOOD GULCH: the boulder at the south side
- `low-boulder-tree`: BLOOD GULCH: the low boulder and the dead tree
- `big-rock`: BLOOD GULCH: the big rock
- `boulder-east`: BLOOD GULCH: the boulder to the east
- `bushes`: BLOOD GULCH: the bush cluster
- `off-switch`: BLOOD GULCH: the switch off, at the big rock

## bots-score-arena: BOT SCORECARD: ARENA

File `bots-score-arena.txt`, 4 tests.

Bot scorecard, arena 4v4 (arena tests never run on Blood Gulch): Temple and Sali Creek, AE TEAM SLY, 7 bots. Headless

- `temple-1`: The bots play
- `temple-2`: The bots play on
- `sali-1`: The bots play
- `sali-2`: The bots play on

## bots-score-btb: BOT SCORECARD: BIG TEAM

File `bots-score-btb.txt`, 5 tests.

Bot scorecard, big team: Blood Gulch, AE BTB SLY, 15 bots. Headless only, not a hand-played list: run_list.py --after

- `play-1`: The bots play
- `play-2`: The bots play on
- `play-3`: The bots play on
- `ride-1`: NEED A RIDE
- `ride-2`: NEED A RIDE from the gunner's seat (a bot comes to drive)

## bots-score-canyon: BOT SCORECARD: DANGER CANYON

File `bots-score-canyon.txt`, 4 tests.

Bot scorecard, a map's stuck spots: Danger Canyon, AE TEAM SLY, 11 bots (a pocket at the centre (8.9 2.4 -4.0), off

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

Bot scorecard, driving: Snow Grove (a Custom Edition map with vehicles), AE BTB SLY, 15 bots, bots drive

- `drive-1`: The bots play
- `drive-2`: The bots play on
- `drive-3`: The bots play on
- `drive-4`: The bots play on

## bots-score-stuck: BOT SCORECARD: STUCK SPOTS

File `bots-score-stuck.txt`, 4 tests.

Bot scorecard, a map's stuck spots: Blamite Mines, AE TEAM SLY 4v4, 7 bots (the Comp trap: blue bots stuck low).

- `mines-1`: The bots play
- `mines-2`: The bots play on
- `mines-3`: The bots play on
- `mines-4`: The bots play on

## bots-score-temple: BOT SCORECARD: TEMPLE, A FIGHTING PLAYER

File `bots-score-temple.txt`, 2 tests.

Bot scorecard, Temple with a fighting player: AE BR TS, 7 bots, the headless player played by a bot brain

- `temple-1`: The bots play
- `temple-2`: The bots play on

## bots-seat-skill-093: BOTS: EACH BOT ITS OWN SKILL

File `bots-seat-skill-093.txt`, 3 tests.

0.9.3 BOTS: each bot its own SHOOTING and MOVEMENT skill (DEV botskill; the offline social career's bot spread uses it).

- `shooting-pair`: TEMPLE: a RECRUIT and a LEGENDARY shooter at once
- `movement-pair`: TEMPLE: a RECRUIT and a LEGENDARY mover at once
- `back-to-settings`: TEMPLE: botskill clear: both play the settings

## bots-squad-fallback-100: SQUAD FALLS BACK WHEN ITS LEADER DIES

File `bots-squad-fallback-100.txt`, 1 tests.

SQUAD FALLS BACK: in a small game, when the person leading a bot squad dies, the squad goes back to defend its team's flag

- `leader-dies`: Your squad goes back to defend when you die

## bots-style-093: BOTS: PLAY STYLES

File `bots-style-093.txt`, 3 tests.

0.9.3 BOTS: play styles (DEV botstyle; the offline social career gives each bot name its own). A style bends what a bot

- `pusher-anchor`: TEMPLE: a PUSHER and an ANCHOR at once
- `hopper`: TEMPLE: a HOPPER against a STEADY bot
- `favourite`: TEMPLE: a favourite weapon

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

## bots-walker-093: BOTS: THE WALKING DUMMY

File `bots-walker-093.txt`, 3 tests.

0.9.3 BOTS: the walking dummy (dummy walk), DEV MODE's staged spot check. One bot on your team is put at a spot where

- `loop-spot`: TEMPLE: the walker leaves the flank-loop spot
- `wall-spot`: TEMPLE: the walker leaves the wall spot
- `roam-here`: TEMPLE: a walker at your feet that roams

## bots-warren-100: WARREN: RAMPS AND THE CAMO PLATFORM

File `bots-warren-100.txt`, 3 tests.

1.0 rc2 WARREN: bots on ledges and paths. The upper level's 34 degree ramps (352 of Warren's 2241 walkable surfaces

- `ramp`: WARREN: a bot walks down the upper ramp
- `camo`: WARREN: a bot gets off the camo platform
- `game`: WARREN: bots get round the map

## button-canon-094: BUTTON CANON

File `button-canon-094.txt`, 10 tests.

0.9.4 BUTTON CANON: one meaning per button across the AE menus. A accepts, B goes back (Back / Cancel / Leave / Close), X acts on

- `browser-pad`: SERVER BROWSER: footer on the pad
- `browser-keys`: SERVER BROWSER: keyboard and mouse
- `browser-lan`: JOIN LAN GAMES: Search again
- `gametype-pad`: CHANGE GAMETYPE: X Manage / Folders
- `gametype-keys`: CHANGE GAMETYPE: the Delete key
- `options-pad`: GAME OPTIONS: X Reset, Y Save as new
- `options-keys`: GAME OPTIONS: F and R
- `settings-player`: SETTINGS: the player on LB / RB
- `b-labels`: B labels: Back, Cancel, Leave, Close
- `footer-band`: The footer band

## calibration-094: AE TOURNAMENT CALIBRATION

File `calibration-094.txt`, 7 tests.

0.9.4 CALIBRATION: the first time a profile opens AE TOURNAMENT it is asked "HOW MUCH HALO HAVE YOU PLAYED?" (NEW / SOME /

- `ask-pad`: The question: pad
- `ask-keyboard-mouse`: The question: keyboard and mouse
- `roster-new`: NEW: the first roster is EASY
- `roster-some`: SOME: the first roster is NORMAL
- `roster-alot`: A LOT: the first roster is HEROIC
- `career-change`: CAREER: change EXPERIENCE
- `split-screen`: Split screen: each player is asked

## camera-rc2: CAMERA RC2: the death camera twice

File `camera-rc2.txt`, 1 tests.

CAMERA (temple): the death camera twice in one game. A second death shows the death camera, not the teammate view,

- `death-camera-twice`: CAMERA: two deaths in one game, 20 s apart

## camera-smooth-081: CAMERAS: DEATH HOLD, AIM FOLLOW, GLIDES

File `camera-smooth-081.txt`, 2 tests.

Death and follow cameras (0.8.1): the camera looks at the killer, starts at the helmet and the right stick orbits the

- `death-hold`: Dying: eyes out to your body, your killer in view
- `follow-switch`: Following: LB and RB switch with a quick fade

## campaign-workshop-rc2: 1.0.0-rc2 CAMPAIGN: CUSTOM CAMPAIGNS

File `campaign-workshop-rc2.txt`, 9 tests.

1.0.0-rc2 CAMPAIGN: custom campaigns. A Halo 1 Workshop campaign installed from SETTINGS > SYSTEM > MCC > STEAM ACCOUNT

- `picker`: CAMPAIGN: the custom campaign and its missions
- `play-quit`: A custom mission: SAVE AND QUIT, then CONTINUE
- `keep-stock`: Each campaign's CONTINUE stays its own
- `continue-look`: A custom campaign's CONTINUE row (made-up saved game)
- `continue-fault`: A saved game that cannot go on says why
- `split`: A custom mission in split screen
- `stock`: The stock campaign as before, under the picker
- `none`: No custom campaign installed: where they come from
- `fault`: A package with a fault: one quiet line says what

## combined-076a: 0.7.6 COMBINED, ROUND A

File `combined-076a.txt`, 13 tests.

0.7.6 COMBINED TEST BUILD (test/combined-076): fireteams, bot driving, the combat AR's viewmodel, the lobby, the

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

TEAM CALLS: the smart button, the wheel, prompts, markers, the feed and the seat panel. A controller on AE's layout

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

TEAM CALLS: the prompt by the reticle, the driver's crosshair, a marked vehicle's driver, an enemy's vehicle. Host and

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

## coop-094: CAMPAIGN CO-OP, PRIVATE LISTING AND HOST LEFT

File `coop-094.txt`, 12 tests.

0.9.4 CAMPAIGN CO-OP: split-screen co-op for up to FOUR players on this PC from AE's own CAMPAIGN screen, and ONLINE co-op

- `seats-pad`: Pads: START joins, B leaves
- `seats-four`: Four players: the panel, the count, the full table
- `start-two`: A level starts for both players
- `keys-mouse`: Keyboard and mouse
- `error-dialog`: The co-op error dialog
- `host-lan`: CO-OP LOBBY on this PC: LAN, two players
- `local-pad-lobby`: A pad's START joins the lobby, up to four
- `host-online`: HOST PC: host a level online from CAMPAIGN
- `join-browser`: JOINING PC: only PUBLIC online co-op is listed
- `join-address`: JOINING PC: join by address lands in the same lobby
- `play-together`: BOTH: the host starts, both play the level
- `host-left`: JOINING PC: the co-op host leaves

## defend-1: BOTS HOLD DEFEND: ROLES, FALL BACK, ZONES

File `defend-1.txt`, 10 tests.

Bots hold DEFEND: a fireteam told DEFENDING HERE, and in a BTB game a bot squad in a defend role, take spaced cover around the

- `btb-role-hold`: A bot squad's defend role holds spaced places
- `bot-squad-breaks`: A bot squad that breaks falls back together
- `fireteam-defend`: DEFENDING HERE in a small game
- `open-ground`: DEFENDING HERE on open ground
- `leader-quiet-death`: A quiet leader death keeps the hold
- `leader-dead-pressure`: A leader dead with his squad down: fall back, then return
- `hill-role`: A squad holds its zone
- `roles-ctf`: A small game's flag roles are left alone
- `roles-king`: A small game's hill roles are left alone
- `roster-change`: A bot leaves and another joins in a small game

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

## dev-unseen-100: DEV MODE: UNSEEN BY THE BOTS

File `dev-unseen-100.txt`, 2 tests.

DEV MODE unseen: "unseen [me|<player>|off]" hides a player from the bots. They do not see, target or hunt that player,

- `unseen-walk`: Walk up to the bots unseen
- `unseen-off`: Unseen off: the bots see you again

## devtest-sample: DEV TEST SAMPLE

File `devtest-sample.txt`, 5 tests.

DEV MODE's sample grouped list: one Blood Gulch load, each test set up by dev commands, the game's own checks judged

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

## digsite-rc2: AE TOURNAMENT: DIGSITE PLAYLISTS

File `digsite-rc2.txt`, 3 tests.

rc2 DIGSITE PLAYLISTS: two casual AE TOURNAMENT playlists on the maps of the "CE Digsite Multiplayer" Steam Workshop pack (item

- `digsite-arena`: AE:T: DIGSITE ARENA row and card
- `digsite-big-team`: AE:T: DIGSITE BIG TEAM row and card
- `digsite-vote`: AE:T VOTE: three Digsite maps with vehicles

## dredwerkz-ws-11x: DREDWERKZ: THE WORKSHOP FILE

File `dredwerkz-ws-11x.txt`, 3 tests.

DREDWERKZ from its Steam Workshop item (2900215828), read as the file is (no converted copy). The map has no script

- `look`: DREDWERKZ from the Workshop: the map and its look
- `teleporters`: DREDWERKZ from the Workshop: its own teleporters
- `pit`: DREDWERKZ from the Workshop: the pit

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

ELIMINATION: the round modes' game logic (rounds, the wipe, sides, the danger zone, revive, late joins, AE players

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

## fif-093: 0.9.3 AIM A/B

File `fif-093.txt`, 2 tests.

0.9.3: a blind A/B of display.frames_in_flight. Two laps, the same map and lap, V-SYNC on, LOW LATENCY off, TARGET 60

- `aim-lap-a`: AIM LAP A
- `aim-lap-b`: AIM LAP B

## fireteam-076b: FIRETEAMS, ROUND B

File `fireteam-076b.txt`, 5 tests.

Fireteam b: the squad you lead takes your calls and no role. BTB Blood Gulch CTF, 16 bots, one load. You lead the

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

## firstrun-094: FIRST RUN: SAFE AREA, SPLIT PAUSE

File `firstrun-094.txt`, 7 tests.

0.9.4 FIRST RUN: HUD SAFE AREA (Settings > HUD: the HUD pulled in 0 to 10 % from the screen's edges, for TVs and very wide

- `safe-one-0`: HUD SAFE AREA 0: as before
- `safe-one-5`: HUD SAFE AREA 5 %: AE HUD
- `safe-one-10`: HUD SAFE AREA 10 %: AE HUD
- `safe-two-5`: HUD SAFE AREA 5 %: two players
- `safe-two-10`: HUD SAFE AREA 10 %: two players
- `pause-two`: Split pause: two pads
- `pause-four`: Split pause: four pads

## fixes-076a: 0.7.6 FIXES, ROUND A

File `fixes-076a.txt`, 5 tests.

0.7.6 FIXES: the HJ magnum's scope, the brute shot at 50 a hit, the sword lunge up close, small arms on vehicles,

- `hj-scope`: HJ magnum: the 2x scope
- `brute-shot`: Brute shot: 50 a direct hit
- `warthog-small-arms`: Pistol on an empty Warthog
- `ghost-sniper`: Sniper rifle on an empty Ghost
- `sword-lunge`: Sword: lunges up close and after a lunge

## fixwave-0771: 0.7.7.1 fix wave

File `fixwave-0771.txt`, 6 tests.

0.7.7.1 fix wave: EXTERMINATION's fixes (a time-out is a tie, the dead respawn armed), the custom crosshair's red over

- `exterm-armed`: EXTERMINATION: dead at the round's end, armed after
- `exterm-timeout`: EXTERMINATION: a time-out is a tied round
- `xh-red`: Custom crosshair turns red over an enemy in range
- `xh-colors`: ENEMY COLOR and ALLY COLOR
- `say-keyboard`: Typing in SAY / chat with a keyboard
- `outside-click`: A click outside a dialog closes it

## flag-spotted: FLAG WHEN SPOTTED

File `flag-spotted.txt`, 3 tests.

The own flag's waypoint WHEN SPOTTED (the AE CTF seeds): away from home it shows; carried by an enemy it shows to the defenders

- `carrier-seen`: Enemy runs your flag out of your base and back in
- `carrier-hidden`: Enemy runs your flag behind your base's walls
- `flag-dropped`: Your flag dropped: its waypoint shows without sight

## grenades-emp: EMP GRENADE

File `grenades-emp.txt`, 8 tests.

The EMP grenade (AE grenade types, catalogue id 0): the vehicle checks on Blood Gulch (engine) and the infantry checks

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

0.7.6 HaloMD: the community ports played from maps_md. Before the run: download beaverc_2 from HaloMD's mod list into

- `halomd-picker`: The HALOMD folder in CHANGE MAP
- `halomd-br`: Battle rifle on beaverc_2
- `halomd-share`: A guest without the map gets it from the host

## hammer-rc3: GRAVITY HAMMER: BACK AND GROUND

File `hammer-rc3.txt`, 4 tests.

The GRAVITY HAMMER pack, revision 3: on the back it lies along the spine (head low by the hip, pommel up past the

- `back-behind`: Hammer on the back, from behind
- `back-side`: Hammer on the back, from the side
- `drop-flat`: A dropped hammer on flat ground
- `drop-slope`: A dropped hammer on a slope

## hj-automag-080: HJ MAGNUM 5 SHOT

File `hj-automag-080.txt`, 1 tests.

The HJ MAGNUM kills with five shots to the head: the first four take the shield down (the pop shows on the 4th) and

- `hj-5shot`: HJ MAGNUM: the fifth shot to the head kills

## host-vote-rc2: MAP VOTE FOR A GAME YOU HOST

File `host-vote-rc2.txt`, 3 tests.

rc2 MAP + GAMETYPE VOTE FOR A GAME YOU HOST: a game hosted from CREATE GAME with a playlist runs the same vote between games a

- `vote-host-alone`: One PC: the host votes, the winner is next
- `vote-two-host`: HOST PC: hosts, then votes with the joiner
- `vote-two-join`: JOINING PC: joins the host and votes

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

## map-pictures-093: MAP PICTURES: stock maps

File `map-pictures-093.txt`, 3 tests.

0.9.3 map pictures: the stock maps' pictures show on AE's own screens. They live in the game's menu data, not in a

- `tournament-stock`: TOURNAMENT: a stock map shows its picture
- `change-map-stock`: CHANGE MAP: a stock map's card
- `community-map`: TOURNAMENT: a community map keeps its picture

## maps-dredwerkz-094: DREDWERKZ: THE PIT KILLS, THE LIFT LIFTS

File `maps-dredwerkz-094.txt`, 3 tests.

0.9.4 DREDWERKZ: the pit and the lifts. The map names its pit's floor "Hell" and a lift "TPA" to "TPB" but its own

- `pit`: DREDWERKZ: the pit kills
- `lift`: DREDWERKZ: the lift takes you up
- `bots-pit`: DREDWERKZ: no bots in the pit

## maps-hub-10x: MAPS & MODS: THE PAGES

File `maps-hub-10x.txt`, 19 tests.

MAPS & MODS: the main menu's MAPS & MODS screen under a tab strip of five pages (LB / RB, Q / E, a click): INSTALLED, REQUIRED

- `installed-list`: INSTALLED: the list and the card
- `installed-options`: INSTALLED: A opens a map's options
- `installed-folder`: INSTALLED: move a map to a folder
- `installed-delete`: INSTALLED: the delete confirm
- `installed-in-use`: INSTALLED: a map a lobby has
- `installed-loading`: INSTALLED: reading the map folders
- `installed-empty`: INSTALLED: a source with no maps
- `required-missing`: REQUIRED FILES: a file is missing
- `required-found`: REQUIRED FILES: everything is there
- `browse-consent`: BROWSE CE MAPS: the first-use card
- `browse-results`: BROWSE CE MAPS: the map list
- `browse-download`: BROWSE CE MAPS: a map installing
- `browse-offline`: BROWSE CE MAPS: no answer from the archive
- `browse-disk-full`: BROWSE CE MAPS: not enough room
- `workshop-list`: WORKSHOP: the Workshop list under the tab strip
- `workshop-signed-out`: WORKSHOP: signed out
- `mods-list`: MODS: the list
- `mods-moving`: MODS: moving a mod
- `mods-restart`: MODS: the modded disc needs a restart

## maps-hub-11x: MAPS & MODS: THE ACTIONS

File `maps-hub-11x.txt`, 9 tests.

MAPS & MODS, the actions that were not wired: CREATE A GAME ON IT and PLAY IT open CREATE GAME and CAMPAIGN on the map, NEW FOLDER takes

- `create-game`: CREATE A GAME ON IT opens CREATE GAME on the map
- `play-it`: PLAY IT opens CAMPAIGN on that level
- `new-folder`: NEW FOLDER takes a typed name
- `open-folder`: OPEN ITS FOLDER opens the map's own folder
- `install-again`: Installing a map this PC has asks first
- `cancel-any-row`: Cancel stops the install from any row
- `steam-page`: OPEN ON STEAM on a made-up map (the page is not opened)
- `signed-out-footer`: Signed-out WORKSHOP offers only what works
- `mod-mouse`: A click moves the mod being moved

## mark-names: MARKS, SOLO

File `mark-names.txt`, 3 tests.

MARKS, solo: aim, reading and the shared button. Keyboard B marks; a controller taps the flashlight button.

- `enemy-foot`: Marking enemies on foot
- `item-names`: What a mark says
- `enemy-driving`: Marking from the driver's seat

## mark-spike: MARKS, SOLO

File `mark-spike.txt`, 11 tests.

MARKS, solo: aim, reading and the shared button. Keyboard B marks; a controller taps the flashlight button.

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

MEDALS, part 2, on testbox: 36 more medals. A medal with no picture in MCC's folder shows as its name; note any that

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

## melee-ae-081: MELEE RULE: CLASSIC / MODERN

File `melee-ae-081.txt`, 11 tests.

The MELEE rule (GAME OPTIONS, SPARTAN: MELEE, CLASSIC / MODERN) on Octagon 2 with a dummy that stands still: MODERN's

- `melee-pistol`: MELEE MODERN with the magnum: shots, then the blow
- `pop-magnum`: MELEE MODERN: the shield pop with the magnum
- `pop-rifle`: MELEE MODERN: the shield pop with the assault rifle
- `pop-smg`: MELEE MODERN: the shield pop with the SMG
- `pop-plasma`: MELEE MODERN: the shield pop with the plasma rifle
- `melee-reach-magnum`: MELEE MODERN: the reach with the magnum
- `melee-reach-rifle`: MELEE MODERN: the reach with the assault rifle
- `melee-classic`: MELEE CLASSIC: Halo CE's melee as before
- `melee-br`: MELEE MODERN with the battle rifle: bursts, then the blow
- `melee-behind`: MELEE MODERN from behind
- `melee-mate`: MELEE MODERN on a teammate

## menu-chief-10x: PROFILES: THE SPARTAN

File `menu-chief-10x.txt`, 1 tests.

PROFILES > EDIT: the profile's Spartan drawn under the COLOURS grid, the game's own model in the row's colour. The

- `edit-spartan`: EDIT: the Spartan under the colours

## menu-chief-11x: PROFILES: THE SPARTAN'S SIDE

File `menu-chief-11x.txt`, 4 tests.

PROFILES and its EDIT page with the profile's Spartan: one centred column on the right side. He stands on a floor

- `edit-page`: EDIT: he stands on the colours
- `edit-colours`: EDIT: the colours have the focus
- `list-page`: PROFILES: the focused profile's Spartan
- `edit-plain`: EDIT: a PC with no Spartan model

## menu-order-093: MENU ORDER

File `menu-order-093.txt`, 3 tests.

0.9.3 main menu order: CAMPAIGN, AE TOURNAMENT, MULTIPLAYER, SETTINGS, then the rest. HOST GAME is a MULTIPLAYER row now

- `main-order`: MAIN MENU: the row order
- `host-game-in-multiplayer`: MULTIPLAYER: HOST GAME
- `classic-card-text`: CLASSIC COMP: the card's get-the-rest lines

## menus-phase1: MENUS PHASE 1

File `menus-phase1.txt`, 8 tests.

Menus phase 1 (0.9.0 line): the main menu's new rows and COMING SOON group, MULTIPLAYER with its descriptions in the

- `main-rows`: MAIN MENU: the rows and COMING SOON
- `main-keys`: MAIN MENU: B, START, Y
- `multiplayer-rows`: MULTIPLAYER: rows with their descriptions
- `matchmaking-lobby`: MATCHMAKING: the playlist lobby
- `party-two`: PARTY: two players, hold B to remove
- `party-full`: PARTY: full at 4/4
- `motd-first`: MESSAGE OF THE DAY: at launch
- `whats-new`: WHAT'S NEW: releases and the badge

## music-rc2: MENU MUSIC: MODES, ONE SONG, MY PLAYLIST, NO MCC, HALO 3 MENU PIECES

File `music-rc2.txt`, 5 tests.

1.0.0-rc2 MENU MUSIC: MENU MUSIC and its two song screens, each opened on its scene.

- `music-menu`: MENU MUSIC: the modes
- `music-one-song`: ONE SONG: picking and previewing
- `music-menu-pieces`: HALO 3 main menu: its pieces
- `music-playlist`: MY PLAYLIST: the editor
- `music-no-mcc`: No MCC: the title theme only

## net-090-host: 0.9.0 TWO MACHINES: HOST

File `net-090-host.txt`, 2 tests.

0.9.0 two machines, the HOST's list (with V-SYNC as it is): each test hosts its game and stands the host's player at

- `host-head-check`: HEAD CHECK: real headshots stay headshots
- `host-hit-cover`: HIT COVER: nothing through a wall

## net-090-join: 0.9.0 TWO MACHINES: JOIN

File `net-090-join.txt`, 1 tests.

0.9.0 two machines, the JOINER's list: its one test joins the host's game (net-090-host.txt on the host, started

- `join-clock-slew`: Join the host's game; a 1 s hitch 40 s in

## next-082a: 0.8.2, ROUND A

File `next-082a.txt`, 4 tests.

0.8.2 TEST BUILD: the PARTY popover, the server browser's hidden-servers count, the menus' speed. Each test sets

- `party-popover`: PARTY: one panel, one row of buttons
- `browser-hidden`: SERVER BROWSER: hidden AE servers named
- `menu-time`: Menus on this PC: MENU and DRAW times
- `split-crosshair`: Four players: every view draws its own crosshair

## next-090-latency: 0.9.0, LOW LATENCY

File `next-090-latency.txt`, 3 tests.

0.9.0 TEST BUILD (a real monitor): LOW LATENCY OFF / ON / BOOST. Same map: one load; each test sets its mode.

- `latency-off`: LOW LATENCY OFF: the feel you know
- `latency-on`: LOW LATENCY ON: aim feels quicker, no stutter
- `latency-boost`: LOW LATENCY BOOST: quickest, still smooth

## next-090a: 0.9.0, ROUND A

File `next-090a.txt`, 4 tests.

0.9.0 TEST BUILD: greyed rows, the crosshair under the pause menu, four players' crosshairs on 0.9.0's split screen.

- `greyed-row`: SETTINGS: a greyed row still reads greyed
- `pause-xh-off`: Pause menu, TARGET 60 FPS off: crosshair stays put
- `pause-xh-on`: Pause menu, TARGET 60 FPS on: crosshair stays put
- `split-four`: Four players: crosshairs and split graphics

## next-092a: 0.9.2 ROUND A

File `next-092a.txt`, 16 tests.

0.9.2: the new SETTINGS rows and the one WEAPON PACKS row (menus tests: each check says where to look), the TEST RUN

- `settings-audio`: SETTINGS: AUDIO, the new rows
- `settings-online-files`: SETTINGS: ONLINE and SYSTEM, the new rows
- `packs-one-row`: GAME OPTIONS: one WEAPON PACKS row
- `card-hide`: TEST RUN card: End toggles it
- `blood-wall`: DYNAMIC BLOOD: on the wall behind
- `riding-bot`: VEHICLE RIDING: on a Warthog a bot drives
- `w-smg-tactical`: Tactical SMG: spawn with it, kill the dummy
- `w-pdw`: Burst PDW: spawn with it, kill the dummy
- `w-revolver`: Heavy revolver: spawn with it, kill the dummy
- `w-focus-beam`: Focus beam: spawn with it, kill the dummy
- `w-spiker`: Spiker: spawn with it, kill the dummy
- `w-ravager`: Ravager: spawn with it, kill the dummy
- `w-auto-shotgun`: Auto shotgun: spawn with it, kill the dummy
- `w-grenade-launcher`: Grenade launcher: spawn with it, kill the dummy
- `w-concussion-rifle`: Concussion rifle: spawn with it, kill the dummy
- `w-gravity-hammer`: Gravity hammer: spawn with it, kill the dummy

## next-092b: 0.9.2 ROUND B

File `next-092b.txt`, 7 tests.

0.9.2, on the release weapon cache (serial 4: the shipped weapons, the Protomag and the HEAVY REVOLVER; the other nine

- `start-list`: START WEAPONS: every weapon the cache has
- `start-both-slots`: START WEAPONS: the same weapon in both slots
- `reverb-row`: SETTINGS: REVERB LEVEL
- `reverb-guns`: REVERB LEVEL: gunfire indoors at 50%
- `revolver-start`: HEAVY REVOLVER: the release cache's
- `split-flash`: SIDE BY SIDE: the muzzle flash at the barrel
- `bots-elite`: BOTS: an ELITE game on Temple

## next-092c: 0.9.2 ROUND C

File `next-092c.txt`, 8 tests.

0.9.2: A DEV speed readout and a speed to try, the side-by-side weapon centred on the crosshair, and the bots' three

- `speed-readout`: SPEED: the readout, normal speed
- `speed-105`: SPEED: 105 %
- `speed-110`: SPEED: 110 %
- `speed-115`: SPEED: 115 %
- `split-aim-br`: SIDE BY SIDE: the rifle points at the crosshair
- `split-aim-sniper`: SIDE BY SIDE: the sniper's trail
- `bots-wall`: BOTS: ELITE on Temple, walls and peeking
- `bots-range`: BOTS: ELITE at battle rifle range

## next-092d: 0.9.2 ROUND D

File `next-092d.txt`, 5 tests.

0.9.2: TARGET 60 FPS. Under V-SYNC it counts missed refreshes (a 16 ms frame is not slow), tries a step back up after

- `target60-one`: TARGET 60 FPS: one player
- `target60-split`: TARGET 60 FPS: two players side by side
- `target60-off`: TARGET 60 FPS off: the same lap to compare
- `split-aim-sniper`: SIDE BY SIDE: the sniper's trail
- `god-mode`: DEV: god mode against bots

## next-092e: 0.9.2 ROUND E

File `next-092e.txt`, 1 tests.

0.9.2: the sniper rifle side by side. Staged only.

- `split-aim-sniper`: SIDE BY SIDE: the sniper's trail

## next-092f: 0.9.2 ROUND F

File `next-092f.txt`, 3 tests.

0.9.2: two laps to compare with every display setting the same, and the loading screen on NO WAIT. Do not open

- `lap-a`: LAP A: run two laps, shoot the bots
- `lap-b`: LAP B: the same two laps
- `loading-none`: LOADING SCREEN on NO WAIT: none at all

## next-092k: 0.9.2 ROUND K

File `next-092k.txt`, 1 tests.

0.9.2: the heavy revolver is held back and the weapon cache is the release's. One staged test: the cache weapons play,

- `cache-weapons`: WEAPON CACHE: battle rifle in hand, no revolver

## next-093c: 0.9.3 ROUND C

File `next-093c.txt`, 4 tests.

0.9.3: LOW LATENCY (waits again only under 65% of the refresh; turns itself off for the game with a log line when it

- `low-latency-one`: LOW LATENCY on: one player
- `low-latency-split`: LOW LATENCY on: two players side by side
- `options-reset-again`: CHANGED tab: RESET EVERY OPTION after another scene
- `preset-performance`: GRAPHICS PRESET: PERFORMANCE

## opence-387-100: OPENCE 387: SHIELD LOOK

File `opence-387-100.txt`, 4 tests.

1.0.0-rc1: OpenCE build-387's shield look. ARM SHIELDS (display.viewmodel_shield, off by default) draws the shield's

- `arms-default`: ARM SHIELDS off (the default)
- `arms-on`: ARM SHIELDS on
- `overshield-arms`: Overshield on the arms
- `hit-feedback-arms`: AE HIT FEEDBACK on the arms

## options-changed-093: GAME OPTIONS: CHANGED VIEW

File `options-changed-093.txt`, 6 tests.

0.9.3 GAME OPTIONS: a value is gray until you change it. A changed option shows in blue with a dot and the game type's

- `options-gray`: All options: gray until changed
- `options-changed-tab`: CHANGED tab: only what differs
- `options-reset-all`: CHANGED tab: RESET EVERY OPTION
- `options-empty`: CHANGED tab: nothing changed
- `options-value-tag`: ALL VALUES: the game type's own value
- `options-mouse-reset`: Mouse: click the struck value

## overshield-arms-rc1: RC1 RETEST: OVERSHIELD ON THE ARMS

File `overshield-arms-rc1.txt`, 1 tests.

1.0.0-rc1: the overshield on the arms. The setup gives over_shield (Damnation's item is named "over shield"), and an

- `overshield-arms`: Overshield on the arms

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

The GRAVITY HAMMER pack (the hammer of Grifball Beta, its art as the map has it; AE's classic-era numbers: one swing

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

## perf-audit-094: PERFORMANCE AUDIT

File `perf-audit-094.txt`, 3 tests.

0.9.4 performance audit: the same game, drawn with less work. The HUD's panels (cells, outlines, soft lines and

- `menus`: Menus: as before
- `hud-ctf`: HUD and CTF on Blood Gulch
- `next-map`: After a map change

## pickups-093: AE PICKUPS

File `pickups-093.txt`, 5 tests.

0.9.3 AE PICKUPS: in an AE game the weapons lying on the map are AE's (the starts are not touched). An assault rifle's

- `hang-pickups`: Hang 'em High: AE's weapons on the map
- `turf-halo2`: Turf: a Halo 2 map's own weapons
- `containment-layout`: Containment: Halo 2's weapons back
- `tournament-pickups`: AE TOURNAMENT lays AE's weapons
- `classic-keeps`: CLASSIC COMP keeps the maps' own

## playlists-094: PLAYLISTS

File `playlists-094.txt`, 3 tests.

0.9.4 PLAYLISTS: several named playlists in CREATE GAME, each with an ORDER (IN TURN, SHUFFLE, RANDOM), and what plays next shown. The

- `playlists-board`: PLAYLISTS: three lists, one in use
- `playlists-order`: A playlist's ORDER: in turn, shuffle, random
- `playlists-new`: NEW PLAYLIST and the migration

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

0.7.6: screens, the ELIMINATION and score strips, fireteams, bots. Each test sets itself up (map, gametype, options,

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

0.7.7 on the release tree: the server lobby and its vote on the live servers, HOST A SERVER on Linux, the crosshair

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

## records-columns: RC2 SERVICE RECORD: DETAIL WINDOW

File `records-columns.txt`, 9 tests.

rc2 SERVICE RECORD detail window: A on a box of a record page opens it on that box's page (MODES, COMBAT, WEAPONS & MAPS,

- `player-modes`: PLAYER RECORD: MODES
- `player-modes-kinds`: PLAYER RECORD: MODES, EVERY KIND
- `player-modes-sparse`: PLAYER RECORD: MODES, FEW KINDS
- `player-combat`: PLAYER RECORD: COMBAT
- `player-weapons`: PLAYER RECORD: WEAPONS & MAPS
- `player-medals`: PLAYER RECORD: MEDALS
- `bot-modes`: BOT RECORD: MODES
- `bot-old`: BOT RECORD: FROM BEFORE THE UPDATE
- `career-records`: CAREER: RECORDS, PLAYERS BESIDE BOTS

## records-rc2: RC2 SERVICE RECORD: PAGES

File `records-rc2.txt`, 13 tests.

rc2 SERVICE RECORD: three pages for a player and for a bot (OVERVIEW | COMBAT | MODES & MAPS, Y or Tab or a click on the chips),

- `player-overview`: PLAYER RECORD: OVERVIEW
- `player-combat`: PLAYER RECORD: COMBAT
- `player-modes`: PLAYER RECORD: MODES & MAPS
- `player-old-record`: PLAYER RECORD: from before the update
- `player-few-games`: PLAYER RECORD: 3 games carry the new numbers
- `bot-overview`: BOT RECORD: OVERVIEW
- `bot-combat`: BOT RECORD: COMBAT
- `bot-modes`: BOT RECORD: MODES & MAPS
- `bot-old-record`: BOT RECORD: from before the update
- `player-focus`: PLAYER RECORD: focus and details
- `bot-lt-rt`: BOT RECORD: LT / RT keep page and box
- `details-flag`: POST-GAME DETAILS: flag game
- `details-ffa`: POST-GAME DETAILS: free for all

## release-040: 0.4.0 CANDIDATE

File `release-040.txt`, 13 tests.

0.4.0: what a game can show of the 0.4.0 changes.

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

## revolver-093: 0.9.3 HEAVY REVOLVER

File `revolver-093.txt`, 3 tests.

0.9.3 HEAVY REVOLVER: its own reticle (design D) and its name where a mark, the pickup text and the kill feed show it.

- `revolver-reticle`: HEAVY REVOLVER: its own reticle
- `revolver-mark`: HEAVY REVOLVER: a mark names it
- `revolver-killfeed`: HEAVY REVOLVER: the kill feed

## round-1111: TEST ROUND 11-11: BOTS, CAMERAS, RAGDOLLS

File `round-1111.txt`, 17 tests.

COMBINED TEST (the 0.8.1 line, the MODERN ragdoll pushes, the 0.9.0 bots and the death

- `temple-easy`: TEMPLE 2V2: EASY bots
- `temple-heroic`: TEMPLE 2V2: HEROIC bots
- `temple-elite`: TEMPLE 2V2: ELITE bots
- `temple-legendary`: TEMPLE 2V2: LEGENDARY bots
- `temple-grenades`: TEMPLE: bots get out of the way of your grenades
- `temple-still`: TEMPLE: no bot stands still for long
- `camera-death-hold`: CAMERA: dying, eyes out to your body, killer in view
- `camera-death-again`: CAMERA: dying a second time in the same game
- `camera-follow-switch`: CAMERA: LB and RB switch with a quick fade
- `spawn-no-grace`: PRISONER: HEROIC bots don't wait after you spawn
- `high-ground`: PRISONER: bots hold the high spots a while
- `teleporter-fight`: CHILL OUT: a bot fighting you stays in the fight
- `weapon-range`: CHILL OUT: bots pick sniper or pistol by distance
- `ragdoll-modern`: RAGDOLL MODERN: tips back high, swept low
- `ragdoll-mixed`: RAGDOLL MIXED: the stock fall, then loose
- `ragdoll-exaggerated`: RAGDOLL EXAGGERATED: loose and wild
- `ragdoll-spaghetti`: RAGDOLL SPAGHETTI WESTERN: a stiff woody fall

## round-rc1: 1.0.0-rc1 ROUND: 387, CAMERA, ROCKETS, UI

File `round-rc1.txt`, 10 tests.

1.0.0-rc1 ROUND: everything since 0.9.4.1 in one build.

- `scoreboard-split-two`: SCOREBOARD, two players: hold BACK
- `scoreboard-split-four`: SCOREBOARD, four players: hold BACK
- `pause-1080`: PAUSE at 1920x1080: press START
- `arms-default`: ARM SHIELDS off (the default)
- `arms-on`: ARM SHIELDS on
- `overshield-arms`: Overshield on the arms
- `hit-feedback-arms`: AE HIT FEEDBACK on the arms
- `names-close`: NAMES over three players standing close
- `rocket-use`: DAMNATION: bots fire the rockets they hold
- `camera-death-hold`: CAMERA: dying, eyes out to your body, killer in view

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

## seeds-rc2: SEEDS RC2

File `seeds-rc2.txt`, 7 tests.

rc2 seeds: every AE CASUAL and BTB gametype (EVO, battle rifle and pistol starts too) has SHOOTABLE GRENADES on; the capture the

- `grenade-casual`: Casual: a grenade on the ground goes off
- `grenade-comp`: Comp: the same grenade does not
- `flag-hold`: Flag pickup: hold the action button
- `flag-no-drive`: Carrier: no driving
- `own-flag-away`: Own flag away from home: its waypoint shows
- `flag-melee`: Carrier: a blow with the flag kills
- `digsite-protomag`: DIGSITE BIG TEAM starts with the PROTOMAG

## settings-search-093: 0.9.3 SETTINGS: FIND A SETTING

File `settings-search-093.txt`, 5 tests.

0.9.3 SETTINGS: FIND A SETTING (ae/settings-search-093). The field heads SETTINGS' top level; typing lists every setting that

- `find-results`: FIND: "zoom", the results and the panel
- `find-none`: FIND: nothing matches
- `find-jump`: FIND: A goes to the setting
- `find-pad-keyboard`: FIND: a pad types with AE's keyboard
- `find-split`: FIND: player 2 searches

## shaders-093: SHADERS: A FRESH CACHE

File `shaders-093.txt`, 1 tests.

0.9.3 shader prep: a map's shaders are made while its loading screen is up (one compile thread, the map's own list

- `fresh-cache`: SHADERS: the first load with nothing cached

## smg-view-094: SMG VIEW

File `smg-view-094.txt`, 2 tests.

0.9.4 SMG VIEW: the SMG and the SUPPRESSED SMG move as one piece with the arms 6 units toward the camera and half a

- `smg-view`: SMG: the tactical SMG's place
- `smg-silenced-view`: Suppressed SMG: the same place

## social-career-093: AE TOURNAMENT

File `social-career-093.txt`, 15 tests.

0.9.3 AE TOURNAMENT: offline games against bots with a rank 1 to 50. It has its own main menu row (above MULTIPLAYER): six

- `tournament-rows`: AE TOURNAMENT: the screen
- `tournament-maps-line`: AE TOURNAMENT: maps missing
- `tournament-dev-line`: AE TOURNAMENT: the DEV line
- `lobby-auto`: TOURNAMENT LOBBY: AUTO
- `lobby-fixed`: TOURNAMENT LOBBY: FIXED
- `roster-spread`: The roster's tiers
- `fallback-maps`: No pack maps: a stock map
- `quit-costs`: QUIT costs 1 EXP
- `postgame-win`: Post-game: a win
- `postgame-rankup`: Post-game: rank up
- `postgame-ffa`: Post-game: 5th of 8
- `postgame-split`: Post-game: split screen
- `career-view`: CAREER
- `crash-forgiven`: A game ended by a crash is forgiven
- `close-costs`: A closed game counts as a quit

## sound-mix-080: SOUND MIX: PISTOL SHOT, RIFLE ECHO, PAUSE

File `sound-mix-080.txt`, 4 tests.

Weapon sound mix (0.8.0): the starter pistol's new firing sound, the rifles' recorded echo faded, the reverb on pause.

- `sound-pistol`: Starter pistol: the new shot (Digsite E3 rifle's)
- `sound-br`: Battle rifle: short echo, the room does the rest
- `sound-dmr-evo`: DMR: the same short echo
- `sound-carbine`: Carbine: its 2.7 s tail cut to about 1 s

## split-090: 0.9.0 SPLIT SCREEN, TARGET 60 FPS

File `split-090.txt`, 11 tests.

0.9.0 split screen: two and four players on one screen, the two-player layouts (top and bottom, side by side, side

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

## split-join-10x: SPLIT SCREEN: JOIN IN PROGRESS

File `split-join-10x.txt`, 3 tests.

SPLIT SCREEN JOIN IN PROGRESS: a pad of this PC joins a game already running, as its own view, and leaves again. The

- `join-leave`: Player 2 joins the game under way, then leaves
- `join-card`: A pad's START opens the join card
- `join-off`: The host keeps joining off

## split-join-11x: SPLIT SCREEN: THE JOIN CARD

File `split-join-11x.txt`, 2 tests.

SPLIT SCREEN JOIN CARD: the card a pad with no player gets from its START shows the seat's tile in the seat's colour with the seat

- `card-shown`: A pad's START opens the join card
- `card-refused`: The host keeps joining off: the card says so

## steam-rc2: 1.0.0-rc2 STEAM ACCOUNT

File `steam-rc2.txt`, 13 tests.

1.0.0-rc2 STEAM ACCOUNT: sign in with Steam on the desktop for MCC's shared map files and the Halo 1

- `signed-out`: STEAM ACCOUNT: signed out
- `typing`: STEAM ACCOUNT: the password as stars
- `wrong-password`: STEAM ACCOUNT: Steam refused the password
- `guard-code`: STEAM GUARD: the code
- `guard-app`: STEAM GUARD: approve in the app
- `files-missing`: SIGNED IN: MCC's shared files missing
- `files-downloading`: SIGNED IN: downloading, 37 %
- `files-ready`: SIGNED IN: all three files here
- `not-owned`: SIGNED IN: the account does not own MCC
- `log-out`: LOG OUT: the confirm
- `workshop`: WORKSHOP MAPS: a made-up page
- `first-start`: FIRST START: MCC FROM STEAM
- `real-sign-in`: YOUR ACCOUNT: the real sign-in

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

## tab-pages-094: TAB MENU PAGES

File `tab-pages-094.txt`, 7 tests.

0.9.4 TAB MENU PAGES: the TAB / START menu has two new pages before QUIT, CREDITS and NETWORK STATUS. CREDITS scrolls a text

- `tab-rows`: TAB menu: the two new rows
- `credits-pad`: CREDITS: scroll and back, pad
- `credits-keys-mouse`: CREDITS: scroll and back, keys and mouse
- `network-reached`: NETWORK STATUS: master server reached
- `network-unreached`: NETWORK STATUS: master server not reached
- `network-unknown`: NETWORK STATUS: nothing known
- `network-live`: NETWORK STATUS: this PC's own state

## tac-smg-ship-094: TACTICAL SMG SHIPS

File `tac-smg-ship-094.txt`, 1 tests.

0.9.4: the TACTICAL SMG ships (registry row 15 active, the release weapon cache serial 6 carries it). Checks on the

- `tac-smg-ship`: Tactical SMG: in the release cache

## theater-100: THEATER: RECORD A GAME, WATCH IT BACK

File `theater-100.txt`, 4 tests.

The Theater's engine: a bot game recorded, then played back through a player's eyes and a free camera, a seek

- `record`: BLOOD GULCH: play 30 s with the bots, then pass
- `eyes`: BLOOD GULCH replay: through a player's eyes
- `free`: BLOOD GULCH replay: the free camera
- `end`: BLOOD GULCH replay: seek ahead, then the end

## theater-101: THEATER: RECORD A GAME, WATCH IT BACK

File `theater-101.txt`, 4 tests.

The Theater's engine: a bot game recorded, then played back through a bot's eyes and a free camera, a seek and the

- `record`: BLOOD GULCH: play a minute with the bots, then pass
- `eyes`: BLOOD GULCH replay at 15 s: a bot's eyes
- `free`: BLOOD GULCH replay at 15 s: the free camera
- `end`: BLOOD GULCH replay: seek to 50 s, then the end

## tournament-starts-093: TOURNAMENT STARTS

File `tournament-starts-093.txt`, 8 tests.

0.9.3 AE TOURNAMENT: mixed starts and AE's weapons. A game of a casual playlist starts with the battle rifle or the evo rifle

- `variant-lobby`: The lobby names the start
- `evo-in-hand`: Evo start, AE weapons
- `packs-off-back`: WEAPON PACKS off is forced on, then back
- `comp-lobby`: AE COMP TEAM SLAYER lobby
- `comp-pistol-hand`: Comp pistol start
- `classic-lobby`: CLASSIC COMP lobby on a classic map
- `classic-own-weapons`: CLASSIC COMP plays the maps' own weapons
- `filter-chips`: CASUAL / COMP chips on AE TOURNAMENT

## ui-look-rc1: UI FIRST LOOK rc1: SCOREBOARD, PAUSE, NAMES

File `ui-look-rc1.txt`, 4 tests.

UI FIRST LOOK rc1: the scoreboard, the pause menu and the names over heads on the AE HUD and menus. Each test opens on

- `scoreboard-split-two`: SCOREBOARD, two players: hold BACK
- `scoreboard-split-four`: SCOREBOARD, four players: hold BACK
- `pause-1080`: PAUSE at 1920x1080: press START
- `names-close`: NAMES over three players standing close

## ux-parity-094: INPUT PARITY

File `ux-parity-094.txt`, 11 tests.

0.9.4 INPUT PARITY: the menus work the same on a pad and on mouse + keyboard. Each test is checked on the pad first, then on

- `main-keys`: MAIN MENU: START, Tab and the footer
- `mouse-brush`: MAIN MENU: a touchpad brush keeps the pad
- `party-seat-text`: PARTY: the add-a-player text
- `matchmaking-start`: AE TOURNAMENT: START by pad and by F2
- `tournament-wheel`: AE TOURNAMENT at 150 %: wheel and rows
- `browser-lan`: SERVER BROWSER: paging, filters, one click
- `lobby-roster`: SERVER LOBBY: the roster scrolls
- `playlist-reorder`: PLAYLIST: reorder by pad, keys and mouse
- `typing-right-click`: JOIN BY ADDRESS: right-click while typing
- `gametype-delete-cap`: CHANGE GAMETYPE: the Delete prompt's cap
- `pause-bots-mouse`: PAUSE > BOTS: the arrows go both ways

## vehicle-balance-1: VEHICLE BALANCE

File `vehicle-balance-1.txt`, 13 tests.

Vehicle balance, part 1: what hurts a vehicle and in how many shots (VEHICLES DESTRUCTIBLE ON, every AE gametype with

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

Vehicle balance, part 2 (0.7.6): small arms hurt a vehicle, as explosives do. VEHICLES DESTRUCTIBLE ON (AE TEAM SLY),

- `pistol-warthog`: Pistol on an empty Warthog
- `rifle-warthog`: Assault rifle on an empty Warthog
- `sniper-ghost`: Sniper rifle on an empty Ghost
- `small-arms-scorpion`: Small arms on the Scorpion
- `rockets-as-before`: Rockets, as before

## vote-rc2: MAP VOTE: AE TOURNAMENT AND HOSTED LOBBY

File `vote-rc2.txt`, 7 tests.

MAP VOTE rc2: the AE TOURNAMENT lobby's vote (three maps of the playlist, 10 s, the bots vote by their weights, the most votes

- `aet-vote-open`: AE:T VOTE: three maps, you vote, bots voted
- `aet-vote-late`: AE:T VOTE: 3 s left, every bot in
- `aet-vote-winner`: AE:T VOTE: the winner, then STARTING IN 5
- `aet-vote-tie`: AE:T VOTE: a tie is drawn
- `aet-vote-split`: AE:T VOTE: two players on this PC
- `aet-vote-43`: AE:T VOTE: narrow card at 4:3
- `hosted-lobby-vote`: HOSTED LOBBY: no map vote (a server runs it)

## waypoints-rc2: ITEM WAYPOINTS: ON THE MAP UNTIL TAKEN

File `waypoints-rc2.txt`, 3 tests.

ITEM WAYPOINTS rc2: a power item's waypoint shows while the item lies on the map, from the game's start, until it is

- `start`: WAYPOINTS: on every power item at the start
- `taken`: WAYPOINTS: take one, its waypoint goes
- `countdown`: WAYPOINTS: 10 s before it comes back (item respawn: no waiting)

## weapons-b1-092: WEAPONS BATCH 1

File `weapons-b1-092.txt`, 4 tests.

0.9.2 weapons batch 1: BURST PDW (16), HEAVY REVOLVER (17), SPIKER (19), RAVAGER (20), one test each on Octagon 2.

- `pdw-bursts`: Burst PDW: 3 bursts close
- `revolver-slugs`: Heavy revolver: 3 slugs, the last to the head
- `spiker-hold`: Spiker: hold the trigger
- `ravager-taps`: Ravager: 3 taps kill

## weapons-polish-094a: WEAPONS POLISH 1

File `weapons-polish-094a.txt`, 5 tests.

0.9.4 weapons polish, part 1 (the quick fixes): TACTICAL SMG (15), BURST PDW (16), SPIKER (19), GRENADE LAUNCHER (22),

- `tac-smg-view`: Tactical SMG: view and zoom
- `pdw-retune`: Burst PDW: view and strength
- `spiker-bounce`: Spiker: view and bounces
- `launcher-hold`: Grenade launcher: hold cap
- `concussion-buff`: Concussion rifle: slight buff

## weapons-polish-094b: WEAPONS POLISH 2

File `weapons-polish-094b.txt`, 4 tests.

0.9.4 weapons polish, part 2: the TACTICAL SMG (15) fires the BURST PDW's bursts with its own 2x zoom, the SUPPRESSED

- `tac-smg-fire`: Tactical SMG: the PDW's bursts
- `tac-smg-look`: Tactical SMG: scope and holster
- `concussion-view`: Concussion rifle: further back
- `reverb-indoor`: Indoor reverb: mixed down

## ws-collections-10x: WORKSHOP: COLLECTIONS

File `ws-collections-10x.txt`, 10 tests.

MAPS & MODS > WORKSHOP > SHOW: COLLECTIONS. Every test opens on its scene from DEV MODE's `menu maps workshop <scene>`, from

- `show-row`: SHOW has a fourth value, COLLECTIONS
- `list`: The list: the playlists' two, then the others
- `opened`: A collection opened: GET is the first row
- `map-row`: A map of the collection has its own card
- `getting`: A collection being got
- `installed`: A collection all on this PC, two updates
- `no-room`: Not enough room: nothing starts
- `get-question`: GET asks first: a pack of 500 MB or more
- `remove-question`: REMOVE asks first and keeps Steam's maps
- `signed-out`: Signed out: the playlists' collections count

## ws-maps-load-11x: WORKSHOP MAPS: DOES EACH ONE LOAD

File `ws-maps-load-11x.txt`, 29 tests.

The 29 Social Map Pack and Pro Edition Workshop items, each hosted from the game's own download layout (mcc_maps): the

- `load01`: ascetic mcc debug flycam pm
- `load02`: blamitemines mcc flycam pm
- `load03`: blood_covenant_mcc flycam pm
- `load04`: co2 mcc debug flycam pm
- `load05`: cold_shoulder mcc flycam pm
- `load06`: delta_ruined flycam mcc
- `load07`: dreadnought
- `load08`: dredwerkz mcc debug flycam pm
- `load09`: fissurefall mcc dbug flycam pm
- `load10`: fractured peak mcc flycam pm
- `load11`: frigid_keep dbug flycam mcc pm
- `load12`: gothra mcc debug flycam pm
- `load13`: gridiron mcc debug flycam pm
- `load14`: gutter mcc debug flycam pm nm
- `load15`: hektik mcc debug flycam pm
- `load16`: intensity_v2 debug flycam pm
- `load17`: nitra_v2 debug flycam pm
- `load18`: pat_beavercreek
- `load19`: pat_chillout
- `load20`: pat_damnation
- `load21`: pat_downrush
- `load22`: pat_hangemhigh
- `load23`: pat_longest
- `load24`: pat_prisoner
- `load25`: pat_ratrace
- `load26`: pat_wizard
- `load27`: ruins_of_alph mcc flycam pm
- `load28`: sympathy mcc debug flycam pm
- `load29`: warren_mcc debug flycam pm
