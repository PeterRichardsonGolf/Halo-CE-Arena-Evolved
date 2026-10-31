#!/usr/bin/env python3
r"""seed_rules: every seeded gametype's logged rules against the expected values (typed in here from the gametypes
plan's spec and Names tables, the NHE 1.0 Gametypes notes and the special modes' table; never read from the game).

    # each seed hosted once on stock bloodgulch (on the box), its rules in the run's debug.txt:
    python3 tools/ae_test/seed_rules.py --names | while read -r n; do
        hook=""; case "$n" in TS\ *|KOTH*|BALL\ 5M*|CTF\ *) hook="--env HALO_NETWORK_TEST_AUTO_BALANCE=1";; esac
        python3 tools/ae_test/run.py --build <rev> --map bloodgulch --saved-gametype "$n" --save-from <fresh root> \
            --bots 2 --start 6 --exit-after 28 $hook --out-name "seed-$(echo "$n" | tr ' ' '_')"
    done
    python3 tools/ae_test/seed_rules.py ~/halo-test/ae_test/out/run --prefix seed-

(The NHE team gametypes need the auto balance test hook: NHE EXTRAS puts everyone on red, and a team game
does not start with one team. The data root's packs/ needs the EVO rifle's pack: without it, its seeds start with the
assault rifle.) It compares the "game rules", "AE rules" and "player rules" lines, "vehicles
placed" and the first spawn's weapons and frag grenades. Exit 0 when every seed passes.
"""
import argparse
import glob
import re
import sys

T = lambda s: f"{s} ticks ({s / 30:g} s)"

NHE_COMMON = {
    "health": "classic", "fall damage": "on", "starting equipment": "generic", "vehicle set": "1",
    "time limit": "none", "timers": "off", "no spread": "first shot", "pre-game countdown": "on",
    "friendly fire": "on", "shields": "on", "loadout": "category", "no map weapons": "off",
    "vehicle sets": "none (red none, blue none)",
    "timers level": "off", "spawn heat": "on", "objective": "normal", "nhe extras": "on", "drop secondary": "ce",
    "ball melee": "stock", "weapon pack": "none", "pack loadout primary": "stock", "pack loadout secondary": "stock",
    "battle rifle on map": "off", "infinite ammo": "off", "bottomless clip": "off", "map grenades": "on",
    "lives": "0", "health%": "100%", "respawn growth": "0 ticks", "odd man out": "off", "friend indicators": "on",
    "auto team balance": "off", "friendly fire penalty": "0 s", "vehicles placed": "0",
}
SLAYER = {"death bonus": "off", "kill penalty": "off", "kill in order": "off"}
CTF = {"flag at home to score": "on", "assault": "off", "single flag": "off"}
BALL = {"ball": "normal", "balls": "1", "speed with ball": "slow", "with ball": "none", "without ball": "none",
        "random start": "off", "juggernaut rules": "halo ce"}
# name: engine, score, respawn, suicide, radar players, goal radar, weapons, nhe mode, extras
NHE_ROWS = {
    "TS 50": ("slayer (teams)", 50, 150, 150, "none", "none", "normal", "nhe & timer", SLAYER),
    "TS 100": ("slayer (teams)", 100, 150, 300, "none", "none", "normal", "nhe & timer", SLAYER),
    "TS ON-OFF": ("slayer (teams)", 50, 150, 150, "none", "none", "normal", "timer only", SLAYER),
    "TS TRAINING": ("slayer (teams)", 50, 150, 150, "none", "none", "normal", "training", dict(SLAYER, training="on")),
    "TS PRACTICE": ("race (teams)", 1, 0, 0, "none", "none", "normal", "training",
                    {"infinite grenades": "on", "practice": "on", "training": "on", "race": "normal",
                     "team scoring": "minimum"}),
    "TS SNIPERS": ("slayer (teams)", 50, 150, 150, "none", "none", "sniping", "nhe & timer", dict(SLAYER, invisible="on")),
    "FFA 50 NR": ("slayer", 50, 150, 150, "none", "none", "normal", "nhe & timer", SLAYER),
    "FFA 50 R": ("slayer", 50, 150, 150, "all", "motion tracker", "normal", "nhe & timer", SLAYER),
    "1 V 1 NR": ("slayer", 15, 150, 150, "none", "none", "normal", "nhe & timer", SLAYER),
    "1 V 1 R": ("slayer", 15, 150, 150, "all", "motion tracker", "normal", "nhe & timer", SLAYER),
    "KOTH 5M 7S": ("king (teams)", 5, 225, 300, "none", "nav points", "normal", "nhe & timer", {"moving hill": "on"}),
    "KOTH 5M 10S": ("king (teams)", 5, 300, 300, "none", "nav points", "normal", "nhe & timer", {"moving hill": "on"}),
    "BALL 5M 7S": ("oddball (teams)", 5, 225, 300, "none", "nav points", "normal", "nhe & timer", BALL),
    "BALL 5M 10S": ("oddball (teams)", 5, 300, 300, "none", "nav points", "normal", "nhe & timer", BALL),
}
for n, s, r, rad in [("CTF 3C 7S", 3, 225, "none"), ("CTF 3C 7S R", 3, 225, "all"), ("CTF 3C 10S", 3, 300, "none"),
                     ("CTF 3 10S R", 3, 300, "all"), ("CTF 5C 7S", 5, 225, "none"), ("CTF 5C 7S R", 5, 225, "all"),
                     ("CTF 5C 10S", 5, 300, "none"), ("CTF 5 10S R", 5, 300, "all")]:
    NHE_ROWS[n] = ("ctf (teams)", s, r, 300, rad, "nav points", "normal", "nhe & timer", CTF)
NHE_ROWS["CTF WIZARD"] = ("ctf (teams)", 5, 300, 300, "all", "motion tracker", "pistols", "nhe & timer", CTF)



