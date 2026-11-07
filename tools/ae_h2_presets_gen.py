#!/usr/bin/env python3
"""Generates port/linux/game/ae_h2_presets_data.inc, the table of AE's 41 preset gametypes (ae_h2_presets.c), from
a local JSON file of the second-generation settings (h2_settings.json, kept out of the repository) and its
SETTINGS.md (option labels). Usage: ae_h2_presets_gen.py <h2_settings.json> <SETTINGS.md> <out.inc>"""
import json, re, sys

ENGINES = ("slayer", "oddball", "king", "juggernaut", "ctf", "assault", "territories")
SCORE_KEY = {"slayer": "slayer_match/slayer_score_to_win", "oddball": "oddball_match/oddball_score_to_win",
             "king": "king_match/king_score_to_win", "juggernaut": "juggernaut_match/juggernaut_score_to_win",
             "ctf": "ctf_match/ctf_score_to_win", "assault": "assault_match/assault_score_to_win",
             "territories": "territories_match/territories_score_to_win"}

# (preset name in the data, stored name, display name, engine, base seed, style, group); a base seed is the existing AE
# seed whose rules the preset keeps, changing only what the preset's own settings change
CASUAL, COMP = "AE CASUAL", "AE COMP"
TEAM_ARENA, FFA, ACTION, OBJECTIVE = "TEAM ARENA", "FREE FOR ALL", "ACTION SACK", "OBJECTIVE"
ORDER = [
    ("Rockets", "AE ROCKETSX", "AE ROCKETS", "slayer", CASUAL, ACTION), ("Swords", "AE SWORDS", "AE SWORDS", "slayer", CASUAL, ACTION),
    ("Snipers", "AE SNIPERSX", "AE SNIPERS", "slayer", CASUAL, ACTION), ("Phantoms", "AE PHANTMX", "AE PHANTOMS", "slayer", CASUAL, ACTION),
    ("Team Phantoms", "AE TM PHANT", "AE TEAM PHANTOMS", "slayer", CASUAL, TEAM_ARENA),
    # (the free for all one of before, renamed: owner 2026-11-03, "call the old one elimination classic/CE"; its rules
    # on the round modes' machine, ROUND_EXT below)
    ("Elimination", "AE ELIMX", "AE ELIMINATION CLASSIC/CE", "slayer", CASUAL, OBJECTIVE),
    ("Phantom Elim", "AE PHANT EL", "AE PHANTOM ELIM", "slayer", CASUAL, ACTION),
    ("Rocketball", "AE ROCKBALL", "AE ROCKETBALL", "oddball", CASUAL, ACTION),
    ("Swordball", "AE SWORDBAL", "AE SWORDBALL", "oddball", CASUAL, ACTION),
    ("Low Ball", "AE LOWBALL", "AE LOW BALL", "oddball", CASUAL, ACTION),
    ("Phantom King", "AE PHANTKNG", "AE PHANTOM KING", "king", CASUAL, ACTION),
    ("Crazy King", "AE CRAZYKX", "AE CRAZY KING", "king", CASUAL, TEAM_ARENA),
    ("Team Crazy King", "AE TM CRAZY", "AE TEAM CRAZY KING", "king", CASUAL, TEAM_ARENA),
    ("2 on 1", "AE 2 ON 1", "AE 2 ON 1", "juggernaut", CASUAL, ACTION), ("3 on 1", "AE 3 ON 1", "AE 3 ON 1", "juggernaut", CASUAL, ACTION),
    ("Ninjanaut", "AE NINJANT", "AE NINJANAUT", "juggernaut", CASUAL, ACTION),
    ("Dreadnaut", "AE DREADNT", "AE DREADNAUT", "juggernaut", CASUAL, ACTION),
    ("Phantom Fodder", "AE FODDER", "AE PHANTOM FODDER", "juggernaut", CASUAL, ACTION),
    ("CTF Classic", "AE CTF CLAS", "AE CTF CLASSIC", "ctf", CASUAL, TEAM_ARENA),
    ("1 Flag CTF", "AE 1 FLAG", "AE 1 FLAG CTF", "ctf", CASUAL, TEAM_ARENA),
    ("1 Flag CTF Fast", "AE 1FLAG FS", "AE 1 FLAG CTF FAST", "ctf", CASUAL, TEAM_ARENA),
    ("Multi Bomb", "AE MULTI BM", "AE MULTI BOMB", "assault", CASUAL, TEAM_ARENA),
    ("Single Bomb", "AE SNGL BMB", "AE SINGLE BOMB", "assault", CASUAL, TEAM_ARENA),
    ("Single Bomb Fast", "AE SGLBM FS", "AE SINGLE BOMB FAST", "assault", CASUAL, TEAM_ARENA),
    (None, "AE TERRIT", "AE TERRITORIES", "territories", CASUAL, TEAM_ARENA),
    ("3 Plots", "AE 3 PLOTS", "AE 3 PLOTS", "territories", CASUAL, TEAM_ARENA),
    ("Land Grab", "AE LAND GRB", "AE LAND GRAB", "territories", CASUAL, TEAM_ARENA),
    ("Gold Rush", "AE GOLDRUSH", "AE GOLD RUSH", "territories", CASUAL, TEAM_ARENA),
    ("Fiesta", "AE FIESTA", "AE FIESTA", "oddball", CASUAL, ACTION),
    ("Blast Resort", "AE BLASTRES", "AE BLAST RESORT", "assault", CASUAL, TEAM_ARENA),
    ("Control Issues", "AE CONTROL", "AE CONTROL ISSUES", "territories", CASUAL, TEAM_ARENA),
    ("Contention", "AE CONTENT", "AE CONTENTION", "territories", CASUAL, TEAM_ARENA),
]
# the comp versions: (stored, display, engine, base seed, extra settings over the engine's, group)
COMP_ROWS = [
    ("AE C TERRIT", "AE COMP TERRITORIES", "territories", "AE COMP KOH", {"territories_game/number_of_territories": 3}),
    ("AE C ASSLT", "AE COMP ASSAULT", "assault", "AE BOMB", {"assault_game/assault_bomb_type": 2,
        "assault_game/assault_bomb_arm_time": 5, "assault_match/assault_score_to_win": 3}),
]
# the round modes (mode ELIMINATION, ae_round_rules.h; owner 2026-11-03): (stored, display, base seed, style, group,
# settings over the engine's, AEXV bytes by index). One life a round, first to 5 of 9 (rounds byte 8), RESOLVE TIES
# (match flags 0x02), mode 3; revive and the danger zone on (round flags 0). The COMP one starts as AE COMP does (the
# EVO rifle) and keeps revive
ROUND_TEAM = {"slayer_teams/team_play": 1, "players/lives_per_round": 1, "slayer_match/slayer_score_to_win": 5}
ROUND_ROWS = [
    ("AE ELIM TM", "AE ELIMINATION", "AE TEAM SLY", CASUAL, OBJECTIVE, ROUND_TEAM, {0: 8, 2: 0x02, 18: 3}),
    ("AE C ELIM", "AE ELIMINATION COMP", "AE COMP TS", COMP, "TEAM", ROUND_TEAM, {0: 8, 2: 0x02, 18: 3}),
]
# presets of before played on the round modes' machine: AEXV bytes by index (AE ELIMX: mode 3, round flags NO REVIVE
# and NO ZONE, so its free for all rules, best of 5 with 120 s rounds, stay)
ROUND_EXT = {"AE ELIMX": {18: 3, 25: 0x03}}
# existing seeds that take settings from a preset: (stored name, ext field index, value)
MERGES = [("AE COMP KOH", 8, 6), ("PS COMP KOH", 8, 6), ("AE BR C KOH", 8, 6),
          # revision 29: casual AE KING plays an uncontested hill (king_flags bit 0), its battle rifle and EVO starts too
          ("AE KING", 9, 1), ("AE BR KING", 9, 1), ("EV KING", 9, 1)]
