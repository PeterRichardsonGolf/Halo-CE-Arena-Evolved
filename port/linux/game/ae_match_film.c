/*
AE_MATCH_FILM.C

The match film's file (ae_match_film.h): open, write, close, and the oldest films removed over the budget.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/port_config.h"
#include "../src/posix.h"

#include "ae_match_film.h"
#include "ae_match_film_rules.h"

/* (stdio.h's fopen takes Xbox paths under the data root, as the game's code uses it: a film's path is a host path in the
config folder, so this one is the C library's; remove too, for the old films) */
#undef fopen
#undef remove

void platform_log(char const *format, ...);

/* a film name slot (the names are at most this long: film_name) */
#define AE_FILM_NAME 160
/* the first line's words, and the game's start line, as long as the objective log's lines are */
#define AE_FILM_LINE 800

static int mode_value = -1;
static FILE *film;
static boolean film_finished;
static char film_folder[400];
static char film_name[AE_FILM_NAME];
static char film_map[96], film_engine[24], film_header_rest[200];
static char film_start_line[AE_FILM_LINE];
static boolean exit_registered;

int ae_match_film_mode(void)
{
	if (mode_value < 0)
		mode_value = ae_match_film_rules_mode(config_string("debug.match_film"));
	return mode_value;
}

boolean ae_match_film_is_open(void)
{
	return film != NULL;
}

boolean ae_match_film_finished(void)
{
	return film_finished;
}

/* qsort's compare of two film name slots: oldest first, by the stamp in the name */
static int films_compare(void const *a, void const *b)
{
	return ae_match_film_name_compare((char const *)a, (char const *)b);
}

/* the films beyond the budget removed, the oldest first (by the stamp in their names), never the one being written */
static void films_trim(void)
{
	char name[256], path[512];
	char (*names)[AE_FILM_NAME] = NULL;
	long long *sizes;
	int *picked;
	void *directory = posix_directory_open(film_folder);
	int count = 0, capacity = 0, index, picks, removed = 0, current = -1;
	long budget_mb = ae_match_film_budget_mb(config_integer("debug.match_film_budget_mb"));
	long long budget = (long long)budget_mb * 1024LL * 1024LL;

	if (!directory)
		return;
	/* (every film's name: the folder's order is not the age's) */
	while (posix_directory_next(directory, name, sizeof name))
	{
		if (!ae_match_film_name_is_film(name) || strlen(name) >= AE_FILM_NAME)
			continue;
		if (count == capacity)
		{
			char (*grown)[AE_FILM_NAME];

			capacity = capacity ? capacity * 2 : 64;
			grown = realloc(names, (size_t)capacity * sizeof names[0]);
			if (!grown)
				break;
			names = grown;
		}
		snprintf(names[count], AE_FILM_NAME, "%s", name);
		count++;
	}
	posix_directory_close(directory);
	sizes = count ? malloc((size_t)count * sizeof sizes[0]) : NULL;
	picked = count ? malloc((size_t)count * sizeof picked[0]) : NULL;
	if (!sizes || !picked)
	{
		free(names);
		free(sizes);
		free(picked);
		return;
	}
	qsort(names, (size_t)count, sizeof names[0], films_compare);
	for (index = 0; index < count; index++)
	{
		struct posix_file_information information;

		sizes[index] = 0;
		if (snprintf(path, sizeof path, "%s/%s", film_folder, names[index]) < (int)sizeof path && !posix_stat(path, &information))
			sizes[index] = (long long)information.size_high * 4294967296LL + (long long)information.size_low;
		if (!strcmp(names[index], film_name))
			current = index;
	}
	picks = ae_match_film_pick_removals(sizes, count, budget, current, picked);
	for (index = 0; index < picks; index++)
	{
		if (snprintf(path, sizeof path, "%s/%s", film_folder, names[picked[index]]) < (int)sizeof path && !remove(path))
			removed++;
	}
	if (picks)
		platform_log("match film: %d old films removed (the folder is over %ld MB)%s", removed, budget_mb,
			removed < picks ? ", some could not be" : "");
	free(names);
	free(sizes);
	free(picked);
}

static void film_exit(void)
{
	if (film)
	{
		fclose(film);
		film = NULL;
	}
}

void ae_match_film_game_start(char const *map, char const *engine, char const *header_rest)
{
	film_exit();
	film_finished = FALSE;
	film_name[0] = 0;
	film_start_line[0] = 0;
	snprintf(film_map, sizeof film_map, "%s", map ? map : "");
	snprintf(film_engine, sizeof film_engine, "%s", engine ? engine : "");
	snprintf(film_header_rest, sizeof film_header_rest, "%s", header_rest ? header_rest : "");
}

void ae_match_film_note_start_line(char const *line)
{
	snprintf(film_start_line, sizeof film_start_line, "%s", line ? line : "");
}

void ae_match_film_open(void)
{
	char path[512], stamp[32];
	int attempt;
	time_t now;
	struct tm *local;

	if (film || film_finished)
		return;
	film_finished = TRUE;
	config_folder(film_folder, sizeof film_folder);
	if (strlen(film_folder) + 7 >= sizeof film_folder)
		return;
	strcat(film_folder, "/films");
	posix_make_directory(film_folder);
	now = time(NULL);
	local = localtime(&now);
	if (!local || !strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", local))
		snprintf(stamp, sizeof stamp, "00000000-000000");
	/* (the process id tells two servers of one second apart; a name taken, by a game of this process in the same second, gets
	a count: a film is never opened over another) */
	for (attempt = 0; attempt < 100; attempt++)
	{
		struct posix_file_information information;
		char unique[32];

		if (attempt)
			snprintf(unique, sizeof unique, "%lu_%d", (unsigned long)posix_process_id(), attempt);
		else
			snprintf(unique, sizeof unique, "%lu", (unsigned long)posix_process_id());
		if (!ae_match_film_file_name(film_name, sizeof film_name, stamp, film_map, film_engine, unique))
		{
			platform_log("match film: no name for the map %s", film_map);
			film_name[0] = 0;
			return;
		}
		if (snprintf(path, sizeof path, "%s/%s", film_folder, film_name) >= (int)sizeof path)
		{
			film_name[0] = 0;
			return;
		}
		if (posix_stat(path, &information))
			break;
	}
	if (attempt == 100)
	{
		platform_log("match film: every name of this second is taken (%s)", film_name);
		film_name[0] = 0;
		return;
	}
	film = fopen(path, "w");
	if (!film)
	{
		platform_log("match film: could not open %s", path);
		film_name[0] = 0;
		return;
	}
	if (!exit_registered)
	{
		atexit(film_exit);
		exit_registered = TRUE;
	}
	fprintf(film, "ae film: v=1 map=%s engine=%s%s%s\n", film_map, film_engine, film_header_rest[0] ? " " : "", film_header_rest);
	if (film_start_line[0])
		fprintf(film, "%s\n", film_start_line);
	platform_log("match film: %s", path);
	films_trim();
}

void ae_match_film_write(char const *line)
{
	if (film)
		fprintf(film, "%s\n", line);
}

void ae_match_film_flush(void)
{
	if (film)
		fflush(film);
}

void ae_match_film_game_end(void)
{
	film_finished = TRUE;
	film_exit();
	if (film_name[0])
		films_trim();
}
