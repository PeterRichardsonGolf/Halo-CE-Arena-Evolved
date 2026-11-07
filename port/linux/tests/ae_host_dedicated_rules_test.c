/* ae_host_dedicated_rules_test.c: HOST A SERVER's pure rules (ae_host_dedicated_rules.h) */
#include <stdio.h>
#include <string.h>

#include "ae_host_dedicated_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_config_private_default(void)
{
	struct ae_hd_choices choices;
	char text[8192];
	int length;

	ae_host_dedicated_choices_init(&choices, "Goat's \"Server\"\x01 with a long name");
	CHECK(!strcmp(choices.name, "Goat's \"Server\""));
	snprintf(choices.rotation, sizeof(choices.rotation), "bloodgulch:ctf, sali_creek:file=AE TEAM SLY");
	snprintf(choices.status_file, sizeof(choices.status_file), "/srv/save/server/status.json");
	snprintf(choices.mcc_path, sizeof(choices.mcc_path), "C:\\Steam\\MCC");
	length = ae_host_dedicated_config_text(&choices, text, sizeof(text));
	CHECK(length > 0 && length == (int)strlen(text));
	CHECK(strstr(text, "[server]\n") != NULL);
	CHECK(strstr(text, "public = false\n") != NULL);
	CHECK(strstr(text, "web_bind = \"127.0.0.1\"\n") != NULL);
	CHECK(strstr(text, "name = \"Goat's \\\"Server\\\"\"\n") != NULL);
	CHECK(strstr(text, "bots_fill = \"minimum\"\nmin_players = 8\n") != NULL);
	CHECK(strstr(text, "rotation = \"bloodgulch:ctf, sali_creek:file=AE TEAM SLY\"\n") != NULL);
	CHECK(strstr(text, "tunnel_port = 2303\n") != NULL && strstr(text, "web_port = 8080\n") != NULL);
	CHECK(strstr(text, "mcc_path = \"C:\\\\Steam\\\\MCC\"\n") != NULL);
	CHECK(strstr(text, "console_exit_on_eof = false\n") != NULL);
	CHECK(strstr(text, "auto_update = \"off\"\n") != NULL);
	/* (bots off, PUBLIC, no MCC: no [game]) */
	choices.bots = 0;
	choices.public_listing = 1;
	choices.mcc_path[0] = 0;
	ae_host_dedicated_config_text(&choices, text, sizeof(text));
	CHECK(strstr(text, "public = true\n") != NULL && strstr(text, "bots_fill = \"off\"\nmin_players = 1\n") != NULL);
	CHECK(strstr(text, "[game]") == NULL);
	CHECK(ae_host_dedicated_config_text(&choices, text, 40) == -1);
}

static int taken[4] = { 2303, 2304, 8080, 0 };

static int free_port(int port, int tcp, void *context)
{
	int index;

	(void)context;
	(void)tcp;
	for (index = 0; taken[index]; index++)
		if (taken[index] == port)
			return 0;
	return 1;
}

static int none_free(int port, int tcp, void *context)
{
	(void)port;
	(void)tcp;
	(void)context;
	return 0;
}

static void test_ports_skip_taken(void)
{
	int tunnel, web;

	CHECK(ae_host_dedicated_ports(free_port, NULL, &tunnel, &web) && tunnel == 2305 && web == 8081);
	CHECK(!ae_host_dedicated_ports(none_free, NULL, &tunnel, &web) && !tunnel && !web);
}

static void test_step_ready_and_timeout(void)
{
	CHECK(ae_host_dedicated_step(AE_HD_STARTING, 0, 1, 500) == AE_HD_STARTING);
	CHECK(ae_host_dedicated_step(AE_HD_STARTING, 1, 1, 500) == AE_HD_READY);
	CHECK(ae_host_dedicated_step(AE_HD_STARTING, 0, 0, 500) == AE_HD_FAILED);
	CHECK(ae_host_dedicated_step(AE_HD_STARTING, 0, 1, AE_HD_START_TIMEOUT_MS) == AE_HD_FAILED);
	CHECK(ae_host_dedicated_step(AE_HD_READY, 1, 0, 0) == AE_HD_IDLE);
	CHECK(ae_host_dedicated_step(AE_HD_STOPPING, 0, 1, 100) == AE_HD_STOPPING);
	CHECK(ae_host_dedicated_step(AE_HD_STOPPING, 0, 0, 100) == AE_HD_IDLE);
	CHECK(ae_host_dedicated_step(AE_HD_IDLE, 1, 1, 0) == AE_HD_IDLE);
}

