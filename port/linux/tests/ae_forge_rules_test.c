/* ae_forge_rules.c: a "!F" chat line is Forge's only inside a Forge session, and a normal game keeps its version */
#include <stdio.h>
#include <string.h>

#include "ae_forge_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static char const *const lines[] = { "!FS:1:2:3", "!FH:7", "!FQ", "!FGN:x", "!F", "!Fz" };
	static char const *const chat[] = { "", "hello", "!", "!f lower", "F!S", " !FS:1", "!G:1", "?FS" };
	unsigned int index;

	for (index = 0; index < sizeof(lines) / sizeof(lines[0]); index++)
	{
		CHECK(ae_forge_rules_is_forge_line(lines[index]) == 1);
		/* a Forge line switches nothing on in a game that did not start as a Forge session */
		CHECK(ae_forge_rules_line_accepted(0, lines[index]) == 0);
		CHECK(ae_forge_rules_line_accepted(1, lines[index]) == 1);
	}
	for (index = 0; index < sizeof(chat) / sizeof(chat[0]); index++)
	{
		CHECK(ae_forge_rules_is_forge_line(chat[index]) == 0);
		CHECK(ae_forge_rules_line_accepted(0, chat[index]) == 0);
		CHECK(ae_forge_rules_line_accepted(1, chat[index]) == 0);
	}
	CHECK(ae_forge_rules_is_forge_line(0) == 0);
	CHECK(ae_forge_rules_line_accepted(1, 0) == 0);
	/* a normal game advertises its own version, a Forge session Forge's */
	/* a session starts on a machine that asked for it; on a client only by Forge's own join, never by the host's variant name or a menu */
	CHECK(ae_forge_rules_session_may_start(0, 0, 1) && !ae_forge_rules_session_may_start(0, 0, 0) && ae_forge_rules_session_may_start(0, 1, 1));
	CHECK(!ae_forge_rules_session_may_start(1, 0, 1) && !ae_forge_rules_session_may_start(1, 0, 0) && ae_forge_rules_session_may_start(1, 1, 0) &&
		ae_forge_rules_session_may_start(1, 1, 1));
	CHECK(ae_forge_rules_network_version(0, 44, 0x4777) == 44);
	CHECK(ae_forge_rules_network_version(1, 44, 0x4777) == 0x4777);
	{
		/* a Forge session takes the keys Forge binds (F1 mode, F2 palette, F4 third person, F5 quick save by default), and
		a game that is not a session takes none; a rebound key is taken as a default one */
		int const forge_vks[] = { 0x70, 0x71, 0x73, 0x74, 0x46 };

		CHECK(ae_forge_rules_key_taken(1, 0x73, forge_vks, 5) == 1);
		CHECK(ae_forge_rules_key_taken(1, 0x74, forge_vks, 5) == 1);
		CHECK(ae_forge_rules_key_taken(1, 0x46, forge_vks, 5) == 1);
		CHECK(ae_forge_rules_key_taken(1, 0x75, forge_vks, 5) == 0);
		CHECK(ae_forge_rules_key_taken(1, 0x77, forge_vks, 5) == 0);
		CHECK(ae_forge_rules_key_taken(0, 0x73, forge_vks, 5) == 0);
		CHECK(ae_forge_rules_key_taken(1, 0, forge_vks, 5) == 0);
		CHECK(ae_forge_rules_key_taken(1, 0x73, 0, 0) == 0);
		CHECK(ae_forge_rules_key_taken(1, 0x73, forge_vks, 0) == 0);
	}
	{
		char other[64];

		/* a vehicle has one name in the multiplayer maps and another in the campaign's */
		CHECK(ae_forge_rules_tag_alias("vehicles\\warthog\\mp_warthog", other, sizeof(other)) && !strcmp(other, "vehicles\\warthog\\warthog"));
		CHECK(ae_forge_rules_tag_alias("Vehicles/Warthog/Warthog", other, sizeof(other)) && !strcmp(other, "vehicles/warthog/mp_warthog"));
		CHECK(ae_forge_rules_tag_alias("vehicles\\ghost\\ghost_mp", other, sizeof(other)) && !strcmp(other, "vehicles\\ghost\\ghost"));
		CHECK(ae_forge_rules_tag_alias("vehicles\\c gun turret\\c gun turret", other, sizeof(other)) && !strcmp(other, "vehicles\\c gun turret\\c gun turret_mp"));
		CHECK(ae_forge_rules_tag_alias("weapons\\pistol\\pistol", other, sizeof(other)) == 0);
		CHECK(ae_forge_rules_tag_alias("vehicles\\warthog\\mp_warthog", other, 8) == 0);
		CHECK(ae_forge_rules_tag_alias(0, other, sizeof(other)) == 0);
	}
	/* an object marked for a gametype is in games of it only; unmarked, all-marked, no game running: everywhere */
	CHECK(ae_forge_rules_gametype_plays(1, 1) == 1 && ae_forge_rules_gametype_plays(1, 2) == 0 && ae_forge_rules_gametype_plays(2, 2) == 1);
	CHECK(ae_forge_rules_gametype_plays(4, 3) == 1 && ae_forge_rules_gametype_plays(8, 4) == 1 && ae_forge_rules_gametype_plays(16, 5) == 1);
	CHECK(ae_forge_rules_gametype_plays(16, 4) == 0 && ae_forge_rules_gametype_plays(3, 2) == 1 && ae_forge_rules_gametype_plays(3, 3) == 0);
	CHECK(ae_forge_rules_gametype_plays(0, 1) == 1 && ae_forge_rules_gametype_plays(0x0FFF, 1) == 1 && ae_forge_rules_gametype_plays(0x1F, 3) == 1);
	CHECK(ae_forge_rules_gametype_plays(1, 0) == 1 && ae_forge_rules_gametype_plays(1, 6) == 0 && ae_forge_rules_gametype_plays(0, 6) == 1);
	{
		/* a GRAVITY volume's pull in place of the world's: the world's gravity took (0, 0, -1) * amount off a velocity, the pull
		puts back what it says */
		float velocity[3] = { 0.0f, 0.0f, 0.0f }, down[3] = { 0.0f, 0.0f, -1.0f }, quarter[3] = { 0.0f, 0.0f, -0.25f };
		float up[3] = { 0.0f, 0.0f, 1.0f }, side[3] = { 1.0f, 0.0f, 0.0f }, none[3] = { 0.0f, 0.0f, 0.0f };

		ae_forge_rules_gravity_correct(velocity, down, 2.0f);
		CHECK(velocity[0] == 0.0f && velocity[1] == 0.0f && velocity[2] == 0.0f);
		ae_forge_rules_gravity_correct(velocity, quarter, 2.0f);
		CHECK(velocity[0] == 0.0f && velocity[2] == 1.5f);
		velocity[2] = -2.0f;
		ae_forge_rules_gravity_correct(velocity, up, 1.0f);
		CHECK(velocity[2] == 0.0f);
		velocity[2] = 0.0f;
		ae_forge_rules_gravity_correct(velocity, side, 0.5f);
		CHECK(velocity[0] == 0.5f && velocity[2] == 0.5f);
		/* the ground holds up as hard as the pull is strong, and no softer than the world's */
		CHECK(ae_forge_rules_ground_gravity(2.0f, quarter) == 2.0f && ae_forge_rules_ground_gravity(2.0f, none) == 2.0f);
		CHECK(ae_forge_rules_ground_gravity(2.0f, up) == 2.0f);
		{
			float strong[3] = { 0.0f, 0.0f, -3.0f };

			CHECK(ae_forge_rules_ground_gravity(2.0f, strong) == 6.0f);
		}
		/* a pull out of the ground lifts a biped off it; one into it, or along it, does not */
		CHECK(ae_forge_rules_pull_lifts(up, up) == 1 && ae_forge_rules_pull_lifts(down, up) == 0);
		CHECK(ae_forge_rules_pull_lifts(side, up) == 0 && ae_forge_rules_pull_lifts(side, side) == 1);
	}
	{
		/* a vehicle in a volume counts, and for a while after; another vehicle, or a later time, does not */
		long last_vehicle = -1, last_time = 0;

		CHECK(ae_forge_rules_gravity_grace(0, 5, 100, 240, &last_vehicle, &last_time) == 0);
		CHECK(ae_forge_rules_gravity_grace(1, 5, 100, 240, &last_vehicle, &last_time) == 1 && last_vehicle == 5 && last_time == 100);
		CHECK(ae_forge_rules_gravity_grace(0, 5, 339, 240, &last_vehicle, &last_time) == 1);
		CHECK(ae_forge_rules_gravity_grace(0, 5, 340, 240, &last_vehicle, &last_time) == 0);
		CHECK(ae_forge_rules_gravity_grace(0, 6, 150, 240, &last_vehicle, &last_time) == 0);
		CHECK(ae_forge_rules_gravity_grace(0, 5, 50, 240, &last_vehicle, &last_time) == 0);
		/* a unit on foot: noted in a volume, kept in the air, let go once it stands on the ground out of them */
		CHECK(ae_forge_rules_gravity_noted(1, 0, 0) == 1 && ae_forge_rules_gravity_noted(0, 0, 1) == 1);
		CHECK(ae_forge_rules_gravity_noted(0, 1, 1) == 0 && ae_forge_rules_gravity_noted(0, 0, 0) == 0);
		CHECK(ae_forge_rules_gravity_noted(1, 1, 0) == 1);
	}
	{
		/* DEV MODE's forge volume: the kinds, each made as a box of its own, a value in its range; settings: a team, an atmosphere */
		struct ae_forge_volume_made made;

		CHECK(ae_forge_rules_volume_kind("gravity") == AE_FORGE_VOLUME_GRAVITY && ae_forge_rules_volume_kind("KILL") == AE_FORGE_VOLUME_KILL);
		CHECK(ae_forge_rules_volume_kind("damage") == AE_FORGE_VOLUME_DAMAGE && ae_forge_rules_volume_kind("hurt") == AE_FORGE_VOLUME_HURT);
		CHECK(ae_forge_rules_volume_kind("oneway") == AE_FORGE_VOLUME_ONE_WAY && ae_forge_rules_volume_kind("fog") == AE_FORGE_VOLUME_NONE);
		CHECK(ae_forge_rules_volume_kind("") == AE_FORGE_VOLUME_NONE && ae_forge_rules_volume_kind(0) == AE_FORGE_VOLUME_NONE);
		CHECK(ae_forge_rules_volume_made(AE_FORGE_VOLUME_GRAVITY, &made) && made.value_kind == 1 && made.value_default == 25.0f &&
			made.value_max == 1000.0f && made.size[2] == 40.0f);
		/* the gravity column starts just under the feet and rises well above a jump */
		CHECK(made.up - made.size[2] / 2.0f < 0.0f && made.up + made.size[2] / 2.0f > 10.0f);
		CHECK(ae_forge_rules_volume_made(AE_FORGE_VOLUME_KILL, &made) && made.value_kind == 0 && made.up == 0.0f);
		CHECK(ae_forge_rules_volume_made(AE_FORGE_VOLUME_HURT, &made) && made.value_kind == 2 && made.value_min == 1.0f && made.value_max == 100.0f);
		CHECK(ae_forge_rules_volume_made(AE_FORGE_VOLUME_ONE_WAY, &made) && made.up + made.size[2] / 2.0f < 1.0f);
		CHECK(!ae_forge_rules_volume_made(AE_FORGE_VOLUME_NONE, &made) && !ae_forge_rules_volume_made(AE_FORGE_VOLUME_KINDS, &made) &&
			!ae_forge_rules_volume_made(0, 0));
		CHECK(ae_forge_rules_team_slot("red") == 0 && ae_forge_rules_team_slot("Blue") == 1 && ae_forge_rules_team_slot("ffa") == 2 &&
			ae_forge_rules_team_slot("green") == -1 && ae_forge_rules_team_slot(0) == -1);
		CHECK(ae_forge_rules_atmosphere("default") == 0 && ae_forge_rules_atmosphere("HEAVY") == 3 && ae_forge_rules_atmosphere("flood") == 4 &&
			ae_forge_rules_atmosphere("rain") == -1 && ae_forge_rules_atmosphere(0) == -1);
		/* a tag asked for by a part of its last name */
		CHECK(ae_forge_rules_tag_matches("characters\\cyborg\\cyborg_mp", "mp") == 1 && ae_forge_rules_tag_matches("characters/elite/Elite", "elit") == 1);
		CHECK(ae_forge_rules_tag_matches("characters\\cyborg\\cyborg_mp", "elite") == 0 && ae_forge_rules_tag_matches("characters\\cyborg\\cyborg_mp", "") == 0);
		/* (the folders do not count: only the leaf) */
		CHECK(ae_forge_rules_tag_matches("characters\\cyborg\\bipd", "cyborg") == 0 && ae_forge_rules_tag_matches(0, "x") == 0);
	}
	{
		/* DEV MODE's forge ai: a team, a goal, and the two and a respawn in one number; the goal's target the lowest there is */
		int numbers[3] = { 7, 2, 5 };

		CHECK(ae_forge_rules_ai_team("own") == 0 && ae_forge_rules_ai_team("Covenant") == 3 && ae_forge_rules_ai_team("players") == 6 &&
			ae_forge_rules_ai_team("red") == 7 && ae_forge_rules_ai_team("blue") == 8 && ae_forge_rules_ai_team("green") == -1 &&
			ae_forge_rules_ai_team(0) == -1);
		CHECK(ae_forge_rules_ai_goal("none") == 0 && ae_forge_rules_ai_goal("PATROL") == 1 && ae_forge_rules_ai_goal("route") == 2 &&
			ae_forge_rules_ai_goal("hunt") == 3 && ae_forge_rules_ai_goal("follow") == -1 && ae_forge_rules_ai_goal(0) == -1);
		CHECK(ae_forge_rules_ai_unpack_team(ae_forge_rules_ai_pack(8, 3, 5)) == 8 && ae_forge_rules_ai_unpack_goal(ae_forge_rules_ai_pack(8, 3, 5)) == 3 &&
			ae_forge_rules_ai_unpack_respawn(ae_forge_rules_ai_pack(8, 3, 5)) == 5);
		CHECK(ae_forge_rules_ai_unpack_respawn(ae_forge_rules_ai_pack(1, 0, 600)) == 600 && ae_forge_rules_ai_unpack_respawn(ae_forge_rules_ai_pack(1, 0, 9999)) == 600 &&
			ae_forge_rules_ai_unpack_respawn(ae_forge_rules_ai_pack(1, 0, -3)) == 0 && ae_forge_rules_ai_unpack_team(ae_forge_rules_ai_pack(99, 0, 0)) == 15);
		CHECK(ae_forge_rules_ai_link(0, numbers, 3) == 2 && ae_forge_rules_ai_link(5, numbers, 3) == 5 && ae_forge_rules_ai_link(0, numbers, 0) == 0);
		{
			struct ae_forge_volume_made made;

			CHECK(ae_forge_rules_volume_kind("patrol") == AE_FORGE_VOLUME_PATROL && ae_forge_rules_volume_made(AE_FORGE_VOLUME_PATROL, &made) &&
				made.value_kind == 3 && made.value_default == 10.0f && made.up - made.size[2] / 2.0f < 0.0f);
		}
	}
	{
		/* forge ready: no session 0; the level, the edits, the mode waited for -1; all done 1 */
		char text[80];

		CHECK(ae_forge_rules_ready(0, 1, 0, 0, 0, text, sizeof(text)) == 0 && strstr(text, "no session"));
		CHECK(ae_forge_rules_ready(1, 0, 3, 1, 0, text, sizeof(text)) == -1 && strstr(text, "level"));
		CHECK(ae_forge_rules_ready(1, 1, 2, 1, 0, text, sizeof(text)) == -1 && strstr(text, "2 edits"));
		CHECK(ae_forge_rules_ready(1, 1, 0, 1, 0, text, sizeof(text)) == -1 && strstr(text, "mode"));
		CHECK(ae_forge_rules_ready(1, 1, 0, 1, 1, text, sizeof(text)) == -1 && strstr(text, "mode"));
		CHECK(ae_forge_rules_ready(1, 1, 0, 0, 1, text, sizeof(text)) == -1 && strstr(text, "AI"));
		CHECK(ae_forge_rules_ready(1, 1, 0, 0, 0, text, sizeof(text)) == 1 && strstr(text, "ready"));
	}
	{
		/* forge ff: the kinds of marker and prop, a round, a wave, and the three in one number */
		int packed = ae_forge_rules_ff_pack(AE_FORGE_FF_ZONE, 3, 2);

		CHECK(ae_forge_rules_ff_kind("infantry") == AE_FORGE_FF_INFANTRY && ae_forge_rules_ff_kind("Ghost") == AE_FORGE_FF_GHOST &&
			ae_forge_rules_ff_kind("pad") == AE_FORGE_FF_PAD && ae_forge_rules_ff_kind("rack") == AE_FORGE_FF_RACK &&
			ae_forge_rules_ff_kind("drop") == AE_FORGE_FF_DROP && ae_forge_rules_ff_kind("generator") == AE_FORGE_FF_GENERATOR &&
			ae_forge_rules_ff_kind("tank") == AE_FORGE_FF_NONE && ae_forge_rules_ff_kind(0) == AE_FORGE_FF_NONE);
		CHECK(ae_forge_rules_ff_round("all") == 0 && ae_forge_rules_ff_round("1") == 1 && ae_forge_rules_ff_round("5") == 5 &&
			ae_forge_rules_ff_round("6") == -1 && ae_forge_rules_ff_round("12") == -1 && ae_forge_rules_ff_round("x") == -1);
		CHECK(ae_forge_rules_ff_unpack_kind(packed) == AE_FORGE_FF_ZONE && ae_forge_rules_ff_unpack_round(packed) == 3 &&
			ae_forge_rules_ff_unpack_wave(packed) == 2);
	}
	{
		/* forge host / join: an address as the harness joins one, a map's and a variant's name */
		CHECK(ae_forge_rules_address("127.0.0.1:2302") && ae_forge_rules_address("0.0.0.0:1") && ae_forge_rules_address("255.255.255.255:65535"));
		CHECK(!ae_forge_rules_address("127.0.0.1") && !ae_forge_rules_address("127.0.0.1:0") && !ae_forge_rules_address("127.0.0.1:65536") &&
			!ae_forge_rules_address("256.0.0.1:1") && !ae_forge_rules_address("1.2.3:4") && !ae_forge_rules_address("1.2.3.4.5:6") &&
			!ae_forge_rules_address("1.2.3.4:5x") && !ae_forge_rules_address("") && !ae_forge_rules_address(0) && !ae_forge_rules_address("a.b.c.d:1") &&
			!ae_forge_rules_address("1.2.3.4:1234567") && !ae_forge_rules_address(" 1.2.3.4:5") && !ae_forge_rules_address("1..3.4:5"));
		CHECK(ae_forge_rules_net_name("bloodgulch") && ae_forge_rules_net_name("my-map_2") && !ae_forge_rules_net_name("") &&
			!ae_forge_rules_net_name("a b") && !ae_forge_rules_net_name("a:b") && !ae_forge_rules_net_name("a\\b") && !ae_forge_rules_net_name(0) &&
			!ae_forge_rules_net_name("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") && ae_forge_rules_net_name("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
	}
	CHECK(ae_forge_rules_raw_line("!FM:0:5:1:2:3:0:0:0") && ae_forge_rules_raw_line("!fm:0:5:1.5:-2:3:0:0:0") && ae_forge_rules_raw_line("!FQ") &&
		!ae_forge_rules_raw_line("FM:0") && !ae_forge_rules_raw_line("!G:0") && !ae_forge_rules_raw_line("!F") && !ae_forge_rules_raw_line("!F1") &&
		!ae_forge_rules_raw_line("!FM:0 1") && !ae_forge_rules_raw_line("!FM:0\\1") && !ae_forge_rules_raw_line(0) &&
		!ae_forge_rules_raw_line("!FMaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") && ae_forge_rules_raw_line("!FMaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_forge_rules_test: all passed\n");

	return 0;
}
