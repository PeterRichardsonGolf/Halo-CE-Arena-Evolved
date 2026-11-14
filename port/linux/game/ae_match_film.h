/*
AE_MATCH_FILM.H

The match film's file (debug.match_film; the lines are the objective log's, ae_objective_log.c): one text file per game in
<config folder>/films, film-<YYYYmmdd-HHMMSS>-<map>-<engine>.txt, opened when the game starts (or, in "people" mode,
at the first person's spawn), closed at its end, at the next game's start or at exit. The oldest films go when the
folder is over debug.match_film_budget_mb, never the one being written. The host only.
*/

#ifndef __AE_MATCH_FILM_H
#define __AE_MATCH_FILM_H

#include "cseries.h"

/* the setting now (ae_match_film_rules.h's AE_MATCH_FILM_*): read once */
int ae_match_film_mode(void);
/* whether a film is being written */
boolean ae_match_film_is_open(void);
/* whether this game's film is over (written and closed, or failed to open): no new one until the next game */
boolean ae_match_film_finished(void);
/* a game starts: the one before is closed; the map, the engine word (the mode's), the rest of the film's first line
(gametype and build, as "key=value ..."); the game's start line is kept for the file to begin with, however late it opens */
void ae_match_film_game_start(char const *map, char const *engine, char const *header_rest);
void ae_match_film_note_start_line(char const *line);
/* the file is opened now (the mode's wish is the caller's): its first line, then the game's start line */
void ae_match_film_open(void);
/* a line of the film (nothing while no film is open) */
void ae_match_film_write(char const *line);
/* once a second: what is written is on disk */
void ae_match_film_flush(void);
/* the game is over: the film is closed */
void ae_match_film_game_end(void);

#endif
