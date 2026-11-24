/*
AE_CAMPAIGN_WORKSHOP_RULES.C

The custom campaigns CAMPAIGN lists. See ae_campaign_workshop_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_campaign_workshop_rules.h"

void ae_ws_shown_text(const char *text, char *shown, int size)
{
	int length = 0;
	const unsigned char *cursor;

	if (!shown || size <= 0)
		return;
	shown[0] = 0;
	for (cursor = (const unsigned char *)(text ? text : ""); *cursor && length < size - 1; cursor++)
	{
		unsigned char c = *cursor;

		/* (a UTF-8 character: '?' for its first byte, nothing for the others) */
		if (c >= 0x80 && c < 0xC0)
			continue;
		if (c >= 0xC0 || (c < 0x20 && c != '\t' && c != '\r' && c != '\n') || c == 0x7F)
			c = '?';
		else if (c == '_' || c == '\t' || c == '\r' || c == '\n')
			c = ' ';
		else if (c >= 'a' && c <= 'z')
			c = (unsigned char)(c - 32);
		if (c == ' ' && (!length || shown[length - 1] == ' '))
			continue;
		shown[length++] = (char)c;
	}
	while (length && shown[length - 1] == ' ')
		length--;
	shown[length] = 0;
}

static void say_fault(ae_ws_fault_function fault, void *context, const char *package, const char *line)
{
	if (fault)
		fault(context, package, line);
}

/* why a package is left out, or NULL: it is listed */
static const char *left_out(const struct ae_ws_package *package, char *why, size_t size)
{
	int mission, missions = package->count > AE_WS_PACKAGE_MISSIONS ? AE_WS_PACKAGE_MISSIONS : package->count;

	if (package->manifest == AE_WS_MANIFEST_NONE)
		return "it has no CampaignInfo.json";
	if (package->manifest != AE_WS_MANIFEST_OK)
	{
		snprintf(why, size, "its CampaignInfo.json could not be read (%.100s)", package->reason[0] ? package->reason : "damaged");
		return why;
	}
	if (missions <= 0)
		return "its CampaignInfo.json names no mission";
	for (mission = 0; mission < missions; mission++)
	{
		if (package->missions[mission].map == AE_WS_MAP_MISSING || !package->missions[mission].level[0])
		{
			snprintf(why, size, "mission %d's map (%s) is not installed", mission + 1,
				package->missions[mission].name[0] ? package->missions[mission].name : "unnamed");
			return why;
		}
	}
	for (mission = 0; mission < missions; mission++)
	{
		if (package->missions[mission].map == AE_WS_MAP_CAMPAIGN)
			return NULL;
	}
	return "it has no campaign map";
}

int ae_ws_campaigns_make(const struct ae_ws_package *packages, int count, struct ae_ws_campaign *campaigns, int maximum,
	ae_ws_fault_function fault, void *context)
{
	int index, made = 0, mission, missions;
	char line[AE_WS_FAULT_TEXT + 192], why[192], name[AE_WS_TEXT];

	for (index = 0; packages && campaigns && index < count; index++)
	{
		const struct ae_ws_package *package = &packages[index];
		struct ae_ws_campaign *campaign;
		const char *reason = left_out(package, why, sizeof(why));

		/* (the faults name the package as CAMPAIGN would: its title, else its folder) */
		ae_ws_shown_text(package->title, name, sizeof(name));
		if (!name[0])
			ae_ws_shown_text(package->package, name, sizeof(name));
		if (reason)
		{
			snprintf(line, sizeof(line), "%s is not listed: %s.", name, reason);
			say_fault(fault, context, package->package, line);
			continue;
		}
		if (made >= maximum)
		{
			snprintf(line, sizeof(line), "%s is not listed: CAMPAIGN lists %d campaigns.", name, maximum);
			say_fault(fault, context, package->package, line);
			continue;
		}
		campaign = &campaigns[made++];
		memset(campaign, 0, sizeof(*campaign));
		snprintf(campaign->package, sizeof(campaign->package), "%s", package->package);
		missions = package->count > AE_WS_PACKAGE_MISSIONS ? AE_WS_PACKAGE_MISSIONS : package->count;
		for (mission = 0; mission < missions; mission++)
		{
			const struct ae_ws_package_mission *from = &package->missions[mission];
			struct ae_ws_mission *to;

			if (from->map == AE_WS_MAP_MULTIPLAYER)
			{
				snprintf(line, sizeof(line), "%s: mission %d (%s) is a multiplayer map, left out.", name, mission + 1,
					from->name);
				say_fault(fault, context, package->package, line);
				continue;
			}
			if (campaign->count == AE_WS_MISSIONS)
			{
				snprintf(line, sizeof(line), "%s: only its first %d missions are listed.", name, AE_WS_MISSIONS);
				say_fault(fault, context, package->package, line);
				break;
			}
			to = &campaign->missions[campaign->count++];
			ae_ws_shown_text(from->title, to->title, sizeof(to->title));
			if (!to->title[0])
				ae_ws_shown_text(from->name, to->title, sizeof(to->title));
			snprintf(to->level, sizeof(to->level), "%s", from->level);
		}
		ae_ws_shown_text(package->title, campaign->title, sizeof(campaign->title));
		if (!campaign->title[0])
			snprintf(campaign->title, sizeof(campaign->title), "%s", campaign->missions[0].title);
		if (!campaign->title[0])
			ae_ws_shown_text(package->package, campaign->title, sizeof(campaign->title));
	}
	return made;
}