BASE = {("slayer", 0): "AE FFA SLAY", ("slayer", 1): "AE TEAM SLY", ("oddball", 0): "AE FFA BALL", ("oddball", 1): "AE TEAM OB",
        ("king", 0): "AE FFA KING", ("king", 1): "AE KING", ("juggernaut", 0): "AE JUGGER", ("juggernaut", 1): "AE JUGGER",
        ("ctf", 1): "AE CTF", ("assault", 1): "AE BOMB", ("territories", 1): "AE KING"}
TWIST = ["score", "lives", "respawn", "suicide", "betrayal", "shields", "radar", "camo", "weapons", "loadout", "ball_speed",
         "crazy", "at_home", "single", "arm", "reset"]

# starting weapon / secondary values -> CE loadout weapon (0 none 1 random 2 assault rifle 3 pistol 4 shotgun
# 5 sniper 6 rocket launcher 7 plasma pistol 8 plasma rifle 9 needler)
WEAPON = {2: 1, 3: 3, 4: 3, 5: 2, 6: 7, 7: 8, 18: 8, 8: 6, 9: 4, 10: 5, 11: 6, 12: 9, 13: 2, 14: 5, 17: 0, 19: 0}
# weapons on the map -> CE weapon set (enum game_engine_weapons); None: no weapons on the map
WEAPON_SET = {0: 0, 1: None, 2: 6, 3: 7, 4: None, 5: 6, 6: 12, 7: 0, 8: 13, 9: 0, 10: 0, 11: 2, 12: 4, 13: 5,
              14: 1, 15: 3, 16: 9, 17: 11, 19: 0}


