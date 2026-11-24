/*
AE_BACKPACK_RULES.C

BACKPACK WEAPONS' rules (ae_backpack_rules.h): a holstered weapon's kind and
place. No game dependency, so the unit test builds it alone.
*/

#include <string.h>

#include "ae_backpack_rules.h"

/* words in a weapon's tag path that make it small (a holster's), or very
long */
static char const *const ae_backpack_small_words[] =
{
	"pistol", "magnum", "revolver", "smg", "needler", "plasma rifle", "plasma_rifle",
};
static char const *const ae_backpack_long_words[] =
{
	"sniper",
};

/* the models fitted to the back, by a part of their tag path (lower case): the gravity hammer pack's model holds
its shaft 32.2 degrees off its up axis, the head toward its right, so it is turned onto it; it is moved 5 cm out
from the back and leaned 15 degrees toward upright, so the stock cyborg's back plate, shoulder pads and helmet stay
clear of its shaft and pommel */
/* the fuel rod's model, in the space it is drawn in (its barrel forward, its top up, as a rifle's): a tube 0.41 units
long (0.25 behind its origin, 0.16 ahead) that lies 0.05 to 0.16 above its origin, a grip below it down to 0.08 under
the origin, and 0.06 thick from side to side. So it lies across the back as a rifle does, on its place's barrel and
its place's top, its thin way against the back plate: nothing of it is turned. Only its tube is far from its origin
(0.1 toward its top, which on the back is down to the right), so the whole model is moved to the left across the
back and a little down, and the tube lies on the spine from the right shoulder to the left hip, its back end below
the helmet; and it is moved out by its extra thickness over a rifle's, so its side lies on the upper back plate
(sized for the stock cyborg: the back plate 0.1 behind the spine, the back 0.3 wide and 0.2 from pelvis to neck) */
#define AE_BACKPACK_FUEL_ROD_OUT 0.035f
#define AE_BACKPACK_FUEL_ROD_SIDE 0.14f
#define AE_BACKPACK_FUEL_ROD_UP -0.03f

/* the flamethrower's model (its node rests turned 55 degrees about its side) is about 0.45 units along its barrel,
nearly all of it ahead of its origin: across the back as a rifle lies, it ran from the shoulder blade to below the
hip, as its node's turn (in the plane of the back) stood it on end. Its barrel is set 55 degrees back the other way, so
it lies on the back as a rifle does, moved back along its barrel (up to the right) so it is centred on the back, and
out by the part of it that would be inside the back plate */
#define AE_BACKPACK_FLAMETHROWER_OUT 0.04f
#define AE_BACKPACK_FLAMETHROWER_UP 0.12f
#define AE_BACKPACK_FLAMETHROWER_SIDE -0.12f

static struct
{
	char const *part;
	struct ae_backpack_fit fit;
} const ae_backpack_fits[] =
{
	{ "ae\\packs\\gravity_hammer\\", { -32.2f, 0.05f, 15.f, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, 0.f, 0.f, 0.f } },
	/* (across the back as a rifle lies: its place's barrel and top, moved left onto the spine, a little down, and out) */
	{ "weapons\\plasma_cannon\\", { 0.f, AE_BACKPACK_FUEL_ROD_OUT, 0.f, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f },
		AE_BACKPACK_FUEL_ROD_UP, AE_BACKPACK_FUEL_ROD_SIDE, 0.f } },
	/* (its barrel 10 degrees above the body's left, its top 80 below it: with its node's 55 degree turn, the barrel goes
	down to the left hip as a rifle's does) */
	{ "weapons\\flamethrower\\", { 0.f, AE_BACKPACK_FLAMETHROWER_OUT, 0.f, { 0.f, 0.985f, 0.174f }, { 0.f, 0.174f, -0.985f },
		AE_BACKPACK_FLAMETHROWER_UP, AE_BACKPACK_FLAMETHROWER_SIDE, 0.f } },
};

/* whether text has part in it, whatever its case */
static int ae_backpack_has(
	char const *text,
	char const *part)
{
	char lower[256];
	size_t index;

	for (index = 0; index < sizeof(lower) - 1 && text[index]; index++)
		lower[index] = (char)(text[index] >= 'A' && text[index] <= 'Z' ? text[index] - 'A' + 'a' : text[index]);
	lower[index] = 0;
	return strstr(lower, part) != NULL;
}

int ae_backpack_rules_kind(
	char const *tag_path,
	float bounding_radius)
{
	size_t index;

	for (index = 0; tag_path && index < sizeof(ae_backpack_small_words) / sizeof(ae_backpack_small_words[0]); index++)
	{
		if (ae_backpack_has(tag_path, ae_backpack_small_words[index]))
			return AE_BACKPACK_KIND_THIGH;
	}
	for (index = 0; tag_path && index < sizeof(ae_backpack_long_words) / sizeof(ae_backpack_long_words[0]); index++)
	{
		if (ae_backpack_has(tag_path, ae_backpack_long_words[index]))
			return AE_BACKPACK_KIND_LONG;
	}
	return bounding_radius < AE_BACKPACK_SMALL_RADIUS ? AE_BACKPACK_KIND_THIGH : AE_BACKPACK_KIND_BACK;
}

int ae_backpack_rules_place(
	int kind,
	int *on_back,
	int *on_thighs)
{
	if (kind == AE_BACKPACK_KIND_THIGH)
	{
		if (*on_thighs > 1)
			return AE_BACKPACK_PLACE_NONE;
		return (*on_thighs)++ == 0 ? AE_BACKPACK_PLACE_THIGH : AE_BACKPACK_PLACE_THIGH_SECOND;
	}
	if (*on_back > 1)
		return AE_BACKPACK_PLACE_NONE;
	if ((*on_back)++ > 0)
		return AE_BACKPACK_PLACE_BACK_SECOND;
	return kind == AE_BACKPACK_KIND_LONG ? AE_BACKPACK_PLACE_LONG : AE_BACKPACK_PLACE_BACK;
}

void ae_backpack_rules_fit(
	char const *tag_path,
	struct ae_backpack_fit *fit)
{
	size_t index;

	fit->turn_degrees = 0.f;
	fit->out = 0.f;
	fit->lean_degrees = 0.f;
	memset(fit->barrel, 0, sizeof(fit->barrel));
	memset(fit->top, 0, sizeof(fit->top));
	fit->up = 0.f;
	fit->side = 0.f;
	fit->along = 0.f;
	for (index = 0; tag_path && index < sizeof(ae_backpack_fits) / sizeof(ae_backpack_fits[0]); index++)
	{
		if (ae_backpack_has(tag_path, ae_backpack_fits[index].part))
		{
			*fit = ae_backpack_fits[index].fit;
			return;
		}
	}
}