static void test_running_state_reused(void)
{
	struct ae_hd_running running, read;
	char text[256];

	memset(&running, 0, sizeof(running));
	running.pid = 4242;
	running.tunnel_port = 2303;
	running.web_port = 8081;
	running.started = 1762400000UL;
	running.stamp = 987654321ULL;
	snprintf(running.name, sizeof(running.name), "Friday Night");
	CHECK(ae_host_dedicated_running_text(&running, text, sizeof(text)) > 0);
	CHECK(ae_host_dedicated_running_read(text, &read) && read.pid == 4242 && read.tunnel_port == 2303 &&
		read.web_port == 8081 && read.started == 1762400000UL && read.stamp == 987654321ULL && !strcmp(read.name, "Friday Night"));
	/* (Review Focus 3: a live pid: JOIN / STOP, never a second server; a dead one: forgotten) */
	CHECK(ae_host_dedicated_running_reused(&read, 1, 987654321ULL));
	CHECK(!ae_host_dedicated_running_reused(&read, 0, 987654321ULL));
	/* (a reboot hands the pid to another process: a different start time, or none readable, is not our server, and an
	EPERM "alive" proves nothing; a state with no stamp is never adopted) */
	CHECK(!ae_host_dedicated_running_reused(&read, 1, 123ULL) && !ae_host_dedicated_running_reused(&read, 1, 0));
	CHECK(!ae_host_dedicated_running_identity(&read, 123ULL) && ae_host_dedicated_running_identity(&read, 987654321ULL));
	read.stamp = 0;
	CHECK(!ae_host_dedicated_running_reused(&read, 1, 0) && !ae_host_dedicated_running_identity(&read, 5ULL));
	CHECK(!ae_host_dedicated_running_read("pid=1\ntunnel=2303\nweb=8080\n", &read));
	CHECK(!ae_host_dedicated_running_read("pid=500\nweb=8080\n", &read));
	CHECK(!ae_host_dedicated_running_read("pid=500\ntunnel=99999\nweb=8080\n", &read));
	CHECK(!ae_host_dedicated_running_read("", &read) && !ae_host_dedicated_running_read(NULL, &read));
}

static void test_status(void)
{
	struct ae_hd_status status;
	char const *json = "{\"ok\":true,\"time\":1,\"name\":\"Friday \\\"Night\\\"\",\"state\":\"lobby\",\"uptime\":125,"
		"\"map\":\"bloodgulch\",\"gametype\":\"ctf\",\"players\":3,\"max_players\":16,"
		"\"listing\":{\"online\":true,\"public\":false,\"invite\":\"halo://join/abcd\"},\"rotation\":{\"index\":0}}";

	CHECK(ae_host_dedicated_status_read(json, &status));
	CHECK(!strcmp(status.state, "lobby") && !strcmp(status.name, "Friday \"Night\"") && status.uptime == 125);
	CHECK(!strcmp(status.map, "bloodgulch") && status.players == 3 && status.max_players == 16);
	CHECK(!strcmp(status.invite, "halo://join/abcd"));
	CHECK(ae_host_dedicated_status_ready(&status));
	CHECK(ae_host_dedicated_status_read("{\"state\":\"starting\"}", &status) && !ae_host_dedicated_status_ready(&status));
	CHECK(!ae_host_dedicated_status_read("{\"ok\":true}", &status));
	CHECK(!ae_host_dedicated_status_read(NULL, &status));
	/* (a cut file reads what it has) */
	CHECK(ae_host_dedicated_status_read("{\"state\":\"pla", &status) && !strcmp(status.state, "pla"));
}

static void test_rows(void)
{
	char text[32], line[256];
	int choices[3];

	CHECK(ae_host_dedicated_bots_step(8, 1) == 12 && ae_host_dedicated_bots_step(0, -1) == 0);
	CHECK(ae_host_dedicated_bots_step(16, 1) == 16 && ae_host_dedicated_bots_step(2, -1) == 0);
	ae_host_dedicated_bots_word(0, text, sizeof(text));
	CHECK(!strcmp(text, "OFF"));
	ae_host_dedicated_bots_word(8, text, sizeof(text));
	CHECK(!strcmp(text, "UP TO 8"));
	CHECK(ae_host_dedicated_players_step(16, 1) == 16 && ae_host_dedicated_players_step(2, -1) == 2);
	CHECK(ae_host_dedicated_rotation_line("# playlist: 4V4\n# map vote: 3\nsali_creek:file=AE TEAM SLY  # weight 3\n"
		"\r\n  sanctuary:file=AE KING\nbad,entry\n", line, sizeof(line)) == 2);
	CHECK(!strcmp(line, "sali_creek:file=AE TEAM SLY, sanctuary:file=AE KING"));
	CHECK(ae_host_dedicated_rotation_line("# nothing\n", line, sizeof(line)) == 0 && !line[0]);
	CHECK(ae_host_dedicated_quit_choices(0, choices) == 2 && choices[0] == AE_HD_QUIT_KEEP && choices[1] == AE_HD_QUIT_STOP);
	CHECK(ae_host_dedicated_quit_choices(1, choices) == 3 && choices[1] == AE_HD_QUIT_STOP_EMPTY);
	CHECK(!strcmp(ae_host_dedicated_quit_label(AE_HD_QUIT_STOP_EMPTY), "STOP WHEN EMPTY"));
	CHECK(ae_host_dedicated_link_wanted("maps", 1) && ae_host_dedicated_link_wanted("packs", 1));
	CHECK(!ae_host_dedicated_link_wanted("debug.txt", 0) && !ae_host_dedicated_link_wanted("server", 1));
	CHECK(!ae_host_dedicated_link_wanted(".hidden", 1) && !ae_host_dedicated_link_wanted("screenshots", 1));
}

int main(void)
{
	test_config_private_default();
	test_ports_skip_taken();
	test_step_ready_and_timeout();
	test_running_state_reused();
	test_status();
	test_rows();
	if (failures)
		return 1;
	printf("ae_host_dedicated_rules: ok\n");
	return 0;
}
