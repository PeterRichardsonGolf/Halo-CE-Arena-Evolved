/* ae_party_client_rules.c: the party service's wire, game side (URLs, queries, the answers read) */
#include <stdio.h>
#include <string.h>

#include "ae_party_client_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int parse(const char *json, struct ae_pc_view *view)
{
	return ae_pc_view_parse(json, (int)strlen(json), view);
}

int main(void)
{
	struct ae_pc_view view;
	struct ae_pnet party;
	char url[200], text[120];

	/* the hello's text and a call's URL */
	ae_pc_hello_text("https://m.example/party", "0123456789abcdef", "ab12", text, sizeof(text));
	CHECK(!strcmp(text, "ae-party-hello-2\nhttps://m.example/party\n0123456789abcdef\nab12"));
	/* the service's address as signed */
	{
		char origin[80];

		CHECK(ae_pc_origin("HTTPS://Host/party/", origin, sizeof(origin)) && !strcmp(origin, "https://host/party"));
		CHECK(ae_pc_origin("http://127.0.0.1:18781/Party", origin, sizeof(origin)) && !strcmp(origin, "http://127.0.0.1:18781/Party"));
		CHECK(ae_pc_origin("https://M.Example:8443?x=1#y", origin, sizeof(origin)) && !strcmp(origin, "https://m.example:8443"));
		CHECK(ae_pc_origin("https://m.example//", origin, sizeof(origin)) && !strcmp(origin, "https://m.example"));
		CHECK(!ae_pc_origin("m.example/party", origin, sizeof(origin)) && !ae_pc_origin("://x", origin, sizeof(origin)));
		CHECK(!ae_pc_origin("https://", origin, sizeof(origin)) && !ae_pc_origin("https://m.example/party", origin, 10));
	}
	CHECK(ae_pc_url(url, sizeof(url), "https://m.example/party/", "poll") && !strcmp(url, "https://m.example/party/v1/poll"));
	CHECK(ae_pc_url(url, sizeof(url), "https://m.example/party", "poll") && !strcmp(url, "https://m.example/party/v1/poll"));
	CHECK(!ae_pc_url(url, 20, "https://m.example/party", "poll"));

	/* a JSON body: strings only, escaped; text that is not UTF-8 reads as '?'; a body that does not fit is left as it was */
	{
		char body[200], small[40];

		body[0] = 0;
		CHECK(ae_pc_body_add(body, sizeof(body), "name", "Chief \"&\" Co\\\xc3\xa9\n") && ae_pc_body_add(body, sizeof(body), "seats", "2"));
		CHECK(!strcmp(body, "{\"name\":\"Chief \\\"&\\\" Co\\\\\xc3\xa9\\u000a\",\"seats\":\"2\"}"));
		body[0] = 0;
		CHECK(ae_pc_body_add(body, sizeof(body), "name", "a\xff" "b\xc3") && !strcmp(body, "{\"name\":\"a?b?\"}"));
		small[0] = 0;
		CHECK(ae_pc_body_add(small, sizeof(small), "k", "v") && !ae_pc_body_add(small, sizeof(small), "key", "a long value that does not fit"));
		CHECK(!strcmp(small, "{\"k\":\"v\"}"));
		CHECK(!ae_pc_body_add(small, 8, "k", "v"));
	}

	/* an error reply */
	CHECK(parse("{\"error\":\"party_full\",\"detail\":\"THE PARTY IS FULL\",\"room\":1}", &view));
	CHECK(!view.ok && !strcmp(view.error, "party_full") && view.room == 1 && !strcmp(view.detail, "THE PARTY IS FULL"));
	ae_pc_view_summary(&view, text, sizeof(text));
	CHECK(!strcmp(text, "refused party_full: THE PARTY IS FULL"));

	/* a challenge */
	CHECK(parse("{\"nonce\":\"0123456789abcdef0123456789abcdef\",\"expires_s\":60}", &view) && view.ok && strlen(view.nonce) == 32);
	/* an error longer than the field is still an error; numbers beyond what the client holds are held to it */
	CHECK(parse("{\"error\":\"an_error_name_far_longer_than_the_field_holds\"}", &view) && !view.ok && view.error[0]);
	CHECK(parse("{\"error\":\"\"}", &view) && !view.ok);
	CHECK(parse("{\"rev\":1e300,\"room\":-1e300,\"party\":{\"id\":\"P\",\"leader\":\"L\",\"seats\":99999999999,\"members\":["
		"{\"code\":\"A\",\"seats\":2147483647},{\"code\":\"B\",\"seats\":-5},{\"code\":\"C\",\"seats\":0}]},"
		"\"invites\":[{\"party\":\"Q\",\"from\":{\"code\":\"X\"},\"seats\":1e15,\"expires_s\":1e20}]}", &view));
	CHECK(view.ok && view.rev == 1000000000 && view.room == -1000000000);
	CHECK(view.seats == 8 && view.member[0].seats == 4 && view.member[1].seats == 1 && view.member[2].seats == 1);
	CHECK(view.invite[0].seats == 8);
	ae_pc_party(&view, &party);
	CHECK(ae_pnet_seats(&party) == 6);

	/* the poll interval: the service's, held to 1-15 s, and 5 s or more while idle */
	CHECK(ae_pc_poll_ms(500, 1) == 1000 && ae_pc_poll_ms(99999, 1) == 15000 && ae_pc_poll_ms(0, 1) == 1000 && ae_pc_poll_ms(-5, 1) == 1000);
	CHECK(ae_pc_poll_ms(1000, 1) == 1000 && ae_pc_poll_ms(2500, 1) == 2500);
	CHECK(ae_pc_poll_ms(1000, 0) == 5000 && ae_pc_poll_ms(500, 0) == 5000 && ae_pc_poll_ms(8000, 0) == 8000 && ae_pc_poll_ms(99999, 0) == 15000);
	CHECK(parse("{\"sid\":\"abc\",\"config\":{\"poll_ms\":3000}}", &view) && view.poll_ms == 3000);

	/* a hello, a poll that changed nothing */
	CHECK(parse("{\"sid\":\"0123456789abcdef0123456789abcdef\",\"code\":\"CSM7-NBFR-CAYQ\",\"now\":5,\"config\":{\"poll_ms\":2000}}", &view));
	CHECK(view.ok && !strcmp(view.code, "CSM7-NBFR-CAYQ") && strlen(view.sid) == 32);
	CHECK(parse("{\"rev\":12,\"same\":true}", &view) && view.ok && view.same && view.rev == 12);

	/* a view: a party of three, an invite, a search found, a follow, a notice */
	CHECK(parse("{\"rev\":9,\"code\":\"AAAA-BBBB-CCCC\",\"party\":{\"id\":\"P1\",\"leader\":\"AAAA-BBBB-CCCC\",\"privacy\":\"friends\","
		"\"max\":8,\"seats\":4,\"link\":\"LINK12LINK12\",\"members\":["
		"{\"code\":\"XXXX-XXXX-XXXX\",\"name\":\"Rook\",\"seats\":2,\"state\":\"party\",\"build\":\"\"},"
		"{\"code\":\"AAAA-BBBB-CCCC\",\"name\":\"Chief\",\"seats\":1,\"state\":\"searching\",\"build\":\"\"},"
		"{\"code\":\"YYYY-YYYY-YYYY\",\"name\":\"Pickle \\u00e9\",\"seats\":1,\"state\":\"offline\",\"build\":\"\"}]},"
		"\"invites\":[{\"party\":\"P9\",\"from\":{\"code\":\"ZZZZ-ZZZZ-ZZZZ\",\"name\":\"Juno\"},\"seats\":3,\"expires_s\":250}],"
		"\"search\":{\"state\":\"found\",\"playlist\":\"casual\",\"n\":3,\"seats\":4,\"expires_s\":60,\"ticket\":\"TICKET\","
		"\"server\":{\"address\":\"play.example:10660\",\"name\":\"Casual\",\"players\":6,\"max\":16,\"ping_ms\":34},\"again\":true},"
		"\"follow\":{\"kind\":\"hosted\",\"address\":\"\",\"invite\":\"abcd\",\"n\":2,\"from\":\"AAAA-BBBB-CCCC\"},"
		"\"notice\":{\"text\":\"Rook joined\",\"n\":4}}", &view));
	CHECK(view.ok && !view.same && view.rev == 9 && view.has_party && !strcmp(view.party_id, "P1") && view.seats == 4);
	CHECK(view.privacy == AE_PNET_FRIENDS && !strcmp(view.link, "LINK12LINK12") && view.member_count == 3);
	CHECK(!strcmp(view.member[0].name, "Rook") && view.member[0].seats == 2 && !strcmp(view.member[1].state, "searching"));
	CHECK(!strcmp(view.member[2].name, "Pickle \xc3\xa9"));
	CHECK(view.invite_count == 1 && !strcmp(view.invite[0].from_name, "Juno") && view.invite[0].seats == 3 && view.invite[0].expires_s == 250);
	CHECK(view.search.present && !strcmp(view.search.state, "found") && view.search.n == 3 && view.search.seats == 4 && view.search.again);
	CHECK(!strcmp(view.search.server_address, "play.example:10660") && view.search.server_players == 6 && view.search.server_max == 16 &&
		view.search.ping_ms == 34 && !strcmp(view.search.ticket, "TICKET"));
	CHECK(view.follow.present && !strcmp(view.follow.kind, "hosted") && !strcmp(view.follow.invite, "abcd") && view.follow.n == 2);
	CHECK(!strcmp(view.notice, "Rook joined"));
	ae_pc_view_summary(&view, text, sizeof(text));
	CHECK(!strcmp(text, "rev 9, in a party, 1 invite, search found"));

	/* the party as the rules module holds it: the leader found by code, an offline member flagged */
	ae_pc_party(&view, &party);
	CHECK(party.count == 3 && party.leader == 1 && ae_pnet_seats(&party) == 4 && !party.member[2].online && party.member[0].online);
	CHECK(party.privacy == AE_PNET_FRIENDS && ae_pnet_can_invite(&party, "XXXX-XXXX-XXXX") == AE_PNET_OK);

	/* a view with Steam friends found */
	CHECK(parse("{\"rev\":3,\"party\":null,\"steam\":[{\"hash\":\"96d3391cf79f\",\"code\":\"AAAA-BBBB-CCCC\",\"name\":\"Rook\",\"state\":\"menus\",\"verified\":false},"
		"{\"hash\":\"e9b597766e27\",\"code\":\"DDDD-EEEE-FFFF\",\"name\":\"Pickle\",\"state\":\"party\",\"verified\":true}]}", &view));
	CHECK(view.steam_count == 2 && !strcmp(view.steam[0].hash, "96d3391cf79f") && !strcmp(view.steam[1].name, "Pickle") &&
		!view.steam[0].verified && view.steam[1].verified);

	/* a view with nothing: no party, no search, ping -1; a bot offer */
	CHECK(parse("{\"rev\":1,\"code\":\"A\",\"party\":null,\"invites\":[],\"search\":null,\"follow\":null,\"notice\":null}", &view));
	CHECK(view.ok && !view.has_party && !view.search.present && !view.follow.present && !view.notice[0] && view.search.ping_ms == -1);
	ae_pc_party(&view, &party);
	CHECK(party.count == 0);
	CHECK(parse("{\"rev\":2,\"search\":{\"state\":\"bots\",\"playlist\":\"comp\",\"n\":1,\"seats\":3,\"ask\":true,\"reason\":\"no_room\","
		"\"room_for\":2,\"partial\":null}}", &view));
	CHECK(view.search.present && !strcmp(view.search.state, "bots") && view.search.ask && view.search.room_for == 2 &&
		!strcmp(view.search.reason, "no_room"));

	/* garbage: not JSON, not an object, empty, no text, too deep */
	CHECK(!parse("not json", &view) && !view.ok);
	CHECK(!parse("[1,2]", &view));
	CHECK(!parse("", &view));
	CHECK(!ae_pc_view_parse(NULL, 5, &view));
	CHECK(!parse("{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":1}}}}}}}}}}}}}", &view));
	/* more members than a party holds, a long name: cut, never past the record */
	CHECK(parse("{\"party\":{\"members\":[{\"code\":\"A\",\"name\":\"0123456789012345678901234567890123456789\",\"seats\":1},{},{},{},{},{},{},{},{},{}]}}", &view));
	CHECK(view.member_count <= AE_PNET_MAX && view.member[0].name[0] == 0);

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
