# Arena Evolved's gametypes

They are in the gametype list the first time you play. Back to the [README](../README.md).

- **Arena Evolved (casual):** FFA AE SLAYER (25 kills), TEAM AE SLAYER (50),
  TEAM AE CTF (3 captures), TEAM AE KING, FFA AE ODDBALL and TEAM AE ODDBALL
  (5 minutes held; the ball carrier's melee kills in one hit, BALL MELEE LETHAL), each with a 15-minute time limit, the motion sensor, the
  stock respawns, item TIMERS with waypoints, and both weapons dropped on death
  (DROP SECONDARY ALWAYS). The 2V2 ones (TEAM AE 2V2 SLAYER to 25, CTF, KING
  and ODDBALL, with the lethal ball) play the same rules; the player limit of 4 is the host's setting.
  FFA AE PRACTICE: every weapon and powerup each 30 seconds, 500 kills, no time
  limit, the map's vehicles. All of Arena Evolved's gametypes play COMBAT
  FEEDBACK: ARENA EVOLVED.
- **Arena Evolved free for all variants** (the stock free for all gametypes on
  the casual rules, 15 minutes, item TIMERS with waypoints, both weapons
  dropped; each keeps its own score, lives, ball, hill and vehicles): FFA AE
  ELIMINATION (one life), PHANTOMS (invisible), ENDURANCE (5 lives), ROCKETS,
  SNIPERS, JUGGERNAUT, REVERSE TAG, ACCUMULATION, STALKER, KING, CRAZY KING
  (the hill moves), RACE and RALLY (these two need a map with race flags).
  They keep the stock ball melee (the ball in these modes is never held in
  hand, so BALL MELEE LETHAL has nothing to act on) and their own motion
  sensor, so FFA AE ROCKETS, SNIPERS, PHANTOMS, REVERSE TAG, ACCUMULATION and
  STALKER have none. FFA AE JUGGERNAUT plays its own rules: to 15, the
  juggernaut with extra damage at normal speed, only the juggernaut's kills
  score, his killer takes the role, and a random player gets it when he kills
  himself or there is none.
- **Arena Evolved special modes** (team slayer to 50): SNIPERS (sniper rifle and
  pistol, the sniping weapon set, no motion sensor), SHOTTY SNIPERS (shotgun
  and sniper rifle, no weapons on the map), SWAT (pistols, no shields, classic
  health, no grenades), ROCKETS, SHOTGUNS and HEAVIES (75 kills, the map's
  vehicles).
- **Arena Evolved BTB** (big team, 8 against 8; seed revision 22): TEAM AE BTB SLAYER (100 kills), TEAM AE BTB CTF (3
  captures), TEAM AE BTB KING and TEAM AE BTB ODDBALL (5 minutes held, the lethal ball), each with a 20-minute time
  limit and the map's own vehicles, on the casual team rules (the stock respawns, the motion sensor showing enemies,
  item TIMERS with waypoints, the EVO RIFLE (seed revision 27) and an assault rifle, 2 frags); stored as AE BTB SLY, AE BTB CTF, AE BTB KING,
  AE BTB BALL. TEAM AE BTB HEAVIES (AE BTB HVY): big team slayer to 100 with the heavy vehicles, rocket warthogs and
  scorpions for both teams (each team's vehicles CUSTOM: as many as the map places, up to 4 of each; the Xbox maps
  have no rocket warthog, so they place their scorpions only). A dropped flag goes home after Halo CE's own 17
  seconds untouched (the gametype has no setting for it). Since seed revision 28 TEAM AE BTB CTF gives each team 3
  warthogs, 4 ghosts, 2 scorpions and 2 rocket warthogs (as many of each as the map has places for; the Xbox maps have
  no rocket warthog).
- **Arena Evolved BTB+** (seed revision 28): every AE BTB gametype again with **2 banshees a team**, in the AE BTB
  folder after its BTB gametype: TEAM AE BTB+ SLAYER, CTF, KING, ODDBALL and HEAVIES, stored as AE BTB+ SLY, AE BTB+ CTF,
  AE BTB+ KOH, AE BTB+ OB and AE BTB+ HVY (and the battle rifle versions AE BR BTB+S, C, K, B and H, the START switch's
  EVO, battle rifle and pistol starts as for the BTB ones). Score, time, spawns and starts are the BTB gametype's.
  The vehicles are each team's CUSTOM set: slayer, king and oddball the map's own warthogs, ghosts and scorpions (up to
  4 of each), capture the flag BTB CTF's 3 warthogs, 4 ghosts, 2 scorpions and 2 rocket warthogs, heavies its rocket
  warthogs and scorpions (up to 4 of each), and 2 banshees besides. A map with no place for a vehicle places none of
  it: banshees fly only where the map has banshee places (a Workshop map made for them).