def labels(md):
    out, group = {}, None
    for line in open(md):
        m = re.match(r"## (\w+)", line)
        if m:
            group = m.group(1)
            continue
        m = re.match(r"- (\w+): (.*)", line)
        if m and group:
            out[group + "/" + m.group(1)] = m.group(2).strip()
    return out


def main():
    data = json.load(open(sys.argv[1]))
    lab = labels(sys.argv[2])
    settings = {v["name"]: v["values"] for v in data["settings"].values()}
    defaults = {}
    for chunk in data["defaults"].values():
        for e, key in SCORE_KEY.items():
            if key in chunk:
                defaults[e] = chunk
    assert set(defaults) == set(ENGINES), defaults.keys()
    presets = {p["name"].replace(" ", ""): p for p in data["presets"]}

    def on(key, v):
        """a two-valued setting is on when its value is the position of 'on' in its labels"""
        vals = settings[key]
        text = lab.get(key, "")
        if text.startswith("on") or text.startswith("normal"):
            return v == vals[0]
        return v == vals[1] if text.startswith("off") else bool(v)

    rows = []
    entries = [(n, s, d, e, st, gr, None) for n, s, d, e, st, gr in ORDER] + \
              [(None, s, d, e, COMP, "TEAM", extra) for s, d, e, base, extra in COMP_ROWS] + \
              [(None, s, d, "slayer", st, gr, extra) for s, d, base, st, gr, extra, raw in ROUND_ROWS]
    comp_base = {s: base for s, d, e, base, extra in COMP_ROWS}
    comp_base.update({s: base for s, d, base, st, gr, extra, raw in ROUND_ROWS})
    raw_ext = dict(ROUND_EXT)
    raw_ext.update({s: raw for s, d, base, st, gr, extra, raw in ROUND_ROWS})
    for name, stored, display, engine, style, group, extra in entries:
        own = dict(presets[name.replace(" ", "")]["settings"]) if name else {}
        if extra:
            own.update(extra)
        base_defaults = defaults[engine]
        has = lambda k: k in own
        g = lambda k, d=0: own.get(k, d)
        teams = own.get("slayer_teams/team_play", base_defaults.get("slayer_teams/team_play", 0))
        if engine in ("ctf", "assault", "territories"):
            teams = 1
        ext = bytearray(48)
        twist = {}
        if has("match/number_of_rounds"):
            rounds = g("match/number_of_rounds")
            ext[0] = 0 if rounds == 0 else rounds + 1
        if has("match/round_time_limit"): ext[1] = g("match/round_time_limit") // 10
        mf = 0
        if has("match/rounds_reset_map") and not on("match/rounds_reset_map", g("match/rounds_reset_map")): mf |= 1
        if has("match/resolve_ties") and on("match/resolve_ties", g("match/resolve_ties")): mf |= 2
        ext[2] = mf
        if teams and has("teams/respawn_time_modifier"):
            v = g("teams/respawn_time_modifier")
            ext[3] = 0 if v == 2 else v + 1
        pf = 0
        if has("players/damage_resistance") and on("players/damage_resistance", g("players/damage_resistance")): pf |= 1
        if has("players/extra_damage") and on("players/extra_damage", g("players/extra_damage")): pf |= 2
        if has("players/shield_type") and g("players/shield_type") == 2: pf |= 4
        if has("equipment/invis") and not on("equipment/invis", g("equipment/invis")): pf |= 8
        if has("equipment/overshields") and not on("equipment/overshields", g("equipment/overshields")): pf |= 0x10
        ext[4] = pf
        if has("players/max_active_players"):
            mx = g("players/max_active_players")
            ext[5] = 0 if mx >= 16 else mx
        if has("equipment/weapons_respawn"): ext[6] = g("equipment/weapons_respawn")
        sf = 0
        for bit, k in ((1, "slayer_game/death_point_loss"), (2, "slayer_game/suicide_point_loss"), (4, "slayer_game/slayer_bonus_points")):
            if has(k) and on(k, g(k)): sf |= bit
        ext[7] = sf
        if has("king_game/moving_hill"): ext[8] = g("king_game/moving_hill") // 10
        kf = 0
        for bit, k in ((1, "king_game/uncontested_hill"), (2, "king_game/team_time_multiplier"), (4, "king_game/camo_on_hill"),
                       (8, "king_game/damage_resistance"), (0x10, "king_game/extra_damage")):
            if has(k) and on(k, g(k)): kf |= bit
        ext[9] = kf
        of = 0
        if has("oddball_game/ball_waypoint"): of |= g("oddball_game/ball_waypoint") + 1
        if has("oddball_game/ball_camo") and on("oddball_game/ball_camo", g("oddball_game/ball_camo")): of |= 0x10
        if has("oddball_game/ball_toughness") and on("oddball_game/ball_toughness", g("oddball_game/ball_toughness")): of |= 0x20
        if has("oddball_game/vehicle_operation") and not on("oddball_game/vehicle_operation", g("oddball_game/vehicle_operation")): of |= 0x40
        ext[10] = of
        if engine == "ctf":
            ext[11] = {0: 1, 1: 2, 2: 3}[own.get("ctf_game/ctf_flag_type", 0)]
            if has("ctf_game/ctf_flag_reset_time"): ext[12] = g("ctf_game/ctf_flag_reset_time")
            cf = 0
            for bit, k in ((1, "ctf_game/ctf_flag_return"), (2, "ctf_game/ctf_sudden_death"), (4, "ctf_game/ctf_slow_flag"),
                           (8, "ctf_game/ctf_flag_damage"), (0x10, "ctf_game/ctf_active_camo"), (0x20, "ctf_game/ctf_damage_resistance")):
                if has(k) and on(k, g(k)): cf |= bit
            if has("ctf_game/ctf_flag_driving") and not on("ctf_game/ctf_flag_driving", g("ctf_game/ctf_flag_driving")): cf |= 0x40
            ext[13] = cf
            if has("ctf_game/ctf_ownflag_waypoint"): ext[14] = {0: 0, 1: 1, 2: 2, 3: 4}[g("ctf_game/ctf_ownflag_waypoint")]
        if engine == "assault":
            ext[15] = {0: 1, 1: 2, 2: 3}[own.get("assault_game/assault_bomb_type", 0)]
            bf = 0
            for bit, k in ((1, "assault_game/assault_sticky_arming"), (2, "assault_game/assault_touch_return"),
                           (4, "assault_game/assault_sudden_death"), (8, "assault_game/assault_bomb_speed"),
                           (0x10, "assault_game/assault_bomb_damage"), (0x40, "assault_game/assault_active_camo"),
                           (0x80, "assault_game/assault_damage_resistance")):
                if has(k) and on(k, g(k)): bf |= bit
            if has("assault_game/assault_bomb_driving") and not on("assault_game/assault_bomb_driving", g("assault_game/assault_bomb_driving")): bf |= 0x20
            ext[16] = bf
            if has("assault_game/assault_enemy_bomb_indicator"): ext[17] = g("assault_game/assault_enemy_bomb_indicator") + 1
            ext[18] = 1
        if engine == "territories":
            ext[18] = 2
            ext[21] = own.get("territories_game/number_of_territories", base_defaults["territories_game/number_of_territories"])
            ext[22] = own.get("territories_game/control_time", base_defaults["territories_game/control_time"])
            ext[23] = own.get("territories_game/contest_time", base_defaults["territories_game/contest_time"])
        if engine == "juggernaut":
            if has("juggernaut_game/jug_movement"): ext[19] = g("juggernaut_game/jug_movement") + 1
            jf = 0
            for bit, k in ((1, "juggernaut_game/jug_active_camo"), (2, "juggernaut_game/jug_damage_resistance"),
                           (4, "juggernaut_game/jug_extra_damage"), (8, "juggernaut_game/jug_infinite_ammo"),
                           (0x10, "juggernaut_game/jug_motion_sensor"), (0x20, "juggernaut_game/jug_overshield"),
                           (0x40, "juggernaut_game/betrayal_point_loss")):
                if has(k) and on(k, g(k)): jf |= bit
            ext[20] = jf
        for index, value in raw_ext.get(stored, {}).items():
            ext[index] = value
        # the CE side: only what the preset's own settings change
        sk = SCORE_KEY[engine]
        score = g(sk)
        if engine in ("oddball", "king", "territories"):
            score = max(1, round(score / 60.0)) if score else 0
        pw, sw = g("equipment/starting_weapon"), g("equipment/secondary_weapon")
        primary = 3 if not has("equipment/starting_weapon") or pw == 0 else WEAPON[pw]
        secondary = 2 if not has("equipment/secondary_weapon") or sw == 0 else (0 if sw == 1 else (1 if sw == 2 else WEAPON.get(sw, 0)))
        if sw == 3: secondary = 2
        wset = WEAPON_SET[g("equipment/weapons_on_map")] if has("equipment/weapons_on_map") else 0
        vehicles = 1 if has("vehicles/primary_land_vehicle") and g("vehicles/primary_land_vehicle") == 7 and g("vehicles/primary_heavy_vehicle") == 4 else 0
        base = BASE[(engine, teams if engine in ("slayer", "oddball", "king") else 1)]
        if stored in comp_base: base = comp_base[stored]
        bits = 0
        for n, k in enumerate(TWIST):
            use = {"score": has(sk), "lives": has("players/lives_per_round"), "respawn": has("players/respawn_time"),
                   "suicide": has("players/suicide_penalty"), "betrayal": has("teams/betrayal_penalty"),
                   "shields": has("players/shield_type") and g("players/shield_type") == 1,
                   "radar": has("players/motion_sensor"), "camo": has("players/active_camo"),
                   "weapons": has("equipment/weapons_on_map"),
                   "loadout": has("equipment/starting_weapon") or has("equipment/secondary_weapon"),
                   "ball_speed": has("oddball_game/ball_speed"), "crazy": ext[8] != 0 and engine == "king",
                   "at_home": has("ctf_game/ctf_flag_at_home_to_score"),
                   "single": engine == "ctf" and own.get("ctf_game/ctf_flag_type", 0) == 1,
                   "arm": has("assault_game/assault_bomb_arm_time"), "reset": has("assault_game/assault_bomb_reset_time")}[k]
            if use: bits |= 1 << n
        rows.append(dict(
            stored=stored, display=display, base=base, style=style, group=group, twist=bits, engine=ENGINES.index(engine),
            teams=1 if teams else 0, score=score, lives=g("players/lives_per_round"), respawn=g("players/respawn_time"),
            suicide=g("players/suicide_penalty"), betrayal=g("teams/betrayal_penalty"),
            no_shields=1 if g("players/shield_type") == 1 else 0, radar=1 if on("players/motion_sensor", g("players/motion_sensor", 1)) else 0,
            camo=1 if has("players/active_camo") and on("players/active_camo", g("players/active_camo")) else 0,
            wset=wset or 0, no_map=1 if has("equipment/weapons_on_map") and WEAPON_SET[g("equipment/weapons_on_map")] is None else 0,
            primary=primary, secondary=secondary, vehicles=vehicles, ball_speed=g("oddball_game/ball_speed"),
            at_home=1 if has("ctf_game/ctf_flag_at_home_to_score") and on("ctf_game/ctf_flag_at_home_to_score", g("ctf_game/ctf_flag_at_home_to_score")) else 0,
            single=g("match/round_time_limit") if engine == "ctf" and own.get("ctf_game/ctf_flag_type", 0) == 1 else 0,
            arm=g("assault_game/assault_bomb_arm_time"), reset=g("assault_game/assault_bomb_reset_time"), merge=0, ext=list(ext)))
    for stored, index, value in MERGES:
        ext = [0] * 48
        ext[index] = value
        rows.append(dict(stored=stored, display="", base="", style="", group="", twist=0, engine=0, teams=0, score=0, lives=0,
                         respawn=0, suicide=0, betrayal=0, no_shields=0, radar=1, camo=0, wset=0, no_map=0, primary=3, secondary=2,
                         vehicles=0, ball_speed=0, at_home=0, single=0, arm=0, reset=0, merge=1, ext=ext))

    ENGINE_TEXT = ("Slayer", "Oddball", "King of the Hill", "Juggernaut", "Capture the flag", "Bomb", "Territories")
    BUILDER = {("slayer", 0): "build_game_variant_slayer", ("slayer", 1): "build_game_variant_team_slayer",
               ("oddball", 0): "build_game_variant_oddball", ("oddball", 1): "build_game_variant_team_oddball",
               ("king", 0): "build_game_variant_king", ("king", 1): "build_game_variant_team_king",
               ("juggernaut", 0): "build_game_variant_juggernaut", ("juggernaut", 1): "build_game_variant_juggernaut",
               ("ctf", 1): "build_game_variant_ctf", ("assault", 1): "build_game_variant_bomb",
               ("territories", 1): "build_game_variant_team_king"}
    WEAPON_NAME = {0: "no weapon", 1: "random weapons", 2: "assault rifle", 3: "pistol", 4: "shotgun", 5: "sniper rifle",
                   6: "rocket launcher", 7: "plasma pistol", 8: "plasma rifle", 9: "needler"}

    def describe(r):
        e = ENGINES[r["engine"]]
        e_ext, tw, bits = r["ext"], r["twist"], []
        if e_ext[18] == 3:
            bits.append("one life a round")
        if e_ext[0]: bits.append("%d rounds" % (1, 2, 4, 6)[e_ext[0] - 1] if e_ext[0] < 5 else "best of %d" % (3 + 2 * (e_ext[0] - 5)))
        if e_ext[1]: bits.append("%d s rounds" % (e_ext[1] * 10))
        if tw & 2 and e_ext[18] != 3: bits.append("%d %s" % (r["lives"], "life" if r["lives"] == 1 else "lives"))
        if tw & 128: bits.append("everyone camouflaged")
        if tw & 64 and not r["radar"]: bits.append("no motion sensor")
        if tw & 512: bits.append("start with " + WEAPON_NAME[r["primary"]])
        if r["no_map"]: bits.append("no map weapons")
        if r["vehicles"]: bits.append("no vehicles")
        if e_ext[8]: bits.append("the hill moves every %d s" % (e_ext[8] * 10))
        if e_ext[4] & 4: bits.append("overshields")
        if e_ext[5]: bits.append("%d play at once" % e_ext[5])
        if e == "territories": bits.append("%d territories" % e_ext[21])
        if e_ext[18] == 3 and not e_ext[25] & 2: bits.append("a closing danger zone")
        if e_ext[18] == 3 and r["teams"] and not e_ext[25] & 1: bits.append("teammates revive")
        if e == "assault": bits.append(("one bomb each team", "a bomb for the attackers", "one bomb in the middle")[e_ext[15] - 1])
        lead = "AE's %s" % ENGINE_TEXT[r["engine"]].lower() if r["engine"] else "AE's Slayer"
        return ("%s with AE's rules%s%s." % (lead, (": " + ", ".join(bits)) if bits else "", " (beta)" if e == "territories" else ""))[:160]

    news = [r for r in rows if not r["merge"]]
    for r in news:
        r["description"] = describe(r)
    out = ["/* Generated by tools/ae_h2_presets_gen.py: do not edit. */\n"]
    out.append("static struct ae_h2_preset const ae_h2_presets[] =\n{\n")
    for r in rows:
        out.append("\t{ \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", %du, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n\t\t{ %s } },\n" % (
            r["stored"], r["display"], r["base"], r["style"], r["group"], r["twist"], r["engine"], r["teams"], r["score"],
            r["lives"], r["respawn"], r["suicide"], r["betrayal"], r["no_shields"], r["radar"], r["camo"], r["wset"],
            r["no_map"], r["primary"], r["secondary"], r["vehicles"], r["ball_speed"], r["at_home"], r["single"], r["arm"],
            r["reset"], r["merge"], 0, ", ".join(str(b) for b in r["ext"])))
    out.append("};\n")
    open(sys.argv[3], "w").write("".join(out))
    names = []
    for r in news:
        e = ENGINES[r["engine"]]
        engine = {"slayer": "game_engine_slayer", "oddball": "game_engine_oddball", "king": "game_engine_king",
                  "juggernaut": "game_engine_oddball", "ctf": "game_engine_ctf", "assault": "game_engine_bomb",
                  "territories": "game_engine_king"}[e]
        names.append('\t{ "%s", "%s", ARENA_NEW_MODES_SET, %s, %s, FALSE,\n\t\t"%s" },\n' % (
            r["stored"], r["display"], engine, "TRUE" if r["teams"] else "FALSE", r["description"]))
    # the START switch (seed revision 29): a preset that does not set the starting weapon has a battle rifle start and a
    # third one (the EVO rifle; the pistol for a COMP preset, which starts with the EVO rifle), each a stored name of its
    # own ("BR ROCKETS", "EV ROCKETS"/"PS ..."): no row, the base row built and the start put over it
    # (arena_gametypes.c: arena_gametype_starts, derived)
    starts, seen = [], set(r["stored"] for r in news)
    for r in news:
        if r["twist"] & 512:
            continue
        assert r["stored"].startswith("AE ") and r["display"].startswith("AE "), r["stored"]
        suffix, shown = r["stored"][3:], r["display"][3:]
        comp = r["base"].startswith(("AE COMP", "AE C2V2"))  # (the base starts with the EVO rifle)
        third = ("PS " if comp else "EV ") + suffix
        br = "BR " + suffix
        assert br not in seen and third not in seen, (br, third)
        seen.update((br, third))
        e = ENGINES[r["engine"]]
        engine = {"slayer": "game_engine_slayer", "oddball": "game_engine_oddball", "king": "game_engine_king",
                  "juggernaut": "game_engine_oddball", "ctf": "game_engine_ctf", "assault": "game_engine_bomb",
                  "territories": "game_engine_king"}[e]
        team = "TRUE" if r["teams"] else "FALSE"
        for stored, label, text in ((br, "BR", "battle rifle"), (third, "PISTOL" if comp else "EVO", "pistol" if comp else "EVO RIFLE")):
            assert len("AE %s %s" % (label, shown)) <= 31
            names.append('\t{ "%s", "AE %s %s", ARENA_NEW_MODES_SET, %s, %s, FALSE,\n\t\t"The %s start of %s." },\n' % (
                stored, label, shown, engine, team, text, r["display"]))
        starts.append('\t{ "%s", "%s", "%s", %s, FALSE, TRUE },\n' % (
            r["stored"], br, third, "_arena_gametype_start_pistol" if comp else "_arena_gametype_start_evo"))
        r["derived"] = (br, third)
    open(sys.argv[3].replace("_data", "_starts"), "w").write("/* Generated by tools/ae_h2_presets_gen.py */\n" + "".join(starts))
    open(sys.argv[3].replace("_data", "_names"), "w").write("/* Generated by tools/ae_h2_presets_gen.py */\n" + "".join(names))
    groups = ['\t{ "%s", "%s", "%s" },\n' % (r["stored"], r["style"], r["group"]) for r in news]
    groups += ['\t{ "%s", "%s", "%s" },\n' % (d, r["style"], r["group"]) for r in news for d in r.get("derived", ())]
    open(sys.argv[3].replace("_data", "_groups"), "w").write("/* Generated by tools/ae_h2_presets_gen.py */\n" + "".join(groups))
    rowsc = []
    for n, r in enumerate(rows):
        if r["merge"]:
            continue
        e = ENGINES[r["engine"]]
        builder = BUILDER[(e, r["teams"] if e in ("slayer", "oddball", "king") else 1)]
        line = '\t{ "%s", %s, ARENA_NEW_MODE_FLAGS, 0, 0, ARENA_STOCK, ARENA_STOCK, ARENA_RADAR_STOCK, ARENA_VEHICLES_STOCK, TRUE, .h2 = %d },\n' % (
            r["stored"], builder, n + 1)
        if e == "assault":
            line = "#ifdef HALO_GAME_BROWSER\n" + line + "#endif\n"
        rowsc.append(line)
    open(sys.argv[3].replace("_data", "_rows"), "w").write("/* Generated by tools/ae_h2_presets_gen.py */\n" + "".join(rowsc))
    if len(sys.argv) > 4:
        lines = ["| stored | display | base seed | style / group | what it changes |", "|---|---|---|---|---|"]
        for r in news:
            lines.append("| %s | %s | %s | %s / %s | %s |" % (r["stored"], r["display"], r["base"], r["style"], r["group"], r["description"].split(": ", 1)[-1] if ": " in r["description"] else "AE ext block only"))
        open(sys.argv[4], "w").write("\n".join(lines) + "\n")
    print(len(news), "new gametypes,", len(rows) - len(news), "merges")


main()