def fields(text):
    out = {}
    m = re.search(r"game rules: (.*)", text)
    if not m:
        return out
    first, _, second = m[1].partition("; ")
    for part in first.split(", "):
        k, _, v = part.rpartition(" ")
        if k == "pre-game": k, v = "pre-game countdown", v
        out[k] = v
    # (the multi-word keys: parse by known names)
    for key in ["health", "fall damage", "starting equipment", "vehicle set", "time limit", "timers", "training",
                "no spread", "pre-game countdown", "practice"]:
        mm = re.search(rf"(?:^|, ){re.escape(key)} ([^,]*)", first)
        if mm: out[key] = mm[1]
    eng = re.match(r"([a-z]+(?: \(teams\))?), ", second)
    out["engine"] = eng[1] if eng else "?"
    for key in ["score to win", "respawn", "suicide penalty", "friendly fire", "radar players", "goal radar", "shields",
                "invisible", "infinite grenades", "weapon set", "loadout", "no map weapons", "vehicle sets", "death bonus",
                "kill penalty", "kill in order", "flag at home to score", "assault", "single flag", "moving hill", "ball",
                "balls", "speed with ball", "with ball", "without ball", "random start", "juggernaut rules", "race",
                "team scoring"]:
        mm = re.search(rf", {re.escape(key)} ((?:[^,(]|\([^)]*\))*)", second)
        if mm: out[key] = mm[1].strip()
    m = re.search(r"AE rules: (.*)", text)
    for key in ["timers level", "spawn heat", "objective", "nhe extras", "drop secondary", "nhe mode", "ball melee"]:
        mm = re.search(rf"(?:^|, ){re.escape(key)} ([^,]*)", m[1] if m else "")
        if mm: out[key] = mm[1].strip()
    # (the weapon pack's fields, written with colons: "weapon pack: none, pack loadout: primary stock, secondary
    # stock, battle rifle on map: off")
    for key, pattern in [("weapon pack", r"weapon pack: ([^,(]*(?:\([^)]*\))?)"), ("pack loadout primary", r"pack loadout: primary ([^,]*)"),
                         ("pack loadout secondary", r"pack loadout: primary [^,]*, secondary ([^,]*)"),
                         ("battle rifle on map", r"battle rifle on map: ([^,]*)")]:
        mm = re.search(pattern, m[1] if m else "")
        if mm: out[key] = mm[1].strip()
    # (revision 16's ammo rules, a line of their own: "AE ammo: infinite ammo on, bottomless clip on")
    m = re.search(r"AE ammo: (.*)", text)
    for key in ["infinite ammo", "bottomless clip"]:
        mm = re.search(rf"(?:^|, ){re.escape(key)} ([^,]*)", m[1] if m else "")
        if mm: out[key] = mm[1].strip()
    # (revision 18's MAP GRENADES, a line of its own: "AE map grenades: off")
    mm = re.search(r"AE map grenades: (\w+)", text)
    if mm: out["map grenades"] = mm[1]
    # (revision 19's SENSOR RANGE, a line of its own as the HUD loads: "AE motion sensor range: 10 m (...)")
    mm = re.search(r"AE motion sensor range: (stock|\d+ m)", text)
    if mm: out["sensor range"] = mm[1]
    # (revision 21's CONTROLLER CROUCH, a line of its own: "AE controller crouch: stays down")
    mm = re.search(r"AE controller crouch: (stays down|halo ce)", text)
    if mm: out["controller crouch"] = mm[1]
    # (revision 22's MELEE UNZOOMS, a line of its own: "AE melee unzooms: on")
    mm = re.search(r"AE melee unzooms: (on|off)", text)
    if mm: out["melee unzooms"] = mm[1]
    # (and its MOUSE AIM ASSIST: "AE mouse aim assist: off")
    mm = re.search(r"AE mouse aim assist: (off|allowed)\s", text)
    if mm: out["mouse aim assist"] = mm[1]
    # (revision 23's VEHICLES DESTRUCTIBLE: "AE vehicles destructible: on")
    mm = re.search(r"AE vehicles destructible: (on|off)", text)
    if mm: out["vehicles destructible"] = mm[1]
    # (revision 27's SPAWNS: "AE spawns: modern")
    mm = re.search(r"AE spawns: (classic|modern)", text)
    if mm: out["spawns"] = mm[1]
    m = re.search(r"player rules: (.*)", text)
    for key, name in [("lives", "lives"), ("health%", "health"), ("respawn growth", "respawn growth"),
                      ("odd man out", "odd man out"), ("friend indicators", "friend indicators"),
                      ("auto team balance", "auto team balance"), ("friendly fire penalty", "friendly fire penalty")]:
        mm = re.search(rf"(?:^|, ){re.escape(name)} ([^,]*)", m[1] if m else "")
        if mm: out[key] = mm[1].strip()
    m = re.search(r"vehicles placed: (\d+)", text)
    if m: out["vehicles placed"] = m[1]
    return out



AE = {"health": "halo 2", "fall damage": "off", "starting equipment": "generic", "no spread": "first shot",
      "pre-game countdown": "on", "friendly fire": "on", "shields": "on", "invisible": "off",
      "infinite grenades": "off", "no map weapons": "off", "nhe extras": "off", "drop secondary": "always",
      "lives": "0", "odd man out": "off", "friendly fire penalty": "0 s", "auto team balance": "off",
      "spawn heat": "on", "nhe mode": "by vehicles", "ball melee": "stock", "weapon pack": "none",
      "pack loadout primary": "stock", "pack loadout secondary": "stock", "battle rifle on map": "off",
      "infinite ammo": "off", "bottomless clip": "off", "map grenades": "on"}
CASUAL = dict(AE, **{"timers": "hud + waypoints", "timers level": "hud + waypoints", "training": "off",
                     "practice": "off", "time limit": "15 min", "radar players": "all", "objective": "normal",
                     "loadout": "pistol + assault rifle", "weapon set": "normal", "starting frags": "0"})
