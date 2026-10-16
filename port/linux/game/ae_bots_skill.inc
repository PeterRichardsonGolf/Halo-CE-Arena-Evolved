/*
AE_BOTS_SKILL.INC

Included in bots.c right after its skill table (one AE hook line), so that
Arena Evolved's RECRUIT level sits below EASY without a fifth row in the
table of OpenCE pull request #367: bots.skill = "recruit". For new players
and children (owner 2026-10-15: EASY "will clap my poor kids"): it reacts
late, sees less, starts far off its target and settles slowly, turns slowly,
fires short bursts with long pauses, throws no grenades, does not jump, takes
no lead, and roams the map more often than it heads for the nearest enemy.
*/

#define AE_BOTS_SKILL_RECRUIT NUMBER_OF_BOTS_SKILLS

static struct bots_skill_definition const ae_bots_recruit =
	{ "recruit", 1.4f, 16.0f, 80.0f, 22.0f, 2.5f, 5.0f, 2.6f, 0.0f, 0.0f, 0.0f, 4, 24 };

/* the skill's definition: the table's, or RECRUIT's */
static struct bots_skill_definition const *ae_bots_skill_definition(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? &ae_bots_recruit : &bots_skills[skill];
}

/* the chance a roam goes towards an enemy (the place nearest a living one)
rather than anywhere: bots.c's 0.65, RECRUIT's 0.2 */
static real ae_bots_hunt_chance(short skill)
{
	return skill == AE_BOTS_SKILL_RECRUIT ? 0.2f : 0.65f;
}
