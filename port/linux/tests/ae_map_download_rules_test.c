/* GET MAPS's pure rules (ae_map_download_rules.h): the frame, the server's room, the maps wanted, the card's words */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_map_download_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_frame(void)
{
	struct ae_map_download_frame frame, back;
	unsigned char bytes[AE_MAP_DOWNLOAD_FRAME_BYTES];
	long length;

	memset(&frame, 0, sizeof(frame));
	frame.kind = AE_MAP_DOWNLOAD_KIND_HELLO;
	frame.version = 0x0102;
	length = ae_map_download_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length == 4 && bytes[0] == AE_MAP_DOWNLOAD_FRAME_TYPE && bytes[1] == AE_MAP_DOWNLOAD_KIND_HELLO);
	CHECK(bytes[2] == 0x02 && bytes[3] == 0x01);
	CHECK(ae_map_download_frame_decode(bytes, length, &back) && back.kind == AE_MAP_DOWNLOAD_KIND_HELLO &&
		back.version == 0x0102);
	/* (a hello cut short, or of version 0, is no frame) */
	CHECK(!ae_map_download_frame_decode(bytes, 3, &back));
	bytes[2] = bytes[3] = 0;
	CHECK(!ae_map_download_frame_decode(bytes, 4, &back));
	frame.version = 0;
	CHECK(!ae_map_download_frame_encode(&frame, bytes, sizeof(bytes)));

	frame.kind = AE_MAP_DOWNLOAD_KIND_WELCOME;
	length = ae_map_download_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length == 2 && ae_map_download_frame_decode(bytes, length, &back) && back.kind == AE_MAP_DOWNLOAD_KIND_WELCOME);

	frame.kind = AE_MAP_DOWNLOAD_KIND_REFUSE;
	frame.reason = AE_MAP_DOWNLOAD_REFUSED_FULL;
	length = ae_map_download_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length == 3 && ae_map_download_frame_decode(bytes, length, &back) && back.reason == AE_MAP_DOWNLOAD_REFUSED_FULL);
	frame.kind = AE_MAP_DOWNLOAD_KIND_BYE;
	frame.reason = AE_MAP_DOWNLOAD_REFUSED_FILLED;
	length = ae_map_download_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length == 3 && ae_map_download_frame_decode(bytes, length, &back) && back.kind == AE_MAP_DOWNLOAD_KIND_BYE &&
		back.reason == AE_MAP_DOWNLOAD_REFUSED_FILLED);
	/* (a refusal without its reason; an unknown kind; another frame type; too small a buffer) */
	bytes[2] = 0;
	CHECK(!ae_map_download_frame_decode(bytes, 3, &back));
	frame.reason = 0;
	CHECK(!ae_map_download_frame_encode(&frame, bytes, sizeof(bytes)));
	bytes[0] = AE_MAP_DOWNLOAD_FRAME_TYPE;
	bytes[1] = 9;
	CHECK(!ae_map_download_frame_decode(bytes, 4, &back) && back.kind == 0);
	bytes[0] = 4;
	bytes[1] = AE_MAP_DOWNLOAD_KIND_WELCOME;
	CHECK(!ae_map_download_frame_decode(bytes, 2, &back));
	frame.kind = AE_MAP_DOWNLOAD_KIND_WELCOME;
	CHECK(!ae_map_download_frame_encode(&frame, bytes, 1));
	CHECK(!ae_map_download_frame_decode(NULL, 4, &back) && !ae_map_download_frame_decode(bytes, 4, NULL));
}