COMP = dict(AE, **{"timers": "line of sight", "timers level": "line of sight", "training": "off", "practice": "off",
                   "respawn": T(150), "suicide penalty": T(150), "radar players": "none", "vehicle set": "1",
                   "loadout": "pistol + assault rifle", "weapon set": "normal", "starting frags": "2"})
def TSX(**k):
    d = dict(CASUAL, engine="slayer (teams)", **{"respawn": T(300), "suicide penalty": T(300), "vehicle set": "2"})
    d.update(k)
    return d
ROWS = {  # name: (fields, first-spawn weapons, frags at first spawn)
    "AE FFA SLAY": (dict(CASUAL, engine="slayer", **{"score to win": "25", "respawn": T(0), "suicide penalty": T(300), "vehicle set": "2", "goal radar": "motion tracker"}), "pistol + assault rifle", 4),
    "AE TEAM SLY": (dict(CASUAL, engine="slayer (teams)", **{"score to win": "50", "respawn": T(300), "suicide penalty": T(300), "vehicle set": "2"}), "pistol + assault rifle", 4),
    "AE CTF": (dict(CASUAL, engine="ctf (teams)", **{"score to win": "3", "respawn": T(300), "suicide penalty": T(150), "vehicle set": "2", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE KING": (dict(CASUAL, engine="king (teams)", **{"score to win": "5", "respawn": T(300), "suicide penalty": T(150), "vehicle set": "2", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE FFA BALL": (dict(CASUAL, engine="oddball", **{"ball melee": "lethal", "score to win": "5", "respawn": T(150), "suicide penalty": T(150), "vehicle set": "1", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE 2V2 SLY": (dict(CASUAL, engine="slayer (teams)", **{"score to win": "25", "respawn": T(300), "suicide penalty": T(300), "vehicle set": "2"}), "pistol + assault rifle", 4),
    "AE 2V2 CTF": (dict(CASUAL, engine="ctf (teams)", **{"score to win": "3", "respawn": T(300), "suicide penalty": T(150), "vehicle set": "2", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE 2V2 KING": (dict(CASUAL, engine="king (teams)", **{"score to win": "5", "respawn": T(300), "suicide penalty": T(150), "vehicle set": "2", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE 2V2 BALL": (dict(CASUAL, engine="oddball (teams)", **{"ball melee": "lethal", "score to win": "5", "respawn": T(300), "suicide penalty": T(150), "vehicle set": "1", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE TEAM OB": (dict(CASUAL, engine="oddball (teams)", **{"ball melee": "lethal", "score to win": "5", "respawn": T(300), "suicide penalty": T(150), "vehicle set": "1", "goal radar": "nav points"}), "pistol + assault rifle", 4),
    "AE PRACTICE": (dict(CASUAL, engine="slayer", **{"score to win": "500", "time limit": "none", "practice": "on", "respawn": T(0), "suicide penalty": T(300), "vehicle set": "0"}), "pistol + assault rifle", 4),
    "AE SNIPERS": (TSX(**{"score to win": "50", "radar players": "none", "weapon set": "sniping", "loadout": "sniper rifle + pistol"}), "sniper rifle + pistol", 4),
    "AE SHOTSNIP": (TSX(**{"score to win": "50", "radar players": "none", "no map weapons": "on", "loadout": "shotgun + sniper rifle"}), "shotgun + sniper rifle", 4),
    "AE SWAT": (TSX(**{"score to win": "50", "radar players": "none", "weapon set": "pistols", "health": "classic", "shields": "off", "loadout": "pistol + none", "starting frags": "none", "map grenades": "off"}), "pistol", 0),
    "AE ROCKETS": (TSX(**{"score to win": "50", "weapon set": "rocket launchers", "loadout": "rocket launcher + none"}), "rocket launcher", 4),
    "AE SHOTGUNS": (TSX(**{"score to win": "50", "weapon set": "shotguns", "loadout": "shotgun + pistol"}), "shotgun + pistol", 4),
    "AE HEAVIES": (TSX(**{"score to win": "75", "weapon set": "heavy", "vehicle set": "0", "loadout": "rocket launcher + assault rifle"}), "rocket launcher + assault rifle", 4),
    "AE TRAINING": (dict(AE, engine="slayer", **{"timers": "hud", "timers level": "hud", "training": "on", "practice": "off", "time limit": "none", "score to win": "500", "respawn": T(0), "suicide penalty": T(300), "vehicle set": "0", "radar players": "all", "loadout": "pistol + assault rifle", "weapon set": "normal", "starting frags": "0"}), "pistol + assault rifle", 4),
    "AE COMP FFA": (dict(COMP, engine="slayer", **{"score to win": "25", "time limit": "none", "objective": "normal"}), "pistol + assault rifle", 2),
    "AE COMP TS": (dict(COMP, engine="slayer (teams)", **{"score to win": "50", "time limit": "none", "objective": "normal"}), "pistol + assault rifle", 2),
    "AE COMP CTF": (dict(COMP, engine="ctf (teams)", **{"score to win": "3", "time limit": "15 min", "goal radar": "nav points", "objective": "line of sight"}), "pistol + assault rifle", 2),
    "AE COMP KOH": (dict(COMP, engine="king (teams)", **{"score to win": "5", "time limit": "15 min", "goal radar": "nav points", "objective": "line of sight"}), "pistol + assault rifle", 2),
    "AE COMP OB": (dict(COMP, engine="oddball (teams)", **{"ball melee": "lethal", "score to win": "5", "time limit": "15 min", "goal radar": "nav points", "objective": "line of sight"}), "pistol + assault rifle", 2),
}
# the NHE set (Task 11's table, NHE_ROWS above), the auto balance test hook on in team ones (they start that way)
for name, (engine, score, resp, suic, radar, goal, weapons, mode, extra) in NHE_ROWS.items():
    want = dict(NHE_COMMON, engine=engine, **{"score to win": str(score), "respawn": T(resp), "suicide penalty": T(suic),
                "radar players": radar, "goal radar": goal, "weapon set": weapons, "nhe mode": mode, "starting frags": "0"})
    want.setdefault("invisible", "off"); want.setdefault("infinite grenades", "off"); want.setdefault("training", "off"); want.setdefault("practice", "off")
    want.update(extra)
    if "teams" in engine:
        want["auto team balance"] = "on"
    ROWS[name] = (want, None, None)
# revision 7: BALL MELEE LETHAL on AE FFA BALL, AE TEAM OB, AE 2V2 BALL and AE COMP OB (the other seeds STOCK)
# revision 9: AE VANILLA / AE POWERUPS renamed to the NHE set's stored names; their values stay revision 6's and
# COMBAT FEEDBACK stays HALO CE
for n, mode in (("NHE TS VAN", "vanilla"), ("NHE TS POW", "nhe & powerups")):
    ROWS[n] = (dict(CASUAL, engine="slayer (teams)", **{"score to win": "50", "respawn": T(300), "suicide penalty": T(300), "vehicle set": "2", "nhe mode": mode}), "pistol + assault rifle", 4)

# revision 8: the 13 free for all gametypes made from the stock ones (casual set's rules; each keeps its stock mode's
# score, lives, ball or hill, weapon set, vehicles and motion sensor). Expected values are the stock builders' own
# (game_engine.c build_game_variant_*), typed in here
def FFAX(engine, score, resp, suicide, vehicles, radar, goal, spawn="pistol + assault rifle", **k):
    d = dict(CASUAL, engine=engine, **{"score to win": str(score), "respawn": T(resp), "suicide penalty": T(suicide),
             "vehicle set": str(vehicles), "radar players": radar, "goal radar": goal})
    d.update(k)
    return (d, spawn, 4)
_SL = {"death bonus": "on", "kill penalty": "on", "kill in order": "off"}
_BALL = {"balls": "1", "speed with ball": "slow", "with ball": "none", "without ball": "none", "random start": "off",
         "juggernaut rules": "halo ce"}
ROWS["AE ELIM"] = FFAX("slayer", 25, 0, 300, 2, "all", "motion tracker", lives="1", **_SL)
ROWS["AE PHANTOMS"] = FFAX("slayer", 10, 150, 150, 2, "none", "nav points", invisible="on",
                           **{"death bonus": "off", "kill penalty": "off", "kill in order": "on"})
ROWS["AE ENDURE"] = FFAX("slayer", 10, 0, 300, 2, "all", "motion tracker", lives="5", **_SL,
                         **{"respawn growth": "300 ticks", "odd man out": "on"})
ROWS["AE FFA ROCK"] = FFAX("slayer", 25, 0, 300, 2, "none", "nav points", "rocket launcher",
                           **{"weapon set": "rocket launchers", "loadout": "rocket launcher + none"}, **_SL)
ROWS["AE FFA SNPR"] = FFAX("slayer", 15, 0, 300, 2, "none", "nav points", "sniper rifle + pistol",
                           **{"weapon set": "sniping", "loadout": "sniper rifle + pistol", "respawn growth": "150 ticks"}, **_SL)
# (revision 10: AE JUGGER: to 15, the juggernaut at normal speed, its scoring and succession)
ROWS["AE JUGGER"] = FFAX("oddball", 15, 150, 150, 2, "all", "nav points", **{"ball": "terminator",
                         **_BALL, "with ball": "extra damage", "speed with ball": "normal",
                         "juggernaut rules": "halo 2"})
ROWS["AE REV TAG"] = FFAX("oddball", 2, 150, 150, 1, "none", "nav points", **{"ball": "magic", **_BALL})
ROWS["AE ACCUM"] = FFAX("oddball", 5, 150, 150, 1, "none", "none", **{"ball": "magic", **_BALL,
                        "balls": "16"})
ROWS["AE STALKER"] = FFAX("oddball", 10, 150, 150, 2, "none", "motion tracker", **{
                          "ball": "terminator", **_BALL, "speed with ball": "faster", "with ball": "invisible",
                          "without ball": "damage resistant"})
ROWS["AE FFA KING"] = FFAX("king", 2, 150, 150, 2, "all", "nav points", **{"moving hill": "off"})
ROWS["AE CRAZYKNG"] = FFAX("king", 2, 0, 150, 2, "all", "nav points", **{"moving hill": "on"})
ROWS["AE RACE"] = FFAX("race", 3, 0, 300, 2, "all", "nav points", **{"race": "normal", "team scoring": "minimum"})
ROWS["AE RALLY"] = FFAX("race", 15, 0, 300, 2, "all", "nav points", **{"race": "rally", "team scoring": "minimum"})

# revision 12: PLAYERS ON RADAR ENEMIES on the AE team seeds whose motion sensor shows players (their AE BR twins below
# take it from them); the free for all ones keep ALL (ENEMIES plays as ALL there), the NHE set its own
RADAR_ENEMIES = ("AE TEAM SLY", "AE CTF", "AE KING", "AE 2V2 SLY", "AE 2V2 CTF", "AE 2V2 KING", "AE 2V2 BALL", "AE TEAM OB",
                 "AE ROCKETS", "AE SHOTGUNS", "AE HEAVIES")
for _name in RADAR_ENEMIES:
    ROWS[_name][0]["radar players"] = "enemies"

# revision 11: the AE BR set, each one its AE twin with the battle rifle in hand (the weapon pack's rifle over the
# PRIMARY slot's assault rifle) and the pistol second; nothing else differs (arena_gametypes.c's self-check too)
BR_TWINS = {
    "AE BR FFA": "AE FFA SLAY", "AE BR ELIM": "AE ELIM", "AE BR PHANT": "AE PHANTOMS", "AE BR ENDUR": "AE ENDURE",
    "AE BR FBALL": "AE FFA BALL", "AE BR JUGG": "AE JUGGER", "AE BR RTAG": "AE REV TAG", "AE BR ACCUM": "AE ACCUM",
    "AE BR STALK": "AE STALKER", "AE BR FKING": "AE FFA KING", "AE BR CKING": "AE CRAZYKNG", "AE BR PRAC": "AE PRACTICE",
    "AE BR C FFA": "AE COMP FFA", "AE BR TS": "AE TEAM SLY", "AE BR CTF": "AE CTF", "AE BR KING": "AE KING",
    "AE BR TM OB": "AE TEAM OB", "AE BR 2V2 S": "AE 2V2 SLY", "AE BR 2V2 C": "AE 2V2 CTF", "AE BR 2V2 K": "AE 2V2 KING",
    "AE BR 2V2 B": "AE 2V2 BALL", "AE BR C TS": "AE COMP TS", "AE BR C CTF": "AE COMP CTF", "AE BR C KOH": "AE COMP KOH",
    "AE BR C OB": "AE COMP OB",
}
BR_START = {"loadout": "assault rifle + pistol", "weapon pack": "battle rifle (slot 1)", "pack loadout primary": "pack",
            "pack loadout secondary": "stock", "battle rifle on map": "off"}
for _name, _twin in BR_TWINS.items():
    _want, _spawn, _frags = ROWS[_twin]
    ROWS[_name] = (dict(_want, **BR_START), "battle_rifle + pistol", _frags)

# revision 14: no death bonus and no kill penalty in every slayer seed (everyone at normal speed; the NHE set's since
# revision 4, the Phantoms ones the stock builder's), kill in order as before
for _name, (_want, _, _) in ROWS.items():
    if _want["engine"].startswith("slayer"):
        _want.update({"death bonus": "off", "kill penalty": "off"})
        _want.setdefault("kill in order", "off")

# revision 15 (tools/ae_gametype_change.py): no respawn time growth in AE FFA SNPR: the stock Snipers' adds 5 s to a
# player's respawn per death, up to 25 s, earned back by kills; no other AE gametype has it, Team AE SNIPERS included
for _name in ("AE FFA SNPR",):
    ROWS[_name][0].update({"respawn growth": "0 ticks"})

# revision 16 (owner, 2026-10-20): the OCTAGON set's 1v1 drills, added seeds: AE FFA SLAY's row but instant respawns,
# no grenades at all, INFINITE AMMO and BOTTOMLESS CLIP, each its start (the BR ones the weapon pack's rifle over the
# PRIMARY slot's assault rifle, as the AE BR set's)
_OCTAGON = dict(ROWS["AE FFA SLAY"][0], **{"respawn": T(0), "starting frags": "none", "infinite ammo": "on",
                                           "bottomless clip": "on", "death bonus": "off", "kill penalty": "off"})
_OCTAGON_BR = {"weapon pack": "battle rifle (slot 1)", "pack loadout primary": "pack"}
ROWS["AE OCT BR"] = (dict(_OCTAGON, loadout="assault rifle + none", **_OCTAGON_BR), "battle_rifle", 0)
ROWS["AE OCT BRSN"] = (dict(_OCTAGON, loadout="assault rifle + sniper rifle", **_OCTAGON_BR),
                       "battle_rifle + sniper rifle", 0)
ROWS["AE OCT MAG"] = (dict(_OCTAGON, loadout="pistol + none"), "pistol", 0)
ROWS["AE OCT MGSN"] = (dict(_OCTAGON, loadout="pistol + sniper rifle"), "pistol + sniper rifle", 0)
# revision 17 (owner, 2026-10-20): their team versions, added seeds: AE TEAM SLY's row (50 kills, the motion sensor's
# enemies) with the same changes and starts
_TEAM_OCTAGON = dict(ROWS["AE TEAM SLY"][0], **{"respawn": T(0), "starting frags": "none", "infinite ammo": "on",
                                                "bottomless clip": "on", "death bonus": "off", "kill penalty": "off"})
ROWS["AE OCT TBR"] = (dict(_TEAM_OCTAGON, loadout="assault rifle + none", **_OCTAGON_BR), "battle_rifle", 0)
ROWS["AE OCT TBSN"] = (dict(_TEAM_OCTAGON, loadout="assault rifle + sniper rifle", **_OCTAGON_BR),
                       "battle_rifle + sniper rifle", 0)
ROWS["AE OCT TMAG"] = (dict(_TEAM_OCTAGON, loadout="pistol + none"), "pistol", 0)
ROWS["AE OCT TMSN"] = (dict(_TEAM_OCTAGON, loadout="pistol + sniper rifle"), "pistol + sniper rifle", 0)

# revision 19 (owner, 2026-10-20): SENSOR RANGE 10 m in every AE seed whose motion sensor is on (radar players all,
# friends or enemies), their AE BR twins and the OCTAGON drills with them; the sensor-off ones and the NHE set keep
# STOCK (the map's own range)
SENSOR_10M = ("AE FFA SLAY", "AE TEAM SLY", "AE CTF", "AE KING", "AE FFA BALL", "AE 2V2 SLY", "AE 2V2 CTF",
              "AE 2V2 KING", "AE 2V2 BALL", "AE TEAM OB", "AE PRACTICE", "AE ROCKETS", "AE SHOTGUNS", "AE HEAVIES",
              "AE TRAINING", "AE ELIM", "AE ENDURE", "AE JUGGER", "AE FFA KING", "AE CRAZYKNG", "AE RACE", "AE RALLY",
              "AE BR FFA", "AE BR ELIM", "AE BR ENDUR", "AE BR FBALL", "AE BR JUGG", "AE BR FKING", "AE BR CKING",
              "AE BR PRAC", "AE BR TS", "AE BR CTF", "AE BR KING", "AE BR TM OB", "AE BR 2V2 S", "AE BR 2V2 C",
              "AE BR 2V2 K", "AE BR 2V2 B", "AE OCT BR", "AE OCT BRSN", "AE OCT MAG", "AE OCT MGSN", "AE OCT TBR",
              "AE OCT TBSN", "AE OCT TMAG", "AE OCT TMSN")
for _name, (_want, _, _) in ROWS.items():
    _want["sensor range"] = "10 m" if _name in SENSOR_10M else "stock"
# revision 20 (owner, 2026-10-21): the OCTAGON drills are no-grenade games: MAP GRENADES off too
for _name in ("AE OCT BR", "AE OCT BRSN", "AE OCT MAG", "AE OCT MGSN", "AE OCT TBR", "AE OCT TBSN", "AE OCT TMAG",
              "AE OCT TMSN"):
    ROWS[_name][0]["map grenades"] = "off"
# CONTROLLER CROUCH (bit 8 of the variant's flags): HALO CE in every gametype before revision 21 (no seed had the bit)
for _name, (_want, _, _) in ROWS.items():
    _want["controller crouch"] = "halo ce"
# MELEE UNZOOMS (bit 9 of the variant's flags): OFF in every gametype before revision 22 (no seed had the bit)
for _name, (_want, _, _) in ROWS.items():
    _want["melee unzooms"] = "off"
# MOUSE AIM ASSIST (bit 10 of the variant's flags): ALLOWED in every gametype before revision 22 (no seed had the bit)
for _name, (_want, _, _) in ROWS.items():
    _want["mouse aim assist"] = "allowed"
# VEHICLES DESTRUCTIBLE (bit 11 of the variant's flags): OFF in every gametype before revision 23 (no seed had the bit)
for _name, (_want, _, _) in ROWS.items():
    _want["vehicles destructible"] = "off"

# revision 21 (tools/ae_gametype_change.py): CONTROLLER CROUCH STAYS DOWN in every AE gametype: a controller's held
# crouch keeps you down at full stick; the NHE set keeps HALO CE
for _name in ("AE FFA SLAY", "AE TEAM SLY", "AE CTF", "AE KING", "AE FFA BALL", "AE 2V2 SLY", "AE 2V2 CTF", "AE 2V2 KING", "AE 2V2 BALL", "AE TEAM OB", "AE PRACTICE", "AE SNIPERS", "AE SHOTSNIP", "AE SWAT", "AE ROCKETS", "AE SHOTGUNS", "AE HEAVIES", "AE ELIM", "AE PHANTOMS", "AE ENDURE", "AE FFA ROCK", "AE FFA SNPR", "AE JUGGER", "AE REV TAG", "AE ACCUM", "AE STALKER", "AE FFA KING", "AE CRAZYKNG", "AE RACE", "AE RALLY", "AE TRAINING", "AE COMP FFA", "AE COMP TS", "AE COMP CTF", "AE COMP KOH", "AE COMP OB", "AE BR FFA", "AE BR ELIM", "AE BR PHANT", "AE BR ENDUR", "AE BR FBALL", "AE BR JUGG", "AE BR RTAG", "AE BR ACCUM", "AE BR STALK", "AE BR FKING", "AE BR CKING", "AE BR PRAC", "AE BR C FFA", "AE BR TS", "AE BR CTF", "AE BR KING", "AE BR TM OB", "AE BR 2V2 S", "AE BR 2V2 C", "AE BR 2V2 K", "AE BR 2V2 B", "AE BR C TS", "AE BR C CTF", "AE BR C KOH", "AE BR C OB", "AE OCT BR", "AE OCT BRSN", "AE OCT MAG", "AE OCT MGSN", "AE OCT TBR", "AE OCT TBSN", "AE OCT TMAG", "AE OCT TMSN",):
    ROWS[_name][0].update({"controller crouch": "stays down"})

# revision 22 (owner, 2026-10-22): added seeds, no migration. The AE BTB set: each its AE casual team gametype's row
# (AE TEAM SLY, AE CTF, AE KING, AE TEAM OB) with 20 minutes, the map's own vehicles (vehicle set 0) and BTB's scores
# (slayer 100 kills); AE BTB HVY big team slayer with each team's vehicles CUSTOM (rocket warthogs and scorpions).
# The AE COMP 2V2 set: each its AE COMP team gametype's row, the slayer to 25. Each with its battle rifle version
_BTB = {"time limit": "20 min", "vehicle set": "0", "vehicle sets": "default (red default, blue default)"}
BTB_SEEDS = {"AE BTB SLY": ("AE TEAM SLY", {"score to win": "100"}), "AE BTB CTF": ("AE CTF", {}),
             "AE BTB KING": ("AE KING", {}), "AE BTB BALL": ("AE TEAM OB", {}),
             "AE BTB HVY": ("AE TEAM SLY", {"score to win": "100",
                                            "vehicle sets": "default (red custom, blue custom)"})}
COMP_2V2_SEEDS = {"AE C2V2 TS": ("AE COMP TS", {"score to win": "25"}), "AE C2V2 CTF": ("AE COMP CTF", {}),
                  "AE C2V2 KOH": ("AE COMP KOH", {}), "AE C2V2 OB": ("AE COMP OB", {})}
for _name, (_base, _changes) in BTB_SEEDS.items():
    _want, _spawn, _frags = ROWS[_base]
    ROWS[_name] = (dict(_want, **dict(_BTB, **_changes)), _spawn, _frags)
for _name, (_base, _changes) in COMP_2V2_SEEDS.items():
    _want, _spawn, _frags = ROWS[_base]
    ROWS[_name] = (dict(_want, **_changes), _spawn, _frags)
BR_TWINS_22 = {"AE BR BTB S": "AE BTB SLY", "AE BR BTB C": "AE BTB CTF", "AE BR BTB K": "AE BTB KING",
               "AE BR BTB B": "AE BTB BALL", "AE BR BTB H": "AE BTB HVY", "AE BR C2 TS": "AE C2V2 TS",
               "AE BR C2CTF": "AE C2V2 CTF", "AE BR C2KOH": "AE C2V2 KOH", "AE BR C2 OB": "AE C2V2 OB"}
for _name, _twin in BR_TWINS_22.items():
    _want, _spawn, _frags = ROWS[_twin]
    ROWS[_name] = (dict(_want, **BR_START), "battle_rifle + pistol", _frags)
BR_TWINS.update(BR_TWINS_22)

# revision 22 (tools/ae_gametype_change.py): feel rules as host gametype rules: a melee while zoomed takes the zoom
# off (MELEE UNZOOMS ON) and no aim assist for the mouse (MOUSE AIM ASSIST OFF) in every AE gametype; the NHE set and
# stock gametypes keep Halo CE's
for _name in ("AE FFA SLAY", "AE TEAM SLY", "AE CTF", "AE KING", "AE FFA BALL", "AE 2V2 SLY", "AE 2V2 CTF", "AE 2V2 KING", "AE 2V2 BALL", "AE TEAM OB", "AE PRACTICE", "AE SNIPERS", "AE SHOTSNIP", "AE SWAT", "AE ROCKETS", "AE SHOTGUNS", "AE HEAVIES", "AE BTB SLY", "AE BTB CTF", "AE BTB KING", "AE BTB BALL", "AE BTB HVY", "AE ELIM", "AE PHANTOMS", "AE ENDURE", "AE FFA ROCK", "AE FFA SNPR", "AE JUGGER", "AE REV TAG", "AE ACCUM", "AE STALKER", "AE FFA KING", "AE CRAZYKNG", "AE RACE", "AE RALLY", "AE TRAINING", "AE COMP FFA", "AE COMP TS", "AE COMP CTF", "AE COMP KOH", "AE COMP OB", "AE C2V2 TS", "AE C2V2 CTF", "AE C2V2 KOH", "AE C2V2 OB", "AE BR FFA", "AE BR ELIM", "AE BR PHANT", "AE BR ENDUR", "AE BR FBALL", "AE BR JUGG", "AE BR RTAG", "AE BR ACCUM", "AE BR STALK", "AE BR FKING", "AE BR CKING", "AE BR PRAC", "AE BR C FFA", "AE BR TS", "AE BR CTF", "AE BR KING", "AE BR TM OB", "AE BR 2V2 S", "AE BR 2V2 C", "AE BR 2V2 K", "AE BR 2V2 B", "AE BR C TS", "AE BR C CTF", "AE BR C KOH", "AE BR C OB", "AE BR BTB S", "AE BR BTB C", "AE BR BTB K", "AE BR BTB B", "AE BR BTB H", "AE BR C2 TS", "AE BR C2CTF", "AE BR C2KOH", "AE BR C2 OB", "AE OCT BR", "AE OCT BRSN", "AE OCT MAG", "AE OCT MGSN", "AE OCT TBR", "AE OCT TBSN", "AE OCT TMAG", "AE OCT TMSN",):
    ROWS[_name][0].update({"melee unzooms": "on", "mouse aim assist": "off"})

# revision 23 (tools/ae_gametype_change.py): VEHICLES DESTRUCTIBLE ON in every AE gametype with vehicles (vehicle set
# not NONE): explosives only, no death while occupied
for _name in ("AE FFA SLAY", "AE TEAM SLY", "AE CTF", "AE KING", "AE 2V2 SLY", "AE 2V2 CTF", "AE 2V2 KING", "AE PRACTICE", "AE SNIPERS", "AE SHOTSNIP", "AE SWAT", "AE ROCKETS", "AE SHOTGUNS", "AE HEAVIES", "AE BTB SLY", "AE BTB CTF", "AE BTB KING", "AE BTB BALL", "AE BTB HVY", "AE ELIM", "AE PHANTOMS", "AE ENDURE", "AE FFA ROCK", "AE FFA SNPR", "AE JUGGER", "AE STALKER", "AE FFA KING", "AE CRAZYKNG", "AE RACE", "AE RALLY", "AE TRAINING", "AE BR FFA", "AE BR ELIM", "AE BR PHANT", "AE BR ENDUR", "AE BR JUGG", "AE BR STALK", "AE BR FKING", "AE BR CKING", "AE BR PRAC", "AE BR TS", "AE BR CTF", "AE BR KING", "AE BR 2V2 S", "AE BR 2V2 C", "AE BR 2V2 K", "AE BR BTB S", "AE BR BTB C", "AE BR BTB K", "AE BR BTB B", "AE BR BTB H", "AE OCT BR", "AE OCT BRSN", "AE OCT MAG", "AE OCT MGSN", "AE OCT TBR", "AE OCT TBSN", "AE OCT TMAG", "AE OCT TMSN",):
    ROWS[_name][0].update({"vehicles destructible": "on"})

# revision 24 (owner, 2026-10-23): added seeds, no migration, in the builds with Bomb (game_engine_bomb.c; the game's
# log names its engine "terminator", Bungie's cut game type in that slot). AE BOMB: Bomb's rules (3 rounds, its
# 5 second respawns) on AE CTF's casual team set, no vehicles (Bomb's own: VEHICLES DESTRUCTIBLE stays off). AE
# GRIFBALL: Grifball's rules (melee only: no starting weapons, none on the map, no grenades; no shields, 3 second
# respawns, rounds to 3, no game time limit, the motion sensor at the map's range) with AE's rules, COMBAT FEEDBACK
# ARENA EVOLVED, CONTROLLER CROUCH STAYS DOWN, MELEE UNZOOMS ON and MOUSE AIM ASSIST OFF; no DROP SECONDARY ALWAYS or
# TIMERS (no weapons or power items to act on). Grifball's first spawn is the map's sword or the unseen fists: not
# checked
BOMB_ENGINE = "terminator (teams)"
_want, _spawn, _frags = ROWS["AE CTF"]
# (and revision 13's two frag grenades, which AE CTF's row above still lacks)
ROWS["AE BOMB"] = (dict(_want, **{"engine": BOMB_ENGINE, "respawn": T(150), "suicide penalty": T(150),
                                  "vehicle set": "1", "vehicles destructible": "off", "starting frags": "2"}),
                   _spawn, 2)
ROWS["AE GRIFBALL"] = (dict(AE, **{
    "engine": BOMB_ENGINE, "score to win": "3", "respawn": T(90), "suicide penalty": T(0), "time limit": "none",
    "timers": "off", "timers level": "off", "training": "off", "practice": "off", "objective": "normal",
    "radar players": "all", "goal radar": "nav points", "shields": "off", "weapon set": "no grenades",
    "loadout": "none + none", "no map weapons": "on", "drop secondary": "ce", "vehicle set": "1",
    "starting frags": "0", "sensor range": "stock", "controller crouch": "stays down", "melee unzooms": "on",
    "mouse aim assist": "off", "vehicles destructible": "off"}), None, None)
REVISION_24_SEEDS = ("AE GRIFBALL", "AE BOMB")

# revision 26 (owner, 2026-10-26): AE SND, AE SEARCH AND DESTROY, an added seed (no migration) in the builds with Bomb:
# AE BOMB's row with the Bomb variant's SEARCH AND DESTROY bit (ae_bomb_snd_rules.h, flags bit 7: not in the rules
# log), the score to win 4 (as config game.bomb_snd_rounds_to_win) and no game time limit
ROWS["AE SND"] = (dict(ROWS["AE BOMB"][0], **{"score to win": "4", "time limit": "none"}), _spawn, 2)
REVISION_26_SEEDS = ("AE SND",)

# revision 13: two frag grenades at the start in every AE gametype that starts with them, and the AE BR start's assault
# rifle second (ARENA_BR_START); the rows above still say the revisions before
for _name, (_want, _spawn, _frags) in list(ROWS.items()):
    if _want.get("starting frags") == "0" and _name not in NHE_ROWS and not _name.startswith("NHE ") and _frags == 4:
        ROWS[_name] = (dict(_want, **{"starting frags": "2"}), _spawn, 2)
    if _name in BR_TWINS:
        _want, _spawn, _frags = ROWS[_name]
        ROWS[_name] = (dict(_want, loadout="assault rifle + assault rifle"), "battle_rifle + assault rifle", _frags)

# revision 25: the five shipped packs named in the gametypes GAME OPTIONS can start them from (their rows' adjust): not
# the AE COMP set, its AE BR versions, the OCTAGON drills, AE TRAINING or the NHE set
PACKS_NAMED = "battle_rifle + smg + smg_silenced + carbine + ar_silenced (slots 1, 2, 3, 4, 5)"
for _name, (_want, _, _) in ROWS.items():
    if (_name in NHE_ROWS or _name.startswith("NHE ") or _name.startswith(("AE COMP", "AE C2V2", "AE BR C ", "AE BR C2", "AE OCT"))
            or _name == "AE TRAINING"):
        continue
    _want["weapon pack"] = PACKS_NAMED

# revision 27 (owner, 2026-10-30): SPAWNS MODERN in every AE gametype; the NHE set keeps CLASSIC
for _name, (_want, _, _) in ROWS.items():
    _want["spawns"] = "classic" if _name in NHE_ROWS or _name.startswith("NHE ") else "modern"

# and the AE BTB and AE COMP sets start with the EVO rifle in the PRIMARY slot (its own weapon an assault rifle where
# the pack is not) and an assault rifle second; their AE BR twins keep the battle rifle
EVO_START_SEEDS = ("AE BTB SLY", "AE BTB CTF", "AE BTB KING", "AE BTB BALL", "AE BTB HVY", "AE COMP FFA", "AE COMP TS",
                   "AE COMP CTF", "AE COMP KOH", "AE COMP OB", "AE C2V2 TS", "AE C2V2 CTF", "AE C2V2 KOH", "AE C2V2 OB")
for _name in EVO_START_SEEDS:
    _want, _, _frags = ROWS[_name]
    _pack = PACKS_NAMED.replace(" (slots 1, 2, 3, 4, 5)", " + evo_rifle (slots 1, 2, 3, 4, 5, 7)") if _name.startswith("AE BTB") \
        else "evo_rifle (slot 7)"
    ROWS[_name] = (dict(_want, **{"loadout": "assault rifle + assault rifle", "pack loadout primary": "pack",
                                  "weapon pack": _pack}), "dmr + assault rifle", _frags)  # (the log names the EVO rifle by its tag, dmr)


def check(runs, prefix):
    """{name: (passed, problems)} for every seed run under runs/<prefix><name, spaces as _>/*/debug.txt"""
    out = {}
    for name, (want, spawn, frags) in ROWS.items():
        paths = glob.glob(f"{runs}/{prefix}{name.replace(' ', '_')}/*/debug.txt")
        if not paths:
            out[name] = (False, {"run": "missing"})
            continue
        text = open(paths[0], errors="replace").read()
        got = fields(text)
        m = re.search(r"AE rules: .*starting frags (\w+)", text)
        if m:
            got["starting frags"] = m[1]
        bad = {k: (v, got.get(k)) for k, v in want.items() if got.get(k) != v}
        m = re.search(r"first spawn: ([a-z_ ]+?(?: \+ [a-z_ ]+?)?), (\d+) frag", text)
        if spawn and (not m or m[1] != spawn or int(m[2]) != frags):
            bad["first spawn"] = ((spawn, frags), (m[1], int(m[2])) if m else None)
        out[name] = (not bad, bad)
    return out


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("runs", nargs="?", help="the runs folder (ae_test/out/run)")
    p.add_argument("--prefix", default="seed-", help="each run's --out-name before the name (spaces as _)")
    p.add_argument("--names", action="store_true", help="print the seeds' stored names, one a line")
    a = p.parse_args(argv)
    if a.names:
        print("\n".join(ROWS))
        return 0
    if not a.runs:
        p.error("the runs folder is needed")
    results = check(a.runs, a.prefix)
    for name, (passed, bad) in results.items():
        print(f"{name:12} {'PASS' if passed else 'FAIL'}" + ("" if passed else f" {bad}"))
    failed = sum(1 for passed, _ in results.values() if not passed)
    print(len(results), "gametypes:", "ALL PASS" if not failed else f"{failed} FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
