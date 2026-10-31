/*
AE_OPENCE_SERVER_STUBS.C

ChupathingyCE's dedicated server (this program, server/) and OpenCE's (halo --server: port/linux/src/server_mode.c,
server_http.c, server_master.c, server_console.c, server_events.c), merged into Arena Evolved 0.5.0 together. The
game's code calls OpenCE's (port/linux/game/server_director.c, server_commands.c, errors.c), whose units this
program leaves out (tools/server_build.py's SERVER_EXCLUDED: they clash with its own console and roles and want SDL's
threads): here they are as a game that is not OpenCE's dedicated server sees them. halo_server_mode is 0, so the
game's OpenCE paths stay off; this program runs as ChupathingyCE's server always has.
*/

#include <stddef.h>

struct server_json;

int halo_server_mode(void) { return 0; }
int halo_server_debug_echo(void) { return 0; }

int server_console_capture_begin(void) { return 0; }
char *server_console_capture_end(void) { return NULL; }
void server_console_frame(void) {}
void server_console_free(char *text) { (void)text; }
int server_console_line_is_secret(const char *line) { (void)line; return 0; }
int server_console_next(char *line, int size, int *origin)
{
	(void)line; (void)size; (void)origin;
	return 0;
}
void server_console_output(const char *text) { (void)text; }

unsigned long server_event_add(const char *group, const char *object) { (void)group; (void)object; return 0; }
unsigned long server_event_add_addressed(const char *group, const char *object, const char *addressed_object)
{
	(void)group; (void)object; (void)addressed_object;
	return 0;
}

void server_http_capabilities(struct server_json *json, const char *key) { (void)json; (void)key; }
void server_http_main_thread(void) {}

void server_master_changed(void) {}
void server_master_identity(char *instance, int instance_size, char *key_id, int key_id_size)
{
	if (instance && instance_size > 0)
		instance[0] = 0;
	if (key_id && key_id_size > 0)
		key_id[0] = 0;
}
int server_master_probe(const unsigned char *data, int size, unsigned long address, unsigned char *answer,
	int *answer_size)
{
	(void)data; (void)size; (void)address; (void)answer;
	if (answer_size)
		*answer_size = 0;
	return 0;
}
int server_master_publisher(unsigned char *public_key, char *key_id) { (void)public_key; (void)key_id; return 0; }
void server_master_set_state(const char *members, int changed) { (void)members; (void)changed; }
void server_master_status(struct server_json *json) { (void)json; }
void server_master_stop(void) {}

const char *server_mode_build(void) { return ""; }
void server_mode_exit(int code) { (void)code; }
void server_mode_fatal(const char *text) { (void)text; }
void server_mode_frame_times(double *longest, double *mean)
{
	if (longest)
		*longest = 0.0;
	if (mean)
		*mean = 0.0;
}
void server_mode_pace_frame(void) {}
unsigned long server_mode_process_id(void) { return 0; }
void server_mode_set_idle(int idle) { (void)idle; }
int server_mode_signals(int *quit_signal)
{
	if (quit_signal)
		*quit_signal = 0;
	return 0;
}
void server_mode_start(void) {}
unsigned long server_mode_started(void) { return 0; }
double server_mode_uptime(void) { return 0.0; }

int updater_server_check(void) { return 0; }
long updater_server_available(void) { return 0; }
const char *updater_server_available_version(void) { return ""; }
int updater_server_install_start(void) { return 0; }
int updater_server_install_poll(char *error, int size)
{
	if (error && size > 0)
		error[0] = 0;
	return 0;
}

/* (AE) JOIN BY ADDRESS's name lookup (port/linux/src/ae_resolve.c, left out: a player's menu, SDL's threads): never
started on the server */
int ae_resolve_start(const char *host) { (void)host; return 0; }
int ae_resolve_poll(unsigned long *address) { (void)address; return -1; }
void ae_resolve_cancel(void) {}