static void test_room(void)
{
	/* off, a later version, full of peers, too few slots left for joiners; else welcome */
	CHECK(ae_map_download_room(0, 0, 10, AE_MAP_DOWNLOAD_VERSION) == -AE_MAP_DOWNLOAD_REFUSED_OFF);
	CHECK(ae_map_download_room(-1, 0, 10, AE_MAP_DOWNLOAD_VERSION) == -AE_MAP_DOWNLOAD_REFUSED_OFF);
	CHECK(ae_map_download_room(2, 0, 10, AE_MAP_DOWNLOAD_VERSION + 1) == -AE_MAP_DOWNLOAD_REFUSED_VERSION);
	CHECK(ae_map_download_room(2, 2, 10, AE_MAP_DOWNLOAD_VERSION) == -AE_MAP_DOWNLOAD_REFUSED_FULL);
	CHECK(ae_map_download_room(2, 1, 1, AE_MAP_DOWNLOAD_VERSION) == -AE_MAP_DOWNLOAD_REFUSED_FULL);
	CHECK(ae_map_download_room(2, 1, AE_MAP_DOWNLOAD_FREE_SLOTS, AE_MAP_DOWNLOAD_VERSION) == AE_MAP_DOWNLOAD_KIND_WELCOME);
	CHECK(ae_map_download_room(2, 0, 14, AE_MAP_DOWNLOAD_VERSION) == AE_MAP_DOWNLOAD_KIND_WELCOME);

	/* (review 0.9.3: a refused connection's hellos again are not answered nor heard; a peer's are; a peer never joins) */
	CHECK(ae_map_download_hello_action(0, 0) == AE_MAP_DOWNLOAD_HELLO_ANSWER);
	CHECK(ae_map_download_hello_action(0, 1) == AE_MAP_DOWNLOAD_HELLO_IGNORE);
	CHECK(ae_map_download_hello_action(1, 0) == AE_MAP_DOWNLOAD_HELLO_AGAIN);
	CHECK(ae_map_download_hello_action(1, 1) == AE_MAP_DOWNLOAD_HELLO_AGAIN);
	CHECK(!ae_map_download_heard(0) && ae_map_download_heard(1));
	CHECK(ae_map_download_join_allowed(0) && !ae_map_download_join_allowed(1));
	/* a refusal holds while its address is at the slot, 20 s at most; gone once the slot is free or another is there */
	CHECK(ae_map_download_refusal_kept(0x0100007FUL, 0x0100007FUL, 0));
	CHECK(ae_map_download_refusal_kept(0x0100007FUL, 0x0100007FUL, AE_MAP_DOWNLOAD_REFUSAL_MS - 1));
	CHECK(!ae_map_download_refusal_kept(0x0100007FUL, 0x0100007FUL, AE_MAP_DOWNLOAD_REFUSAL_MS));
	CHECK(!ae_map_download_refusal_kept(0x0100007FUL, 0, 0));
	CHECK(!ae_map_download_refusal_kept(0x0100007FUL, 0x0200007FUL, 0));
	CHECK(!ae_map_download_refusal_kept(0, 0, 0));
	/* (the same guest back later: the slot was free between, so its refusal is gone and its hello answered) */
	CHECK(ae_map_download_hello_action(0, ae_map_download_refusal_kept(0x0100007FUL, 0, 100)) ==
		AE_MAP_DOWNLOAD_HELLO_ANSWER);

	CHECK(ae_map_download_peers_setting(-3) == 0 && ae_map_download_peers_setting(0) == 0);
	CHECK(ae_map_download_peers_setting(2) == 2 && ae_map_download_peers_setting(99) == AE_MAP_DOWNLOAD_PEERS_MOST);

	CHECK(!ae_map_download_peer_expired(AE_MAP_DOWNLOAD_IDLE_MS, AE_MAP_DOWNLOAD_VISIT_MS));
	CHECK(ae_map_download_peer_expired(AE_MAP_DOWNLOAD_IDLE_MS + 1, 0));
	CHECK(ae_map_download_peer_expired(0, AE_MAP_DOWNLOAD_VISIT_MS + 1));
}

static int stock(void *context, char const *name)
{
	(void)context;
	return !strcmp(name, "prisoner") || !strcmp(name, "bloodgulch");
}

static int here(void *context, char const *name)
{
	(void)context;
	return !strcmp(name, "gothra@smp");
}

static void test_wanted(void)
{
	char const *names[] = { "ascetic@smp", "prisoner", "gothra@smp", "ASCETIC@smp", "warren@smp", "beavercreek_nhe",
		"", "bloodgulch", NULL };
	int fallbacks[] = { 0, 1, 0, 0, 1, 0, 0, 0, 0 };
	char out[AE_MAP_DOWNLOAD_WANTED][AE_MAP_DOWNLOAD_NAME];
	int count;

	/* (fallbacks, stock maps, maps here, the same map again in another case, an empty name: left out) */
	count = ae_map_download_wanted(names, fallbacks, 9, stock, here, NULL, out, AE_MAP_DOWNLOAD_WANTED);
	CHECK(count == 2);
	CHECK(!strcmp(out[0], "ascetic@smp") && !strcmp(out[1], "beavercreek_nhe"));
	CHECK(ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "Ascetic@SMP"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "ascetic"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "gothra@smp"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, ""));
	/* (a Custom Edition map as the host's list names it: <name>_<8 hex digits>; not another length or a non-digit) */
	CHECK(ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "beavercreek_nhe_0a1b2c3d"));
	CHECK(ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "BeaverCreek_NHE_0A1B2C3D"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "beavercreek_nhe_0a1b2c3"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "beavercreek_nhe_0a1b2c3z"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "beavercreek_nhe-0a1b2c3d"));
	CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, count, "beavercreek_0a1b2c3d"));
	/* (a playlist's <name>@ce: the list's <name>_<8 hex digits>) */
	{
		char ce[1][AE_MAP_DOWNLOAD_NAME] = { "h2_warlock@ce" };

		CHECK(ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])ce, 1, "h2_warlock_9f00e1aa"));
		CHECK(ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])ce, 1, "H2_Warlock_9F00E1AA"));
		CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])ce, 1, "h2_warlock"));
		CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])ce, 1, "h2d_warlock"));
		CHECK(!ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])ce, 1, "h2_warlock@c_9f00e1aa"));
	}
	/* (no fallback marks: every name counts; the most kept to the room given) */
	count = ae_map_download_wanted(names, NULL, 9, stock, here, NULL, out, 1);
	CHECK(count == 1 && !strcmp(out[0], "ascetic@smp"));
	count = ae_map_download_wanted(names, NULL, 9, NULL, NULL, NULL, out, AE_MAP_DOWNLOAD_WANTED);
	CHECK(count == 6);
	CHECK(ae_map_download_wanted(NULL, NULL, 3, NULL, NULL, NULL, out, 4) == 0);
}

