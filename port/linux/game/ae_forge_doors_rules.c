/*
AE_FORGE_DOORS_RULES.C

See ae_forge_doors_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_forge_doors_rules.h"

int ae_forge_doors_pack_state(int ready, int writing, int failed, int maps_wanted)
{
	if (ready)
		return AE_FORGE_PACK_READY;
	if (writing)
		return AE_FORGE_PACK_BUILDING;
	return failed || maps_wanted <= 0 ? AE_FORGE_PACK_NEEDS_MAPS : AE_FORGE_PACK_BUILDING;
}

int ae_forge_doors_percent(int done, int wanted)
{
	if (wanted <= 0 || done <= 0)
		return 0;
	return done >= wanted ? 100 : (int)((long)done * 100 / wanted);
}

void ae_forge_doors_pack_reason(int state, int percent, char *text, size_t size)
{
	if (!size)
		return;
	if (state == AE_FORGE_PACK_BUILDING)
		snprintf(text, size, "GETTING READY %d %%", percent < 0 ? 0 : percent > 100 ? 100 : percent);
	else if (state == AE_FORGE_PACK_NEEDS_MAPS)
		snprintf(text, size, "NEEDS THE GAME'S MAPS");
	else
		text[0] = 0;
}

void ae_forge_doors_pack_progress(int done, int wanted, char *text, size_t size)
{
	if (!size)
		return;
	if (wanted <= 0)
		text[0] = 0;
	else
		snprintf(text, size, "%d OF %d MAPS READ", done < 0 ? 0 : done > wanted ? wanted : done, wanted);
}

int ae_forge_doors_row_needs_pack(int row)
{
	return row == AE_FORGE_HUB_CONTINUE || row == AE_FORGE_HUB_NEW_MAP;
}

void ae_forge_doors_map_title(char const *path, char *text, size_t size)
{
	char const *leaf;
	size_t length, each;

	if (!size)
		return;
	text[0] = 0;
	if (!path)
		return;
	leaf = path;
	for (each = 0; path[each]; each++)
		if (path[each] == '\\' || path[each] == '/')
			leaf = path + each + 1;
	length = strcspn(leaf, "@");
	if (length >= size)
		length = size - 1;
	for (each = 0; each < length; each++)
		text[each] = leaf[each] >= 'a' && leaf[each] <= 'z' ? (char)(leaf[each] - 32) : leaf[each];
	text[length] = 0;
}

/* ---------- FORGE KEYS */

static char const *const labels[AE_FORGE_KEYS_ACTIONS] =
{
	"FORGE / SPARTAN", "OBJECT PALETTE", "3RD-PERSON VIEW", "QUICK SAVE MAP", "ROTATE ITEM", "SNAP TO FLOOR",
	"SNAP TO SURFACE", "ROTATION SNAP", "OBJECT OPTIONS", "DELETE OBJECT", "COPY OBJECT", "OBJECT SETTINGS"
};

static char const *const helps[AE_FORGE_KEYS_ACTIONS] =
{
	"Switches between flying as the Monitor and playing as a Spartan.",
	"Opens the object palette while you fly as the Monitor.",
	"Shows the Monitor's body from behind.",
	"Saves the map under the name it has.",
	"Held with a held item: the mouse or the stick turns it.",
	"Sets a held item down on the floor below it.",
	"Turns a held item to the surface it points at.",
	"Steps the rotation snap: off, 15, 45 and 90 degrees.",
	"Opens the options of the object you hold.",
	"Deletes the object you hold, once the button is held.",
	"Copies the object under the reticle when you hold nothing.",
	"Opens the settings of what the reticle is on, without picking it up."
};

char const *ae_forge_keys_label(int action)
{
	return action >= 0 && action < AE_FORGE_KEYS_ACTIONS ? labels[action] : NULL;
}

char const *ae_forge_keys_help(int action)
{
	return action >= 0 && action < AE_FORGE_KEYS_ACTIONS ? helps[action] : NULL;
}

int ae_forge_keys_vk_of_scancode(int scancode)
{
	if (scancode >= 4 && scancode <= 29)
		return 'A' + (scancode - 4);
	if (scancode >= 30 && scancode <= 38)
		return '1' + (scancode - 30);
	if (scancode == 39)
		return '0';
	if (scancode >= 58 && scancode <= 69)
		return 0x70 + (scancode - 58);
	if (scancode >= 89 && scancode <= 97)
		return 0x61 + (scancode - 89);
	switch (scancode)
	{
	case 98: return 0x60;
	case 43: return 0x09;
	case 40: return 0x0D;
	case 44: return 0x20;
	case 224: return 0xA2;
	case 225: return 0xA0;
	case 226: return 0xA4;
	case 228: return 0xA3;
	case 229: return 0xA1;
	case 230: return 0xA5;
	case 75: return 0x21;
	case 78: return 0x22;
	case 77: return 0x23;
	case 74: return 0x24;
	case 80: return 0x25;
	case 82: return 0x26;
	case 79: return 0x27;
	case 81: return 0x28;
	case 73: return 0x2D;
	case 85: return 0x6A;
	case 87: return 0x6B;
	case 86: return 0x6D;
	case 99: return 0x6E;
	case 84: return 0x6F;
	case 51: return 0xBA;
	case 46: return 0xBB;
	case 54: return 0xBC;
	case 45: return 0xBD;
	case 55: return 0xBE;
	case 56: return 0xBF;
	case 53: return 0xC0;
	case 47: return 0xDB;
	case 49: return 0xDC;
	case 48: return 0xDD;
	case 52: return 0xDE;
	}
	return 0;
}

int ae_forge_keys_vk_of_mouse(int button)
{
	return button == 1 ? 0x01 : button == 3 ? 0x02 : button == 2 ? 0x04 : 0;
}

int ae_forge_keys_gp_of_button(int button)
{
	return button >= 0 && button < 16 ? button + 1 : 0;
}

float ae_forge_keys_pad_step(float value, int direction)
{
	int steps = (int)(value * 4.0f + 0.5f);

	steps += direction < 0 ? -1 : direction > 0 ? 1 : 0;
	return (float)(steps < 1 ? 1 : steps > 10 ? 10 : steps) * 0.25f;
}

int ae_forge_keys_pad_percent(float value)
{
	return (int)(value * 100.0f + 0.5f);
}

void ae_forge_keys_taken_text(char const *shown, char const *from_label, char *text, size_t size)
{
	snprintf(text, size, "%s moved here from %s, which has none now.", shown, from_label);
}
