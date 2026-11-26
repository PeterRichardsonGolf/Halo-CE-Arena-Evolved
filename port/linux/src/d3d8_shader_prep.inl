/*
D3D8_SHADER_PREP.INL

A map's shaders made while its loading screen is up (display.prepare_shaders),
so that a draw in play never stops the game to compile one. Included at the end
of d3d8_gl.c (HALO_SHADER_PREP), whose device and program cache it shares.
Ported from ChupathingyCE's #1012 (Milenko) and adapted to AE's OpenGL program
cache: see d3d8_shader_prep.h for the hooks in d3d8_gl.c.

A draw's shaders follow from its key (xgpu_shader_keys.h): the game's vertex
program and variant, and the pixel shader key. Each map has a list of the keys
its draws use: the stock maps' come with the game (xgpu_shader_keys_stock), and
every map played keeps its own in the cache folder (shader_keys/<map>.keys,
a line for each key it had not had, as its draws first use it), so a custom
map's shaders are made at its loading screen from its second load on.

As a map starts loading (d3d8_gl_map_loading, game_precache_new_map), its keys
are read, and each frame of the loading screen (D3DDevice_Present) makes as
many of their shaders and programs as PREP_LOADING_BUDGET allows, as a draw
does through the renderer's own functions (vertex_shader_get,
fragment_shader_get, program_get):

- a program the disk cache has (display.shader_cache: the binaries of
  program_cache_load) is loaded from it;
- any other has its two shaders' compile and its link asked for, and not
  waited on, then is taken as the driver finishes it: with
  GL_KHR_parallel_shader_compile (or ARB's: Mesa's, NVIDIA's, AMD's), the
  driver's threads compile them all at once, and the loading screen asks
  whether each is done (COMPLETION_STATUS) without waiting; without it, a
  compile and a link are done in turn, a frame's budget at a time. A program
  taken goes into the renderer's tables and the disk cache as a draw's would.

The loading screen waits for them (d3d8_gl_map_waiting, its progress
d3d8_gl_map_progress) for PREP_WAIT_SECONDS at most from the start of the load.
What is left then is made during play at PREP_PLAY_BUDGET a frame by a driver
that compiles in parallel; on another, the draws make what they need, as before.
Nothing waits for a shader in a frame of play that would not have waited for it
before.

debug.frame_hitches logs every frame of a map's first minute that came more
than its milliseconds after the one before it, and how many there were: the
measure of what is left to compile in play.
*/

#include "port_folders.h"

#include <ctype.h>

/* a frame's time for the shaders while the loading screen is up, and during
play (on a driver that compiles in parallel) */
#define PREP_LOADING_BUDGET 0.025
#define PREP_PLAY_BUDGET 0.002
/* the most the loading screen waits for them, from the start of the load,
and the most a load without one does (a map in the cache already) */
#define PREP_WAIT_SECONDS 5.0
#define PREP_LOAD_SECONDS 0.5
/* the most programs waiting on the driver at once */
#define PREP_PENDING_MAXIMUM 4096
/* (debug.frame_hitches: the first minute of a map) */
#define HITCH_WINDOW_SECONDS 60.0
/* GL_COMPLETION_STATUS_KHR, GL_KHR_parallel_shader_compile's (and ARB's) */
#define PREP_COMPLETION_STATUS 0x91B1

struct prep_pending
{
	/* the two shaders and the program linked of them, and where the renderer holds each shader (to drop one that
	failed to compile: the vertex program's slot, the pixel shader's entry) */
	GLuint vertex_shader, fragment_shader, program;
	GLuint *vertex_slot;
	struct fragment_entry *fragment_entry;
};

