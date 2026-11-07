# New weapons in 0.7.6

> Not in 0.7.6: comes in 0.7.7. (The weapons below are held; the 0.7.6 release does not carry them.)

Arena Evolved 0.7.6 adds nine weapons, each as a weapon pack in the `packs` folder of the downloads, as the battle
rifle and the 0.7.5 weapons came. Each is made from community-released tags (CMT's or ZTeam's, credited below and in
the pack's own CREDITS.txt) and tuned for Halo CE's sandbox: a player with full shields has 75 shield and 75 health.
How packs are built is in [weapon-packs.md](weapon-packs.md); the new grenades are in [grenades.md](grenades.md).

**How a game gets them.** The host's gametype names the packs it plays (GAME OPTIONS > WEAPONS > WEAPON PACKS: the
PACKS on the map, and START WEAPONS for PRIMARY and SECONDARY). Where a pack cannot be added (packs turned off on a
machine in Settings > Game Options > WEAPON PACKS, or a map it does not fit) its slot plays the stock weapon named in
the table below. A game that names any of these packs takes only Arena Evolved 0.7.6 or later with the pack: older
builds and other clients are turned away.

| Weapon | Role | Slot | Without the pack | From |
|---|---|---|---|---|
| TACTICAL SMG | suppressed automatic with a 2x red-dot zoom | 15 | assault rifle | CMT |
| BURST PDW | three-round burst submachine gun | 16 | assault rifle | ZTeam |
| HEAVY REVOLVER | slow, heavy-hitting sidearm | 17 | pistol | ZTeam |
| FOCUS BEAM | heat beam that strips shields | 18 | assault rifle | CMT |
| SPIKER | close-to-mid automatic | 19 | assault rifle | CMT |
| RAVAGER | lobbed bolts in bursts, a charged shot that burns | 20 | assault rifle | CMT |
| AUTO SHOTGUN | semi-automatic shotgun | 21 | shotgun | ZTeam |
| GRENADE LAUNCHER | bouncing grenades you set off by letting go | 22 | rocket launcher | ZTeam |
| CONCUSSION RIFLE | explosive bolts that shove players and vehicles | 23 | rocket launcher | ZTeam |

**About slots.** Today a gametype names a weapon by its pack's slot number, and the stored slots end at 24. A native
weapon cache is planned to replace slots: every map gets the whole cache, and a gametype names its weapons by a fixed
id instead of a slot number. The slot numbers on this page belong to the pack system and will not carry over to the
cache.

**Credits.** CMT is the Custom Mapping Team; their weapons here come from their stock-compatible source releases of
The Silent Cartographer: Evolved (the Evolved team: Teh Lag, Ifafudafi, Siliconmaster and the contributors their
credits name), used under the release's own terms. ZTeam's weapons come from the ZTeam tag pack, released on
halomaps.org; it names no individual authors. Every pack's numbers are tuned by Arena Evolved.

## TACTICAL SMG

A suppressed SMG with a red-dot optic and one 2x zoom; its round carries farther than the SMG's.

- 13 rounds a second, 48 a magazine, 144 to start and in reserve.
- 12 damage a round up close, 6.5 once the round has slowed.
- Spread 1 to 4 degrees; one zoom level, 2x, with CMT's own scope picture.
- These numbers are a first pass, to be tuned in play.

Credits: CMT (the model, textures, animations, effects, sounds and zoom lines). Slot 15, held as the SMG is.

## BURST PDW

A compact submachine gun that fires three-round bursts: quick to kill up close, a little slower than the battle rifle
at mid range.

- A pull fires a 3-round burst at once (the engine's burst, as the battle rifle's: no delay on the pull); pulls come
  0.53 s apart at the quickest.
- 20 damage a round up close, 14 once the round has slowed.
- Kills from full shields in 3 bursts up close (1.10 s) and 4 at mid range (1.63 s; the battle rifle takes 1.53 s).
- 36 a magazine (12 bursts), 144 to start and in reserve; spread 0.4 to 1.2 degrees over a burst; no zoom.

Credits: ZTeam (the model in its dark texture set, animations, sounds and HUD). Slot 16.

## HEAVY REVOLVER

