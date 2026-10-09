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
does not start with one team.) It compares the "game rules", "AE rules" and "player rules" lines, "vehicles
placed" and the first spawn's weapons and frag grenades. Exit 0 when every seed passes.
"""
import argparse
import glob
import re
import sys

T = lambda s: f"{s} ticks ({s / 30:g} s)"

NHE_COMMON = {
    "health": "classic", "fall damage": "on", "starting equipment": "generic", "vehicle set": "1",
    "time limit": "none", "timers": "off", "no spread": "nhe", "pre-game countdown": "on",
    "friendly fire": "on", "shields": "on", "loadout": "category", "no map weapons": "off",
    "vehicle sets": "none (red none, blue none)",
    "timers level": "off", "spawn heat": "on", "objective": "normal", "nhe extras": "on", "drop secondary": "ce",
    "ball melee": "stock",
    "lives": "0", "health%": "100%", "respawn growth": "0 ticks", "odd man out": "off", "friend indicators": "on",
    "auto team balance": "off", "friendly fire penalty": "0 s", "vehicles placed": "0",
}
SLAYER = {"death bonus": "off", "kill penalty": "off", "kill in order": "off"}
CTF = {"flag at home to score": "on", "assault": "off", "single flag": "off"}
BALL = {"ball": "normal", "balls": "1", "speed with ball": "slow", "with ball": "none", "without ball": "none",
        "random start": "off"}
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
                "balls", "speed with ball", "with ball", "without ball", "random start", "race", "team scoring"]:
        mm = re.search(rf", {re.escape(key)} ((?:[^,(]|\([^)]*\))*)", second)
        if mm: out[key] = mm[1].strip()
    m = re.search(r"AE rules: (.*)", text)
    for key in ["timers level", "spawn heat", "objective", "nhe extras", "drop secondary", "nhe mode", "ball melee"]:
        mm = re.search(rf"(?:^|, ){re.escape(key)} ([^,]*)", m[1] if m else "")
        if mm: out[key] = mm[1].strip()
    m = re.search(r"player rules: (.*)", text)
    for key, name in [("lives", "lives"), ("health%", "health"), ("respawn growth", "respawn growth"),
                      ("odd man out", "odd man out"), ("friend indicators", "friend indicators"),
                      ("auto team balance", "auto team balance"), ("friendly fire penalty", "friendly fire penalty")]:
        mm = re.search(rf"(?:^|, ){re.escape(name)} ([^,]*)", m[1] if m else "")
        if mm: out[key] = mm[1].strip()
    m = re.search(r"vehicles placed: (\d+)", text)
    if m: out["vehicles placed"] = m[1]
    return out



AE = {"health": "halo 2", "fall damage": "off", "starting equipment": "generic", "no spread": "nhe",
      "pre-game countdown": "on", "friendly fire": "on", "shields": "on", "invisible": "off",
      "infinite grenades": "off", "no map weapons": "off", "nhe extras": "off", "drop secondary": "always",
      "lives": "0", "odd man out": "off", "friendly fire penalty": "0 s", "auto team balance": "off",
      "spawn heat": "on", "nhe mode": "by vehicles", "ball melee": "stock"}
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
    "AE SWAT": (TSX(**{"score to win": "50", "radar players": "none", "weapon set": "pistols", "health": "classic", "shields": "off", "loadout": "pistol + none", "starting frags": "none"}), "pistol", 0),
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
# revision 6: AE VANILLA / AE POWERUPS on the casual set's values with their NHE modes
for n, mode in (("AE VANILLA", "vanilla"), ("AE POWERUPS", "nhe & powerups")):
    ROWS[n] = (dict(CASUAL, engine="slayer (teams)", **{"score to win": "50", "respawn": T(300), "suicide penalty": T(300), "vehicle set": "2", "nhe mode": mode}), "pistol + assault rifle", 4)


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
        m = re.search(r"first spawn: ([a-z ]+?(?: \+ [a-z ]+?)?), (\d+) frag", text)
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