static struct
{
	BOOL initialized, enabled;
	/* the driver compiles on threads of its own, and says so without waiting */
	BOOL parallel;
	/* a shader made now has its compile asked for and not waited on (prep_compile), with this packed mask */
	BOOL submitting;
	unsigned long packed_mask;
	/* the map being prepared (its name as its file's, lower case), its keys, the next to start */
	char map[64];
	struct xgpu_shader_key_list keys;
	unsigned long next;
	/* where the keys are made before play: "at the loading screen", or
	"during the load" (a map in the cache already, loaded without one) */
	const char *where;
	/* started at the loading screen, which waits; the time it started */
	BOOL loading;
	double started;
	/* keys made, failed or found made already; of the keys */
	unsigned long done, total;
	/* made, and how long it took (logged once the map's keys are all done) */
	unsigned long made, failed, from_cache;
	BOOL reported;
	/* started and not finished */
	struct prep_pending *pending;
	unsigned long pending_count;
	/* the vertex programs the game made (D3DDevice_CreateVertexShader), to find a key's */
	struct vertex_shader_object **objects;
	unsigned long object_count, object_capacity;
	/* the map now played, and its own list's writer */
	char noting_map[64];
	struct xgpu_shader_key_writer *writer;
	unsigned long flushed_frame;
} prep;

/* when the map's load began (d3d8_gl_map_loading, or its load in the cache: d3d8_gl_map_loaded), for debug.frame_hitches' load time */
static double prep_load_started;
static char prep_load_map[64];
/* the loading screen began this load (d3d8_gl_map_loading), whose name for the map d3d8_gl_map_loaded then takes: the list
   read and the list written are one, whatever the game calls the map in each */
static BOOL prep_load_by_name;

/* (debug.frame_hitches) */
static struct
{
	/* a frame this much after the one before is a hitch (-1: not read yet, 0: none) */
	double seconds;
	char map[64];
	BOOL counting;
	double last, map_start;
	unsigned long count;
	double worst, total;
} hitches = { -1.0 };

