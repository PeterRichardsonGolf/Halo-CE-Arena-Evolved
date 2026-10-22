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
  STALKER have none. FFA AE JUGGERNAUT plays Halo 2's rules: to 15, the
  juggernaut with extra damage at normal speed, only the juggernaut's kills
  score, his killer takes the role, and a random player gets it when he kills
  himself or there is none.
- **Arena Evolved special modes** (team slayer to 50): SNIPERS (sniper rifle and
  pistol, the sniping weapon set, no motion sensor), SHOTTY SNIPERS (shotgun
  and sniper rifle, no weapons on the map), SWAT (pistols, no shields, classic
  health, no grenades), ROCKETS, SHOTGUNS and HEAVIES (75 kills, the map's
  vehicles).
- **Motion sensor, enemies only** (0.3.0): the team gametypes above that have the
  motion sensor (and their AE BR twins) show only enemies on it, never your
  teammates (OTHER PLAYERS ON RADAR: ENEMIES, in Indicator Options); the free
  for all ones show every other player, as ENEMIES does there.
- **Arena Evolved BR** (0.3.0): the casual set, the free for all variants, FFA AE PRACTICE and AE COMP again with the
  **battle rifle in hand** and an assault rifle second (FFA AE BR SLAYER, TEAM AE BR SLAYER, TEAM AE BR COMP CTF, ... 25
  gametypes; stored as AE BR FFA, AE BR TS, AE BR C CTF, ...). Every other value is its twin's. Not made for the
  single-weapon modes (snipers, rockets, shotguns, SWAT, heavies, shotty snipers), RACE, RALLY or TRAINING. They
  need the battle rifle's pack (the `packs` folder): only machines with it join; on Custom Edition maps, and where
  the pack is not, the slot plays an assault rifle.
- **Arena Evolved COMP**: FFA AE COMP SLAYER (25 kills), TEAM AE
  COMP SLAYER (50), TEAM AE COMP CTF (3 captures), TEAM AE COMP KING and TEAM
  AE COMP ODDBALL (5 minutes held, with the lethal ball). 5-second respawn and suicide penalty, 2
  frag grenades, no motion sensor, no vehicles, item TIMERS only in line of
  sight (no list, no item calls), the objective only in line of sight, both
  weapons dropped; the slayers have no time limit, the others 15 minutes.
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
- Every Arena Evolved gametype has no fall damage, HALO 2 health (SWAT:
  Halo CE), NO SPREAD FIRST SHOT, the PRE-GAME COUNTDOWN and generic starting
  equipment, with a pistol in hand and an assault rifle unless its mode says
  otherwise.
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
  sorted by playlist folder, each battle rifle version right after the gametype it
  copies, then your own gametypes, before the built-in ones (STOCK):

  | Folder | Gametypes |
  |---|---|
  | AE FFA | slayer, king, oddball, COMP slayer, snipers, rockets (all free for all) |
  | AE 2V2 | the 2V2 slayer, CTF, king and oddball |
  | AE 4V4 | team slayer, CTF, king, oddball, then the team COMP set |
  | AE ACTION SACK | phantoms, juggernaut, reverse tag, accumulation, stalker, crazy king, elimination, endurance, race, rally, SWAT, team snipers, shotty snipers, team rockets, shotguns, heavies |
  | AE PRACTICE | practice, BR practice, training, the OCTAGON drills |
  | NHE | Halo 1: NHE's, as before |
 Seeded gametypes you edit are kept as
  you made them when a newer build updates the others.