static void test_words(void)
{
	char text[160];

	/* (a playlist's Custom Edition map, and the card's note on those a PC without Custom Edition's files skips) */
	CHECK(ae_map_download_needs_custom_edition("h2_warlock@ce") && ae_map_download_needs_custom_edition("X@CE"));
	CHECK(!ae_map_download_needs_custom_edition("h2d_warlock") && !ae_map_download_needs_custom_edition("co2@smp"));
	CHECK(!ae_map_download_needs_custom_edition("@ce") && !ae_map_download_needs_custom_edition(NULL));
	ae_map_download_custom_edition_note(1, "h2_warlock@ce", text, sizeof(text));
	CHECK(!strcmp(text, "h2_warlock needs MCC or Halo PC."));
	ae_map_download_custom_edition_note(3, "h2_warlock@ce", text, sizeof(text));
	CHECK(!strcmp(text, "3 maps need MCC or Halo PC."));
	ae_map_download_custom_edition_note(0, "h2_warlock@ce", text, sizeof(text));
	CHECK(!text[0]);

	CHECK(ae_map_download_outcome_of_refusal(AE_MAP_DOWNLOAD_REFUSED_FULL) == AE_MAP_DOWNLOAD_OUTCOME_FULL);
	CHECK(ae_map_download_outcome_of_refusal(AE_MAP_DOWNLOAD_REFUSED_FILLED) == AE_MAP_DOWNLOAD_OUTCOME_FILLED);
	CHECK(ae_map_download_outcome_of_refusal(AE_MAP_DOWNLOAD_REFUSED_VERSION) == AE_MAP_DOWNLOAD_OUTCOME_OLD);
	CHECK(ae_map_download_outcome_of_refusal(AE_MAP_DOWNLOAD_REFUSED_OFF) == AE_MAP_DOWNLOAD_OUTCOME_OFF);

	ae_map_download_outcome_text(AE_MAP_DOWNLOAD_OUTCOME_DONE, 12, "AE #1", text, sizeof(text));
	CHECK(!strcmp(text, "Got 12 maps. Every playlist map is here."));
	ae_map_download_outcome_text(AE_MAP_DOWNLOAD_OUTCOME_DONE, 1, "AE #1", text, sizeof(text));
	CHECK(!strcmp(text, "Got 1 map. Every playlist map is here."));
	ae_map_download_outcome_text(AE_MAP_DOWNLOAD_OUTCOME_OLD, 0, "AE Comp Test", text, sizeof(text));
	CHECK(!strcmp(text, "AE Comp Test needs version 0.9.3 to share maps."));
	ae_map_download_outcome_text(AE_MAP_DOWNLOAD_OUTCOME_OFFLINE, 0, "", text, sizeof(text));
	CHECK(!strcmp(text, "The server did not answer. Try again later."));
	ae_map_download_outcome_text(AE_MAP_DOWNLOAD_OUTCOME_NONE, 0, "", text, sizeof(text));
	CHECK(!text[0]);
	ae_map_download_outcome_text(AE_MAP_DOWNLOAD_OUTCOME_BUSY, 0, "", text, sizeof(text));
	CHECK(!strcmp(text, "Leave the game you are in first."));
	CHECK(ae_map_download_outcome_failed(AE_MAP_DOWNLOAD_OUTCOME_BUSY));
	CHECK(ae_map_download_outcome_failed(AE_MAP_DOWNLOAD_OUTCOME_FULL));
	CHECK(ae_map_download_outcome_failed(AE_MAP_DOWNLOAD_OUTCOME_SOME));
	CHECK(!ae_map_download_outcome_failed(AE_MAP_DOWNLOAD_OUTCOME_DONE));
	CHECK(!ae_map_download_outcome_failed(AE_MAP_DOWNLOAD_OUTCOME_CANCELLED));

	ae_map_download_progress_text("AE #1 Social", 3, 12, 48ULL * 1024 * 1024 + 5, 310ULL * 1024 * 1024 - 7, text,
		sizeof(text));
	CHECK(!strcmp(text, "GETTING MAPS FROM AE #1 SOCIAL  3 OF 12  48 / 310 MB"));
	ae_map_download_progress_text("AE #5", 0, 0, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "LOOKING FOR MAPS ON AE #5"));
	ae_map_download_progress_text("", 1, 2, 0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "GETTING MAPS FROM AE SERVERS  1 OF 2"));
}

int main(void)
{
	test_frame();
	test_room();
	test_wanted();
	test_words();
	if (failures)
	{
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ae_map_download_rules: all passed\n");
	return 0;
}