static double prep_seconds(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

/* whether the driver compiles shaders on threads of its own, asked without
waiting (GL_KHR_parallel_shader_compile, or ARB's), with 1 thread: the game's
memory a map takes is 400 MB without this, 408 with 1 thread (and the same stutter gain), 431 with 2, 476 with 4,
and more with a thread for each of the machine's (a driver's compile thread holds some 30 MB) */
static BOOL prep_parallel_compile_start(void)
{
	typedef void (GLAPIENTRY *threads_function)(GLuint count);
	threads_function threads = NULL;

	if (SDL_GL_ExtensionSupported("GL_KHR_parallel_shader_compile"))
		threads = (threads_function)SDL_GL_GetProcAddress("glMaxShaderCompilerThreadsKHR");
	else if (SDL_GL_ExtensionSupported("GL_ARB_parallel_shader_compile"))
		threads = (threads_function)SDL_GL_GetProcAddress("glMaxShaderCompilerThreadsARB");
	else
		return FALSE;
	if (threads)
		threads(1);
	return TRUE;
}

static BOOL prep_enabled(void)
{
	if (!device.gl_ready)
		return FALSE;
	if (!prep.initialized)
	{
		prep.initialized = TRUE;
		prep.enabled = config_boolean("display.prepare_shaders") != 0;
		if (prep.enabled)
		{
			prep.parallel = prep_parallel_compile_start();
			platform_log("GPU: shaders made at a map's loading screen %s", prep.parallel ?
				"in parallel (parallel_shader_compile)" : "in turn (no parallel_shader_compile)");
		}
	}
	return prep.enabled;
}

/* a map's name as its list has it: its file's, without a folder or an
extension, in lower case */
static void prep_map_name(char *name, size_t size, const char *path)
{
	const char *start = path ? path : "";
	const char *at;
	size_t length = 0;

	for (at = start; *at; at++)
	{
		if (*at == '\\' || *at == '/')
			start = at + 1;
	}
	for (at = start; *at && *at != '.' && length + 1 < size; at++)
		name[length++] = (char)tolower((unsigned char)*at);
	name[length] = 0;
}

static void prep_list_path(char *path, size_t size, const char *map)
{
	snprintf(path, size, "%s/shader_keys/%s.keys", platform_folder(_port_folder_cache), map);
}

/* ---------- what the hooks in vertex_shader_get and fragment_shader_get ask */

/* the packed mask of the key being made, else the current vertex shader's, as a draw's */
static unsigned long prep_packed_mask(void)
{
	return prep.submitting ? prep.packed_mask : device.vertex_shader->packed_mask;
}

/* a shader's compile: asked for and not waited on while a key's is made (its status is
asked for when its program is taken), else as always */
static GLuint prep_compile(GLenum type, const char *source, const char *what)
{
	GLuint shader;

	if (!prep.submitting)
		return xgpu_compile_shader(type, source, what);
	shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	return shader;
}

/* ---------- the game's vertex programs */

static void prep_vertex_program_made(struct vertex_shader_object *object)
{
	object->code_hash = xgpu_vertex_code_hash(object->instructions, object->instruction_count * 4);
	if (prep.object_count == prep.object_capacity)
	{
		unsigned long capacity = prep.object_capacity ? prep.object_capacity * 2 : 256;
		struct vertex_shader_object **grown = realloc(prep.objects, capacity * sizeof(*grown));

		if (!grown)
			return;
		prep.objects = grown;
		prep.object_capacity = capacity;
	}
	prep.objects[prep.object_count++] = object;
}

static struct vertex_shader_object *prep_vertex_program(unsigned long long code_hash)
{
	static unsigned long last;
	unsigned long index;

	if (last < prep.object_count && prep.objects[last]->code_hash == code_hash)
		return prep.objects[last];
	for (index = 0; index < prep.object_count; index++)
	{
		if (prep.objects[index]->code_hash == code_hash)
		{
			last = index;
			return prep.objects[index];
		}
	}
	return NULL;
}

/* ---------- a map's own list */

/* a draw's key, the first time the map draws with its program: into the
map's list */
static void prep_note(unsigned long *noted, struct vertex_shader_object *object, BOOL immediate, BOOL lit,
	unsigned long packed_mask, const struct nv2a_pixel_shader_key *pixel)
{
	struct xgpu_program_key key;

	*noted = shader_prep_serial;
	/* (not the fixed-function program, which the game did not make) */
	if (!prep.writer || !object || !object->code_hash)
		return;
	memset(&key, 0, sizeof(key));
	key.vertex_code = object->code_hash;
	key.flags = (immediate ? _xgpu_key_immediate_bit : 0) | (lit ? _xgpu_key_lit_bit : 0);
	key.packed_mask = immediate ? 0 : packed_mask;
	key.pixel = *pixel;
	xgpu_shader_key_writer_add(prep.writer, &key);
}

/* the map now played: its own list opened (scenario_load) */
static void prep_noting_start(const char *map)
{
	char path[1100];

	if (!strcmp(prep.noting_map, map) && prep.writer)
		return;
	xgpu_shader_key_writer_close(prep.writer);
	prep.writer = NULL;
	snprintf(prep.noting_map, sizeof(prep.noting_map), "%s", map);
	/* (every program noted again for this map) */
	shader_prep_serial++;
	if (!*map || !*platform_folder(_port_folder_cache))
		return;
	snprintf(path, sizeof(path), "%s/shader_keys", platform_folder(_port_folder_cache));
	posix_make_directory(path);
	prep_list_path(path, sizeof(path), map);
	prep.writer = xgpu_shader_key_writer_open(path);
}

/* ---------- making a key's shaders */

/* the multisampling of the screen's targets now (the keys' alpha_test_samples
is the recording machine's), as prepare_draw's */
static unsigned char prep_screen_samples(void)
{
	int samples = anti_aliasing() == _anti_aliasing_msaa ? anti_aliasing_samples : 0;

	return (unsigned char)(samples > 1 ? samples : 0);
}

/* FALSE if the pending could not be kept (no memory): then it was not started */
static BOOL prep_pending_add(const struct prep_pending *pending)
{
	if (prep.pending_count % 256 == 0)
	{
		struct prep_pending *grown = realloc(prep.pending, (prep.pending_count + 256) * sizeof(*grown));

		if (!grown)
			return FALSE;
		prep.pending = grown;
	}
	prep.pending[prep.pending_count++] = *pending;
	return TRUE;
}

/* the renderer's entry for a pixel shader key (fragment_shader_get's table), or NULL */
static struct fragment_entry *prep_fragment_entry(const struct nv2a_pixel_shader_key *key)
{
	unsigned long hash = hash_words(key, sizeof(*key));
	struct fragment_entry *entry;

	for (entry = fragment_buckets[hash % FRAGMENT_BUCKETS]; entry; entry = entry->next)
	{
		if (entry->hash == hash && words_equal(&entry->key, key, sizeof(*key)))
			return entry;
	}
	return NULL;
}

/* a shader of a program that failed to link: if it failed to compile, logged as xgpu_compile_shader does, and
dropped as it is (the renderer holds 0 for it, tried: its draws are left out, and the doomed pair is not linked
again at each draw) */
static void prep_shader_check(GLuint shader, GLuint *slot, const char *what)
{
	GLint status = 0;

	if (!shader)
		return;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status)
		return;
	{
		char log[4096];

		log[0] = 0;
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		platform_log("cannot compile the %s shader (made for the loading screen):\n%s", what, log);
	}
	glDeleteShader(shader);
	if (slot && *slot == shader)
		*slot = 0;
}