A slow, heavy sidearm: few shots, each one hurts.

- One slug a shot, 46.4 damage on a player: 2 strip a full shield, the third to the head kills, 4 to the body kill.
  Without shields, one to the head kills.
- A slug and a melee blow kill from full; a blow alone does not.
- 1.67 slugs a second, 8 a drum, 24 to start, 40 at most.
- The slug is slow and drops: lead your target at range.

Credits: ZTeam (the model, animations, sounds and HUD; the dark red and metal finish is made from ZTeam's own
textures). Slot 17, held as the pistol is.

## FOCUS BEAM

A heat beam that strips shields fast but is weak on bare health: pair it with a pistol, a rifle burst or a melee blow.

- Strips a full shield in about 0.67 s.
- About 2 s of beam overheats it, which leaves a player with no shields at about 41 of 75 health; then it is locked
  about 1.6 s while it cools.
- A battery of 333 rounds, 20 rising to 30 a second; a range of 30 world units; one 2x zoom.
- A narrow beam, so a player under it can still see.

Credits: CMT (their prototype focus beam: model, animations, HUD, effects and sounds). Slot 18.

## SPIKER

A close-to-mid automatic that fires arcing spikes, harder on health than on shields.

- 10 spikes a second, 40 a magazine, 80 in reserve to start, 160 at most.
- 9 spikes strip a full shield, 15 kill from full (1.4 s).
- The spikes fly at 50 world units a second, slowing to 30, and arc; they carry 60 units.

Credits: CMT (their multiplayer spiker: model, animations, HUD, effects and sounds). Slot 19.

## RAVAGER

A launcher of lobbed bolts that burst with a small splash, with a charged shot that leaves a burning pool.

- A pull fires 3 bolts one after another; 4 bolts strip a full shield and 7 kill, so the third pull kills.
- Hold the trigger 0.75 s to charge, then let go for one big bolt: a larger blast, then a burning pool that hurts
  for about 2.25 s where it burst.
- 7 pulls back to back overheat it, then a 3 s vent; a battery of 36 bolts; one 1.4x zoom.
- Its own look: crimson panels, dark steel trim, a heat-tempered blade and cyan glowing seams.

Credits: CMT (their prototype ravager, built on their spiker's model and animations, with the charge and burn effects
of another CMT prototype). Slot 20.

## AUTO SHOTGUN

A semi-automatic shotgun: no pump between shots.

- 12 pellets a shot. Up close, 2 shots kill from full and 1 never does; at range, 3 strip and 5 kill.
- 1.75 shots a second, 6 shells loaded one at a time (0.5 s for the first, 0.43 s for each next: 2.67 s for all 6),
  12 to start and 24 in reserve.
- The draw racks it once and a finished reload works the action once. A shot never does.

Credits: ZTeam (the model, animations, sounds and HUD). Slot 21, held as the shotgun is.

## GRENADE LAUNCHER

A launcher of bouncing grenades that you set off yourself.

- Hold the trigger to keep a grenade from bursting (up to 10 s), let go to burst it where it is. A quick tap leaves
  the grenade to burst 0.8 s after it comes to rest, or at once if it hits a player.
- 85 damage at full within 0.6 world units, none past 1.6: one strips a full shield, two close ones kill.
- 6 a magazine, 12 to start, 24 at most; 1.1 shots a second.
- No picture of this weapon exists that we may use, so its name is written in text in the prompts, the kill feed and
  its HUD.

Credits: ZTeam (the models, the grenade, animations, effects, sounds and HUD). Slot 22.

## CONCUSSION RIFLE

A semi-automatic launcher of explosive bolts that shove what they hit.

- 6 bolts a magazine at 2 a second, 18 to start, 24 in reserve.
- Each bolt bursts for 30 at the centre, falling to 10 at its 1.5-unit edge: 3 direct hits strip a full shield, 5
  kill.
- The blast pushes players and vehicles: one bolt rears a Warthog up, a second flips it.
- The bolt is slow and drops.
- Its name is written in text where a picture of it would be, as the grenade launcher's.

Credits: ZTeam (the models, the bolt, its effects, animations, sounds and HUD). Slot 23.
