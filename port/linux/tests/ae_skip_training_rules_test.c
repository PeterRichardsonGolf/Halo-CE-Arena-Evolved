#include <stdio.h>
#include <string.h>
#include "ae_skip_training_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int difficulty;

	/* on, normal, the level's startup script: told heroic */
	CHECK(ae_skip_training_difficulty(1, 1, "a10", "mission_a10") == 2);
	/* the level named by its path */
	CHECK(ae_skip_training_difficulty(1, 1, "levels\\a10\\a10", "mission_a10") == 2);
	CHECK(ae_skip_training_difficulty(1, 1, "levels/a10/a10", "mission_a10") == 2);
	CHECK(ae_skip_training_difficulty(1, 1, "levels\\a10\\a10x", "mission_a10") == 1);
	CHECK(ae_skip_training_difficulty(1, 1, "levels\\a30\\a30", "mission_a10") == 1);
	/* off: the real one, whatever the script */
	for (difficulty = 0; difficulty < 4; difficulty++)
		CHECK(ae_skip_training_difficulty(0, difficulty, "a10", "mission_a10") == difficulty);
	/* on, a difficulty other than normal: unchanged (the level's own short start already) */
	CHECK(ae_skip_training_difficulty(1, 0, "a10", "mission_a10") == 0);
	CHECK(ae_skip_training_difficulty(1, 2, "a10", "mission_a10") == 2);
	CHECK(ae_skip_training_difficulty(1, 3, "a10", "mission_a10") == 3);
	/* another script of the level, another level, no names: the real one */
	CHECK(ae_skip_training_difficulty(1, 1, "a10", "cryo_search") == 1);
	CHECK(ae_skip_training_difficulty(1, 1, "a10", "[console command]") == 1);
	CHECK(ae_skip_training_difficulty(1, 1, "a30", "mission_a10") == 1);
	CHECK(ae_skip_training_difficulty(1, 1, "", "mission_a10") == 1);
	CHECK(ae_skip_training_difficulty(1, 1, NULL, "mission_a10") == 1);
	CHECK(ae_skip_training_difficulty(1, 1, "a10", NULL) == 1);

	if (!failures)
		printf("ae_skip_training_rules: ok\n");
	return failures != 0;
}