static void prep_pending_remove(unsigned long index)
{
	prep.pending[index] = prep.pending[--prep.pending_count];
}

static struct program_entry *program_find(GLuint vertex_shader, GLuint fragment_shader)
{
	unsigned long hash = (vertex_shader * 2654435761UL) ^ fragment_shader;
	struct program_entry *entry;

	for (entry = program_buckets[hash % PROGRAM_BUCKETS]; entry; entry = entry->next)
	{
		if (entry->vertex_shader == vertex_shader && entry->fragment_shader == fragment_shader)
			return entry;
	}
	return NULL;
}

/* whether the disk cache has the program of these two shaders (program_cache_load) */
static BOOL prep_cache_has(GLuint vertex_shader, GLuint fragment_shader)
{
	struct program_cache_entry *entry;

	if (!program_cache.enabled)
		return FALSE;
	entry = program_cache_find(shader_hash(vertex_shader), shader_hash(fragment_shader));
	return entry != NULL && entry->length != 0;
}

/* a program of the pending's, finished: kept in the renderer's tables, and
its binary in the disk cache, as program_get does for a draw's. linked: its
link worked; if it did not, it is left to its draw, which links it again and
logs why. */
static void prep_take(struct prep_pending *pending, BOOL linked)
{
	if (!linked || program_find(pending->vertex_shader, pending->fragment_shader))
	{
		/* (or a draw needed it first and made its own) */
		glDeleteProgram(pending->program);
		if (!linked)
		{
			prep.failed++;
			prep_shader_check(pending->vertex_shader, pending->vertex_slot, "vertex");
			prep_shader_check(pending->fragment_shader, pending->fragment_entry ? &pending->fragment_entry->shader : NULL,
				"pixel");
		}
		return;
	}
	prep_prelinked = pending->program;
	program_get(pending->vertex_shader, pending->fragment_shader);
	prep_prelinked = 0;
	program_cache.linked++;
	program_cache_store(pending->program, shader_hash(pending->vertex_shader), shader_hash(pending->fragment_shader));
	prep.made++;
}

/* whether a program's link has finished (wait: it is waited for), 1 and 0 as it linked or not; -1 not yet */
static int prep_finished(GLuint program, BOOL wait)
{
	GLint status = 0;

	if (!wait && prep.parallel)
	{
		glGetProgramiv(program, PREP_COMPLETION_STATUS, &status);
		if (!status)
			return -1;
	}
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	return status ? 1 : 0;
}

