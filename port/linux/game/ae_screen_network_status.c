/*
AE_SCREEN_NETWORK_STATUS.C

See ae_screen_network_status.h. The rows are AE's flow rows (ae_setup_flow.h), quiet status lines with their help
shown under the list; the facts are read each frame from the places that already hold them (master_list.c's counts,
p2p.c's port state, the settings, the build identity).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/build_identity.h"
#include "../src/master_list.h"
#include "../src/p2p.h"
#include "halo_port_limits.h"
#include "../src/port_config.h"
#include "ae_glue_social.h"
#include "ae_network_status_rules.h"
#include "ae_screen_network_status.h"
#include "ae_screen_tab_menu.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	NETWORK_HIT = 0x7700,
	HIT_ROWS = NETWORK_HIT + 1, HIT_PROMPTS
};
enum { ROW_FACT = AE_FLOW_ROW_OWN };
enum { STAGED_NONE, STAGED_REACHED, STAGED_UNREACHED, STAGED_UNKNOWN };

static struct
{
	struct ae_flow_screen flow;
	short player;
	int staged;
	char name[64];
	struct ae_netstat_row rows[AE_NETSTAT_ROWS];
} screen;

static char const *const helps[AE_NETSTAT_ROWS] =
{
	"The name your profile plays online under.",
	"Off: no master server, no server browser, no internet games (SETTINGS > NETWORK).",
	"The server that lists online games. It is asked while the server browser is open, so before that it reads unknown.",
	"How many servers the last answer of a master server listed.",
	"The UDP port internet play uses, and what this PC already knows of it. Nothing here tests the port.",
	"Games on another network version cannot be joined.",
	"This build.",
};

static void facts_get(struct ae_netstat_facts *facts)
{
	unsigned short port = 0;

	memset(facts, 0, sizeof(*facts));
	facts->name = screen.name;
	facts->version = build_identity_watermark();
	facts->network_version = HALO_PORT_NETWORK_VERSION;
	facts->listed = -1;
	facts->seconds_since_answer = -1;
	if (screen.staged == STAGED_REACHED)
	{
		facts->online = 1;
		facts->masters = facts->answered = facts->asked = 2;
		facts->listed = 5;
		facts->seconds_since_answer = 42;
		facts->port = 43210;
		facts->port_state = AE_NETSTAT_PORT_FORWARDED;
		facts->upnp_allowed = 1;
		return;
	}
	if (screen.staged == STAGED_UNREACHED)
	{
		facts->online = 1;
		facts->masters = facts->asked = 2;
		facts->port = 43210;
		facts->port_state = AE_NETSTAT_PORT_STRICT;
		facts->upnp_allowed = 1;
		return;
	}
	if (screen.staged == STAGED_UNKNOWN)
		return;
	facts->online = config_boolean("network.online");
	facts->brokers_only = master_list_policy() == _master_listings_brokers;
	master_list_counts(&facts->masters, &facts->answered, &facts->asked);
	master_list_status(&facts->listed, &facts->seconds_since_answer);
	facts->port_state = p2p_port_status(&port);
	facts->port = port;
	facts->upnp_allowed = config_boolean("network.allow_upnp");
}

static void rows_make(void)
{
	struct ae_flow_screen *flow = &screen.flow;
	struct ae_netstat_facts facts;
	short index;

	facts_get(&facts);
	ae_netstat_rows(&facts, screen.rows);
	for (index = 0; index < AE_NETSTAT_ROWS; index++)
	{
		snprintf(flow->value_texts[index], sizeof(flow->value_texts[index]), "%s", screen.rows[index].value);
		flow->rows[index] = (struct ae_flow_row){ ROW_FACT, -1, screen.rows[index].label, flow->value_texts[index],
			helps[index], NULL, 0, 1, 1 };
	}
	flow->count = AE_NETSTAT_ROWS;
}

static void status_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;

	(void)instance;
	ae_flow_full_density(flow);
	rows_make();
	ae_flow_title(flow, "NETWORK STATUS", "MENU");
	ae_flow_draw_rows(flow, flow->frame.rect.width * 0.64f);
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static void status_back(void)
{
	short player = screen.player;

	platform_log("ae network status: closed");
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_ui_remove(&screen);
	ae_screen_tab_menu_open(NULL, player);
}

static int status_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		status_back();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&screen.flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	default:
		return 1;
	}
}

static void status_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;

	if (pointer->right_clicks)
	{
		status_back();
		return;
	}
	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		status_handle(instance, &event);
		return;
	}
	(void)ae_flow_rows_pointer(&screen.flow, pointer);
}

static struct ae_screen_class const status_class =
{
	.name = "network status", .handle = status_handle, .draw = status_draw, .pointer = status_pointer,
};

static int status_open(short local_player, int staged)
{
	if (ae_ui_holds(&screen))
		return 0;
	memset(&screen, 0, sizeof(screen));
	screen.player = local_player;
	screen.staged = staged;
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_social_profile_name(local_player, screen.name, sizeof(screen.name)))
		screen.name[0] = 0;
	if (!ae_ui_push(&status_class, AE_OWNER_ANY, &screen))
		return 0;
	rows_make();
	platform_log("ae network status: open (%s)", staged == STAGED_REACHED ? "staged reached" :
		staged == STAGED_UNREACHED ? "staged unreached" : staged == STAGED_UNKNOWN ? "staged unknown" : "live");
	/* (what each row says, for a TEST RUN's check and the units: the log has them all) */
	{
		short index;

		for (index = 0; index < AE_NETSTAT_ROWS; index++)
			platform_log("ae network status: %s = %s", screen.rows[index].label, screen.rows[index].value);
	}
	return 1;
}

int ae_screen_network_status_open(short local_player)
{
	return status_open(local_player, STAGED_NONE);
}

int ae_screen_network_status_open_staged(short local_player, char const *state)
{
	if (!state)
		return 0;
	if (!strcmp(state, "reached"))
		return status_open(local_player, STAGED_REACHED);
	if (!strcmp(state, "unreached"))
		return status_open(local_player, STAGED_UNREACHED);
	if (!strcmp(state, "unknown"))
		return status_open(local_player, STAGED_UNKNOWN);
	return 0;
}
