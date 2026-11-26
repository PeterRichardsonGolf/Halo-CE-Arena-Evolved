"""The words a TEST RUN report uses for what was really measured: never a bare PASS.

A list's run is said as  STAGED s/t · MEASURED p/m · PROXY x · LOOK n:

    STAGED    tests whose scene came up as the list asks (no refused setup line, a picture), of the tests played
    MEASURED  checks whose expect line judges the check's own effect and passed, of those checks
    PROXY     checks whose expect line stands in for the check without judging it: a dev command's "ok" (`cmd ... ok`),
              or a screen or dialog opening (`log ...: open`, "... opened") on a check that says more than that the
              screen opens. A proxy that passes says the setup ran, not that the check holds. Not counted as MEASURED
    LOOK      checks with no expect line: the tester's eyes only
    RETIRED   expect lines taken out of judging (`retired <n> <why> | <rule>`), when there are any

Used by run_list.py (its summary.json), preflight.py (its rows and record) and test_run_lint.py (a warning for a proxy).
"""
import re

# a check's words that state an outcome, not a screen showing
EFFECT = re.compile(r"\b(does ?n[o']t|doesn't|never|no longer|deletes?|deleted|saves?|saved|kills?|dies|died|"
                    r"follows?|fires?|shoots?|sees?|seen|lands?|loses?|lost|keeps?|kept|stays?|takes?|gives?|unlocks?|"
                    r"removes?|removed|resets?|changes?|moves?|made it|gets? (?:over|through|past)|works?|joins?|"
                    r"drops?|falls?|spawns?|respawns?|hits?|damages?|blocks?|stops?|starts?|ends?|wins?)\b", re.I)
# a rule that only says a screen or a dialog came up
OPENING = re.compile(r"(:\s*open\b|\bopened\b|\bopens\b)", re.I)
# a check that is only about a screen or dialog coming up (then the opening line measures it)
OPENS_CHECK = re.compile(r"\b(opens?|opened|comes up|is open|is up)\b", re.I)


def proxy_reason(rule, check_text):
    """Why an expect line is a proxy for its check, or "" when it measures it."""
    words = rule.split()
    if not words:
        return ""
    if words[0] == "cmd":
        return "a dev command's answer, not the check's effect"
    if words[0] == "log" and OPENING.search(rule) and (EFFECT.search(check_text or "") or
                                                      not OPENS_CHECK.search(check_text or "")):
        return "a screen or dialog opening, and the check is about more than its opening"
    return ""


def count(report_rows, tests_played, problem_tests, retired=0):
    """{staged, tests, measured_passed, measured, proxy, look, retired} from expect.py's report rows ({test, check,
    text, rule, verdict}), the tests played and the tests with a staging problem."""
    judged = [row for row in report_rows if row.get("rule")]
    proxies = [row for row in judged if proxy_reason(row.get("rule", ""), row.get("text", ""))]
    measured = [row for row in judged if row not in proxies]
    return {"staged": len(set(tests_played) - set(problem_tests)), "tests": len(set(tests_played)),
            "measured_passed": sum(1 for row in measured if row.get("verdict") == "PASS"), "measured": len(measured),
            "proxy": len(proxies), "look": sum(1 for row in report_rows if not row.get("rule")), "retired": retired}


def words(counts):
    """STAGED s/t · MEASURED p/m · PROXY x · LOOK n [· RETIRED r]"""
    text = (f"STAGED {counts['staged']}/{counts['tests']} · MEASURED {counts['measured_passed']}/{counts['measured']}"
            f" · PROXY {counts['proxy']} · LOOK {counts['look']}")
    text += f" · RETIRED {counts['retired']}" if counts.get("retired") else ""
    return text + (f" · FRAME {counts['frame']}" if counts.get("frame") else "")