/* a key's shaders and program started; FALSE if it has none to make */
static BOOL prep_start(const struct xgpu_program_key *recorded)
{
	struct vertex_shader_object *object = prep_vertex_program(recorded->vertex_code);
	struct nv2a_pixel_shader_key key = recorded->pixel;
	BOOL immediate = (recorded->flags & _xgpu_key_immediate_bit) != 0;
	BOOL lit = (recorded->flags & _xgpu_key_lit_bit) != 0;
	struct prep_pending pending;

	if (!object || !object->instructions)
		return FALSE;
	/* (as this machine draws it: its multisampling, its lighting) */
	if (key.alpha_test_samples)
		key.alpha_test_samples = prep_screen_samples();
	key.count_samples = 0;
	key.plasma_model = 0;
	if (lit && (!object->lighting.lights || object->lighting_failed || !per_pixel_lighting()))
	{
		lit = FALSE;
		key.per_pixel_lighting = 0;
	}
	memset(&pending, 0, sizeof(pending));
	pending.vertex_slot = lit ? &object->lit_shader[immediate ? 1 : 0] : &object->shader[immediate ? 1 : 0];
	prep.submitting = TRUE;
	prep.packed_mask = recorded->packed_mask;
	pending.vertex_shader = vertex_shader_get(object, immediate, lit);
	pending.fragment_shader = fragment_shader_get(&key);
	prep.submitting = FALSE;
	pending.fragment_entry = prep_fragment_entry(&key);
	if (!pending.vertex_shader || !pending.fragment_shader || program_find(pending.vertex_shader, pending.fragment_shader))
		return FALSE;
	if (prep_cache_has(pending.vertex_shader, pending.fragment_shader))
	{
		/* (loaded from the disk cache, in less than a link takes) */
		if (program_get(pending.vertex_shader, pending.fragment_shader))
		{
			prep.made++;
			prep.from_cache++;
		}
		else
			prep.failed++;
		return TRUE;
	}
	pending.program = glCreateProgram();
	glAttachShader(pending.program, pending.vertex_shader);
	glAttachShader(pending.program, pending.fragment_shader);
	if (program_cache.enabled)
		glProgramParameteri(pending.program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	glLinkProgram(pending.program);
	if (prep.parallel)
	{
		if (!prep_pending_add(&pending))
		{
			/* (not kept, so not started: its draw makes it) */
			glDeleteProgram(pending.program);
			return FALSE;
		}
	}
	else
		prep_take(&pending, prep_finished(pending.program, TRUE) == 1);
	return TRUE;
}

/* the pending keys the driver has finished (wait: all of them, waiting for the driver: a new map's start) */
static void prep_collect(double deadline, BOOL wait)
{
	unsigned long index = 0;

	while (index < prep.pending_count)
	{
		struct prep_pending *pending = &prep.pending[index];
		int linked = prep_finished(pending->program, wait);

		if (linked < 0)
		{
			index++;
			if (prep_seconds() > deadline)
				break;
			continue;
		}
		prep_take(pending, linked == 1);
		prep_pending_remove(index);
	}
}

/* each frame (D3DDevice_Present): the map's keys, as far as the frame's
budget goes */
static void prep_frame(void)
{
	double start, deadline;

	if (!prep.total || prep.reported || !device.gl_ready)
		return;
	start = prep_seconds();
	if (prep.loading && start - prep.started > PREP_WAIT_SECONDS)
		prep.loading = FALSE;
	if (!prep.loading && !prep.parallel)
	{
		/* (in play, a driver that compiles as it is asked would stop the
		frame: the draws make what is left, as before) */
		platform_log("shaders: %s: %lu of %lu keys made %s in %.1f s; the rest at their draws",
			prep.map, prep.done, prep.total, prep.where, start - prep.started);
		prep.reported = TRUE;
		xgpu_shader_key_list_free(&prep.keys);
		return;
	}
	deadline = start + (prep.loading ? PREP_LOADING_BUDGET : PREP_PLAY_BUDGET);
	prep_collect(deadline, FALSE);
	while (prep.next < prep.keys.count && prep.pending_count < PREP_PENDING_MAXIMUM && prep_seconds() < deadline)
		prep_start(&prep.keys.keys[prep.next++]);
	/* (a key is done once it is started and not waited for: made, found
	made, or with nothing to make here) */
	prep.done = prep.next > prep.pending_count ? prep.next - prep.pending_count : 0;
	if (prep.next >= prep.keys.count && !prep.pending_count)
	{
		platform_log("shaders: %s: %lu keys, %lu programs made (%lu from the shader cache)%s, %lu failed, in %.1f s %s",
			prep.map, prep.total, prep.made, prep.from_cache, prep.parallel ? " in parallel" : "", prep.failed,
			prep_seconds() - prep.started, prep.loading ? prep.where : "(some in play)");
		prep.reported = TRUE;
		prep.loading = FALSE;
		prep.done = prep.total;
		xgpu_shader_key_list_free(&prep.keys);
	}
}

/* ---------- the game's side (game_precache_new_map, scenario_load) */

static void prep_begin(const char *map, BOOL loading)
{
	const char *stock;
	unsigned long stock_size = 0, own;
	char path[1100];
	FILE *file;

	/* (a map's keys left from the one before are dropped, what it has
	pending taken) */
	prep_collect(0.0, TRUE);
	xgpu_shader_key_list_free(&prep.keys);
	snprintf(prep.map, sizeof(prep.map), "%s", map);
	prep.next = prep.done = prep.total = prep.made = prep.failed = prep.from_cache = 0;
	prep.reported = FALSE;
	prep.loading = loading;
	prep.where = "at the loading screen";
	prep.started = prep_seconds();
	if (!*map)
		return;
	/* this machine's list of the map (the keys its draws used), then the game's (every shader the map's tags hold),
	each key once */
	prep_list_path(path, sizeof(path), map);
	if ((file = fopen(path, "rb")) != NULL)
	{
		char *text = NULL;
		long size;

		if (fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && size < (8L << 20) &&
			fseek(file, 0, SEEK_SET) == 0 && (text = malloc((size_t)size)) != NULL &&
			fread(text, 1, (size_t)size, file) == (size_t)size)
		{
			unsigned long whole = (unsigned long)size;

			/* (not a last line cut short by a crash) */
			while (whole && text[whole - 1] != '\n')
				whole--;
			xgpu_shader_keys_parse(text, whole, NULL, &prep.keys);
		}
		free(text);
		fclose(file);
	}
	own = prep.keys.count;
	if ((stock = xgpu_shader_keys_stock(&stock_size)) != NULL)
	{
		struct xgpu_shader_key_list game = { NULL, 0, 0 };
		unsigned long index, other;

		xgpu_shader_keys_parse(stock, stock_size, map, &game);
		for (index = 0; index < game.count; index++)
		{
			for (other = 0; other < own; other++)
			{
				if (!memcmp(&prep.keys.keys[other], &game.keys[index], sizeof(game.keys[0])))
					break;
			}
			if (other == own)
			{
				/* (a key twice would make its program twice) */
				if (prep.keys.count == prep.keys.capacity)
				{
					unsigned long capacity = prep.keys.capacity ? prep.keys.capacity * 2 : 256;
					struct xgpu_program_key *grown = realloc(prep.keys.keys, capacity * sizeof(*grown));

					if (!grown)
						break;
					prep.keys.keys = grown;
					prep.keys.capacity = capacity;
				}
				prep.keys.keys[prep.keys.count++] = game.keys[index];
			}
		}
		xgpu_shader_key_list_free(&game);
	}
	prep.total = prep.keys.count;
	if (!prep.total)
	{
		/* (nothing to wait for) */
		prep.reported = TRUE;
		prep.loading = FALSE;
	}
	else
	{
		platform_log("shaders: %s: making %lu keys (%lu from the game's list, %lu from this machine's)%s", map,
			prep.total, prep.total - own, own, loading ? " at the loading screen" : "");
	}
}

void d3d8_gl_map_loading(const char *name)
{
	char map[64];

	prep_map_name(map, sizeof(map), name);
	prep_load_started = prep_seconds();
	snprintf(prep_load_map, sizeof(prep_load_map), "%s", map);
	prep_load_by_name = TRUE;
	if (!prep_enabled())
		return;
	prep_begin(map, TRUE);
}

float d3d8_gl_map_progress(void)
{
	if (!prep.loading || !prep.total)
		return -1.0f;
	return (float)prep.done / (float)prep.total;
}

int d3d8_gl_map_waiting(void)
{
	if (!prep.loading || !prep.total || !prep_enabled())
		return FALSE;
	if (prep.reported || prep_seconds() - prep.started > PREP_WAIT_SECONDS)
	{
		prep.loading = FALSE;
		return FALSE;
	}
	return TRUE;
}

void d3d8_gl_map_loaded(const char *name)
{
	char map[64];

	if (prep_load_by_name && prep_seconds() - prep_load_started < 120.0)
		snprintf(map, sizeof(map), "%s", prep_load_map);
	else
		prep_map_name(map, sizeof(map), name);
	prep_load_by_name = FALSE;
	if (strcmp(prep_load_map, map))
	{
		prep_load_started = prep_seconds();
		snprintf(prep_load_map, sizeof(prep_load_map), "%s", map);
	}
	if (prep_enabled())
	{
		prep_noting_start(map);
		/* (a map loaded without its loading screen, one in the cache already:
		still part of the load here, before its first frame, for PREP_LOAD_SECONDS at the most, about what
		loading a map from the cache takes: the keys the driver is still on, and those not started, go on
		in play where it compiles in parallel, and are made at their draws on another) */
		if (strcmp(prep.map, map))
		{
			prep_begin(map, FALSE);
			if (prep.total)
			{
				double start = prep_seconds();

				prep.loading = TRUE;
				prep.where = "during the load";
				while (prep.loading && !prep.reported && prep_seconds() - start < PREP_LOAD_SECONDS)
					prep_frame();
				prep.loading = FALSE;
			}
		}
	}
	/* (debug.frame_hitches counts from the map's first frame, with the setting off too) */
	snprintf(hitches.map, sizeof(hitches.map), "%s", map);
	hitches.counting = FALSE;
	hitches.map_start = -1.0;
}

/* whether a map's first frame is drawn first through every shader, unseen
(port/linux/game/render_enumerate.c), so that a renderer that makes its
pipelines at a draw has them made before the first frame is shown: not
OpenGL's, whose programs are made here */
int halo_gpu_warm_draws(void)
{
	return FALSE;
}

/* ---------- each frame */

static void prep_hitches(void)
{
	double now = prep_seconds(), interval;

	if (hitches.seconds < 0.0)
		hitches.seconds = config_integer("debug.frame_hitches") > 0 ? config_integer("debug.frame_hitches") / 1000.0 : 0.0;
	if (hitches.seconds <= 0.0)
		return;
	interval = hitches.last > 0.0 ? now - hitches.last : 0.0;
	hitches.last = now;
	if (!*hitches.map)
		return;
	if (hitches.map_start < 0.0)
	{
		/* (from the map's first frame: the one before it is the load's) */
		hitches.map_start = now;
		if (prep_load_started > 0.0)
			platform_log("load: %s: %.2f s from the start of its load to its first frame", hitches.map,
				now - prep_load_started);
		hitches.counting = TRUE;
		hitches.count = 0;
		hitches.worst = hitches.total = 0.0;
		return;
	}
	if (!hitches.counting)
		return;
	if (interval > hitches.seconds)
	{
		hitches.count++;
		hitches.total += interval;
		if (interval > hitches.worst)
			hitches.worst = interval;
		platform_log("frame hitch: %.1f ms, %.1f s into %s", interval * 1000.0, now - hitches.map_start, hitches.map);
	}
	if (now - hitches.map_start >= HITCH_WINDOW_SECONDS)
	{
		platform_log("frame hitches: %s's first minute had %lu over %.0f ms (worst %.1f ms, %.0f ms in all)",
			hitches.map, hitches.count, hitches.seconds * 1000.0, hitches.worst * 1000.0, hitches.total * 1000.0);
		hitches.counting = FALSE;
	}
}

static void prep_present(void)
{
	if (!device.gl_ready)
		return;
	prep_frame();
	if (prep.writer && device.frame - prep.flushed_frame > 300)
	{
		xgpu_shader_key_writer_flush(prep.writer);
		prep.flushed_frame = device.frame;
	}
	prep_hitches();
}
