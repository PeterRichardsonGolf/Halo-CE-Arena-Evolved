#include <stdio.h>
#include <wchar.h>

#include "ae_pack_names_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* Only a pack weapon on its own HUD with no messaging icon shows its name. */
static void test_no_picture(void)
{
	CHECK(ae_pack_names_no_picture(1, 1, -1));
	CHECK(!ae_pack_names_no_picture(1, 1, 0));	/* it has a picture */
	CHECK(!ae_pack_names_no_picture(1, 0, -1));	/* the map's HUD: the map's picture */
	CHECK(!ae_pack_names_no_picture(0, 1, -1));	/* a map's own weapon */
}

/* The name in capitals, cut to the room, always ended; no name is an empty word. */
static void test_word(void)
{
	wchar_t word[32];

	CHECK(ae_pack_names_word("Grenade Launcher", word, 32) == 16 && !wcscmp(word, L"GRENADE LAUNCHER"));
	CHECK(ae_pack_names_word("GRENADE LAUNCHER", word, 8) == 7 && !wcscmp(word, L"GRENADE"));
	CHECK(ae_pack_names_word("caf\xc3\xa9", word, 32) == 5 && !wcscmp(word, L"CAF??"));
	CHECK(ae_pack_names_word(NULL, word, 32) == 0 && word[0] == 0);
	CHECK(ae_pack_names_word("x", word, 1) == 0 && word[0] == 0);
	CHECK(ae_pack_names_word("x", NULL, 4) == 0);
}

#define ALWAYS (1 << AE_PACK_NAMES_OVERLAY_ALWAYS_BIT)

/* Only the HUD's first always-shown overlay item is the picture: a second element, a second item and a state's own
overlay (reload, empty) stay the game's. */
static void test_picture(void)
{
	short first[] = { ALWAYS, ALWAYS | 1 };
	short second[] = { 1 << 2, ALWAYS };
	short none[] = { 1, 1 << 2 };
	short const *both[] = { first, second };
	short const *later[] = { none, second };
	short const *neither[] = { none, none };
	int counts[] = { 2, 2 };
	int element, item;

	/* (a no-picture pack whose HUD has a second overlay element: only element 0's item 0 is replaced) */
	CHECK(ae_pack_names_picture(both, counts, 2, &element, &item) && element == 0 && item == 0);
	CHECK(ae_pack_names_picture(later, counts, 2, &element, &item) && element == 1 && item == 1);
	CHECK(!ae_pack_names_picture(neither, counts, 2, &element, &item) && element == -1 && item == -1);
	CHECK(!ae_pack_names_picture(both, counts, 0, &element, &item) && element == -1);
	CHECK(!ae_pack_names_picture(NULL, counts, 2, &element, &item));
}

int main(void)
{
	test_no_picture();
	test_word();
	test_picture();
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