- **Arena Evolved GRIFBALL and BOMB** (seed revision 24; the builds with Bomb, [bomb.md](bomb.md)), in AE ACTION
  SACK: TEAM AE GRIFBALL (AE GRIFBALL) is Grifball's official rules (melee only, one hit kills, no shields, 3-second
  respawns at your own goal, rounds of 3 minutes, first to 3 rounds) with AE's rules: the countdown, CONTROLLER
  CROUCH STAYS DOWN, MELEE UNZOOMS, MOUSE AIM ASSIST OFF. TEAM AE BOMB (AE BOMB) is Bomb (carry the bomb into the
  other team's goal and arm it for 3 seconds; first to 3 rounds) on AE CTF's casual team rules: 15 minutes, a pistol
  and an assault rifle, 2 frags, the motion sensor showing enemies at 10 m, item TIMERS with waypoints, no vehicles.
  Neither has a battle rifle version. Bomb is played only between machines whose builds have it (an OpenCE machine
  is dropped from the game with a message); bots do not play it.
- **Arena Evolved SEARCH AND DESTROY** (seed revision 26; the builds with Bomb), in AE ACTION SACK after AE BOMB: TEAM
  AE SEARCH AND DESTROY (AE SND) is AE BOMB with classic competitive Search and Destroy rounds: one life a round;
  the bomb spawns at the attackers' flag; the attackers win a round when it goes off or every defender is dead, the
  defenders when they defuse it, every attacker is dead before the plant, or the round's 2:30 pass with none
  planted. Plant 5 s, defuse 5 s, fuse 45 s, sides swap every 3 rounds, first to 4 rounds, a 6-second countdown
  before each round, no game time limit. The rule is a bit of the gametype (the Bomb variant's flags bit 7) that the
  host reads; the round numbers are its config rows (game.bomb_snd_*), and game.bomb_snd turns the rule on for any
  Bomb game as a test. No battle rifle version.
- **Arena Evolved ELIMINATION** (the round modes; 0.7.6, built in, seeded at the next seed revision): AE
  ELIMINATION (stored AE ELIM TM) and AE ELIMINATION COMP (AE C ELIM, AE COMP's start, the EVO RIFLE) play rounds of one
  life each on Slayer: a round is a 5-second frozen countdown at your team's start spawns (turning only), then live
  until one team has nobody standing (that team loses the round; both wiped on one tick is a tied round), then 4
  seconds and the next round. The team score is the round wins (kills score nothing); first to 5 of 9 rounds, RESOLVE
  TIES playing one more round at a time when 9 leave the wins level. From round 5 on the teams start at each other's
  start spawns. A player who dies goes DOWN where they fell: a teammate who holds the action button within a unit of
  the spot for 3 seconds brings them back there (each later revive of the same player in a round takes 1.5 times as
  long; letting go or stepping away starts it over; the hold takes no weapon). The DANGER ZONE closes every round:
  60 s, then 30 s closing to a tenth of its radius, 60 s, then 30 s to nothing (a closing lasts longer where its edge
  would outrun a walking player); outside it a player loses an eighth of full shield and health a second, and a death
  outside leaves no revive spot. Its centre is a map layout's `zone` line, else a hill marker fairly between the
  teams' starts, else the middle of the starts. A player who joins during a round waits for the next, the follow
  camera on a teammate. Only Arena Evolved machines join (the host refuses others, as TERRITORIES does). The free for
  all AE ELIMINATION of the NEW MODES presets is now AE ELIMINATION CLASSIC/CE (AE ELIMX; FFA AE ELIMINATION, AE ELIM,
  stays as it was): its rules (free for all, best of 5, 120-second
  rounds, one life) on the same round machine, with no zone (a time-out is a tied round). The settings are the AEXV
  block's mode ELIMINATION, NUMBER OF ROUNDS best of 9 and its round bytes (`port/linux/game/ae_variant_ext_rules.h`);
  DEV MODE shows a line of the round's state until the HUD for it is made, and has the round, zone, down and revive
  commands.
- **Motion sensor, enemies only** (0.3.0): the team gametypes above that have the
  motion sensor (and their AE BR twins) show only enemies on it, never your
  teammates (OTHER PLAYERS ON RADAR: ENEMIES, in Indicator Options); the free
  for all ones show every other player, as ENEMIES does there.
- **Arena Evolved BR** (0.3.0): the casual set, the free for all variants, FFA AE PRACTICE and AE COMP again with the
  **battle rifle in hand** and an assault rifle second (FFA AE BR SLAYER, TEAM AE BR SLAYER, TEAM AE BR COMP CTF, ... 25
  gametypes; stored as AE BR FFA, AE BR TS, AE BR C CTF, ...; seed revision 22 adds the BTB and COMP 2V2 sets' nine:
  TEAM AE BR BTB SLAYER, ..., TEAM AE BR COMP 2V2 SLAYER, ..., stored as AE BR BTB S/C/K/B/H and AE BR C2 TS, AE BR
  C2CTF, AE BR C2KOH, AE BR C2 OB). Every other value is its twin's. Not made for the
  single-weapon modes (snipers, rockets, shotguns, SWAT, heavies, shotty snipers), RACE, RALLY or TRAINING. They
  need the battle rifle's pack (the `packs` folder): only machines with it join; on a Custom Edition map without the
  stock tags the pack borrows, and where the pack is not, the slot plays an assault rifle.
- **Arena Evolved COMP**: FFA AE COMP SLAYER (25 kills), TEAM AE
  COMP SLAYER (50), TEAM AE COMP CTF (3 captures), TEAM AE COMP KING and TEAM
  AE COMP ODDBALL (5 minutes held, with the lethal ball). 5-second respawn and suicide penalty, the EVO RIFLE (seed revision 27) and
  an assault rifle, 2 frag grenades, no motion sensor, no vehicles (capture the flag, since seed revision 28, plays the
  map's own), item TIMERS only in line of
  sight (no list, no item calls), the objective only in line of sight, both
  weapons dropped; the slayers have no time limit, the others 15 minutes.
  The same four team ones for two against two (seed revision 22): TEAM AE COMP 2V2 SLAYER (25 kills), CTF (3
  captures), KING and ODDBALL (5 minutes held), stored as AE C2V2 TS, AE C2V2 CTF, AE C2V2 KOH, AE C2V2 OB.
- **OCTAGON** (0.3.5): four 1v1 practice drills, made for Octagon: FFA AE OCTAGON BR (the battle rifle alone), FFA AE
  OCTAGON BR SNIPE (battle rifle and sniper rifle), FFA AE OCTAGON MAG (the magnum alone) and FFA AE OCTAGON MAG SNIPE
  (magnum and sniper rifle); stored as AE OCT BR, AE OCT BRSN, AE OCT MAG, AE OCT MGSN. Free for all slayer on the
  casual set's values (25 kills, 15 minutes, the motion sensor, item TIMERS) with instant respawns, no grenades, and
  INFINITE AMMO and BOTTOMLESS CLIP: a magazine never empties and never needs a reload. The BR ones need the battle
  rifle's pack, as the AE BR set. The same four as team drills, to warm up with a team: TEAM AE OCTAGON BR, BR SNIPE,
  MAG and MAG SNIPE (stored as AE OCT TBR, AE OCT TBSN, AE OCT TMAG, AE OCT TMSN), team slayer on TEAM AE SLAYER's
  values (50 kills, 15 minutes, the motion sensor showing enemies) with the same drill rules.
- **FFA AE TRAINING:** item TIMERS and TRAINING's markers, plays to 500, no
  time limit.
- Every Arena Evolved gametype has HALO 2 health (SWAT:
  Halo CE), NO SPREAD FIRST SHOT, the PRE-GAME COUNTDOWN and generic starting
  equipment, with a pistol in hand and an assault rifle unless its mode says
  otherwise. Since seed revision 22 they also have MELEE UNZOOMS ON (a melee
  while zoomed takes the zoom off and strikes; Halo CE has no
  melee while zoomed) and MOUSE AIM ASSIST OFF (no aim assist for the mouse,
  for every player; controllers keep theirs). Halo 1: NHE's set and the stock
  gametypes keep Halo CE's melee and allow the mouse's aim assist.
  Since seed revision 23 every AE gametype with vehicles has VEHICLES
  DESTRUCTIBLE ON. Known issue (0.7.5): only explosives (grenades, rockets,
  tank shells, fuel rods) damage the stock vehicles; small arms and vehicle
  guns do not yet, fixed in 0.7.6. The balance table
  (`port/linux/game/ae_vehicle_balance_rules.h`: a warthog takes 2 rockets,
  2 frag grenades, 12 sniper hits or 125 pistol rounds, a ghost, banshee or
  gun turret 1 rocket or 8 sniper hits, a scorpion 2-3 rockets or 3 tank
  shells; fire and melee none) then applies in full, it cannot die while someone rides in it (it holds until its last rider gets
  out, which gives it back a quarter of its health, or dies in it, which
  destroys it), and the wreck burns 10 seconds before it is cleared; a
  destroyed vehicle respawns by the gametype's time, 30 seconds where that is
  never. NHE's set and the stock gametypes keep Halo CE's vehicles, which
  never die. With the vehicle damage pack (`packs/ae_vehicle_damage.aepack`,
  CMT's TSC:E effects) a damaged warthog, ghost or banshee smokes and burns,
  dies in TSC:E's explosion with its hood or wings coming off, and the
  warthog and the banshee leave a burning wreck (the ghost its burning body);
  each machine shows what its own pack gives, the game is the same.
  Since seed revision 28 they also play JUMP 120% (a map layout's own jump line still wins), and all but the OCTAGON
  drills, FFA AE TRAINING and the practice modes (FFA AE PRACTICE and its battle rifle version) play FALL DAMAGE HALO
  2 (a landing hurts from 6 world units, fully from 10, and a fall past 14 kills; they had no fall damage before). In
  the 4v4 and 2v2 AE CTF and AE COMP CTF gametypes, and their battle rifle versions, capture the flag plays the map's
  own vehicles (the stock capture the flag's warthogs only, and none in AE COMP's; AE COMP's now also have VEHICLES
  DESTRUCTIBLE). Halo 1: NHE's set and the stock gametypes keep Halo CE's fall damage and jump.
- **Halo 1: NHE's own 23 gametypes** (TS 50, TS 100, TS ON-OFF, TS TRAINING,
  TS PRACTICE, TS SNIPERS, FFA 50 NR/R, 1 V 1 NR/R, KOTH 5M 7S/10S, BALL 5M
  7S/10S, eight CTFs and CTF WIZARD), with NHE's values: Halo CE health, fall
  damage, NO SPREAD FIRST SHOT, the countdown, NHE EXTRAS (everyone joins red, no team
  swap, the dead camera on you, the match clock counting up), no death bonus or
  kill penalty in slayer. On Halo 1: NHE's maps each plays its NHE mode; on
  other maps no vehicles.
  With them, TEAM NHE VANILLA and TEAM NHE POWERUPS: team slayer on Arena
  Evolved's casual values that plays Halo 1: NHE's VANILLA and NHE & POWERUPS
  modes on NHE's maps. NHE MODE and NHE EXTRAS are on a gametype's **NHE
  OPTIONS** screen.
- The lists show display names (TEAM AE SLAYER over the stored AE TEAM SLY),
  sorted by playlist folder (seed revision 22), each battle rifle version right after the gametype it
  copies, then your own gametypes, before the built-in ones (STOCK):

  | Folder | Gametypes |
  |---|---|
  | AE FFA | slayer, king, oddball, COMP slayer, snipers, rockets (all free for all) |
  | AE 2V2 | the 2V2 slayer, CTF, king and oddball, then the COMP 2V2 set |
  | AE 4V4 | team slayer, CTF, king, oddball, then the team COMP set |
  | AE BTB | BTB slayer, CTF, king, oddball, heavies |
  | AE ACTION SACK | phantoms, juggernaut, reverse tag, accumulation, stalker, crazy king, elimination, endurance, race, rally, SWAT, team snipers, shotty snipers, team rockets, shotguns, heavies |
  | AE PRACTICE | practice, BR practice, training, the OCTAGON drills |
  | CLASSIC COMP | Halo 1: NHE's, as before |

  AE's gametypes are built in since 0.7.1: no file in your save. PISTOL, BR or EVO is each mode's START switch, kept per
  mode on each PC. OPTIONS on a built-in one saves your own copy.

