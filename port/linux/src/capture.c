/*
CAPTURE.C

Screenshots and video recording (capture.h), on the desktop builds.

A screenshot (F9) reads the back buffer at the next present, before the red
dot is drawn over the window, and a thread of its own writes it as a PNG
(our own encoder below: PNG's filters and deflate with the fixed Huffman
codes) to screenshots/ beside config.toml, named by the time and the map.

A recording (F10) pipes raw frames to an ffmpeg child process (SDL's
process API: fork/exec here, CreateProcess on Windows) that encodes H.264
into recordings/<time>_<map>.video.mp4, at a fixed rate
(capture.record_fps): each present, the frames due by the wall clock since
the start are counted, and the picture is read for them, once however many
are due (a slow frame stands for several, a fast frame that none is due for
is skipped), so the video plays at real speed whatever the game's frame
rate. The read goes into a ring of pixel buffer objects and is copied out
two presents later, when the GPU has long finished it, into a bounded queue
of frames that a writer thread feeds to ffmpeg; the game thread never waits
for ffmpeg: with the queue full, the frame is dropped (counted, and its
time given to the next frame queued, so the timeline stays real). The
mixer's output (dsound_sdl.c) goes, from the audio thread, into a ring that
the writer drains to a raw float file beside the video; when the recording
stops the writer closes ffmpeg's input and runs ffmpeg again to put the two
together (the video copied, the sound encoded as AAC) into <name>.mp4, and
removes the parts. A recording still being saved when the game quits is
finished first (atexit).

Notices ("SCREENSHOT SAVED", "RECORDING NEEDS FFMPEG", ...) are drawn by the
game for 1.5 seconds (halo_capture_notice, main.c), and are in the frames
recorded then; the red dot is not.
*/

#include "platform.h"
#include "port_config.h"
#include "capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HALO_SERVER) || defined(HALO_ANDROID)

/* ---------- no capture */

void capture_request_screenshot(void)
{
}

void capture_request_recording_toggle(void)
{
}

void capture_frame(unsigned int framebuffer, int width, int height)
{
	(void)framebuffer;
	(void)width;
	(void)height;
}

void capture_present_overlay(int x, int y, int width, int height)
{
	(void)x;
	(void)y;
	(void)width;
	(void)height;
}

void capture_audio(const float *samples, unsigned int frames, unsigned int channels, unsigned int rate)
{
	(void)samples;
	(void)frames;
	(void)channels;
	(void)rate;
}

int capture_png_write_bgra(const char *path, const unsigned char *pixels, int width, int height)
{
	(void)path;
	(void)pixels;
	(void)width;
	(void)height;
	return 0;
}

int halo_capture_notice(char *text, int size)
{
	(void)text;
	(void)size;
	return 0;
}

#else

#include "gl.h"
#include "zlib_prefixed.h"

#include <SDL3/SDL.h>
#ifndef _WIN32
#include <fcntl.h>
#endif

/* the game (port/linux/game/capture_game.c) */
int capture_game_playing(void);
void capture_game_map_name(char *name, int size);

#define AUDIO_RATE 48000
#define AUDIO_CHANNELS 2
/* the audio ring: 4 seconds */
#define AUDIO_RING_FRAMES (AUDIO_RATE * 4)
#define PBO_COUNT 3
/* the frame queue: at most this much memory, and 3 to 12 frames */
#define QUEUE_BUDGET_BYTES (160u * 1024u * 1024u)
#define QUEUE_MINIMUM 3
#define QUEUE_MAXIMUM 12
#define NOTICE_MS 1500
#define PATH_SIZE 1024

/* ---------- notices */

static SDL_SpinLock notice_lock;
static char notice_text[64];
static int notice_kind;
static Uint64 notice_until;

static void notice(int kind, const char *text)
{
	SDL_LockSpinlock(&notice_lock);
	SDL_strlcpy(notice_text, text, sizeof(notice_text));
	notice_kind = kind;
	notice_until = SDL_GetTicks() + NOTICE_MS;
	SDL_UnlockSpinlock(&notice_lock);
}

int halo_capture_notice(char *text, int size)
{
	int kind = 0;

	SDL_LockSpinlock(&notice_lock);
	if (notice_kind && SDL_GetTicks() < notice_until)
	{
		SDL_strlcpy(text, notice_text, (size_t)size);
		kind = notice_kind;
	}
	SDL_UnlockSpinlock(&notice_lock);
	return kind;
}

/* ---------- PNG */

struct byte_buffer
{
	unsigned char *data;
	size_t size;
	size_t capacity;
	int failed;
};

static void buffer_reserve(struct byte_buffer *buffer, size_t more)
{
	if (buffer->failed || buffer->size + more <= buffer->capacity)
		return;
	{
		size_t capacity = buffer->capacity ? buffer->capacity : 65536;
		unsigned char *data;

		while (capacity < buffer->size + more)
			capacity *= 2;
		data = realloc(buffer->data, capacity);
		if (!data)
		{
			buffer->failed = 1;
			return;
		}
		buffer->data = data;
		buffer->capacity = capacity;
	}
}

static void buffer_put(struct byte_buffer *buffer, const void *data, size_t size)
{
	buffer_reserve(buffer, size);
	if (buffer->failed)
		return;
	memcpy(buffer->data + buffer->size, data, size);
	buffer->size += size;
}

static void buffer_put_u32(struct byte_buffer *buffer, Uint32 value)
{
	unsigned char bytes[4];

	bytes[0] = (unsigned char)(value >> 24);
	bytes[1] = (unsigned char)(value >> 16);
	bytes[2] = (unsigned char)(value >> 8);
	bytes[3] = (unsigned char)value;
	buffer_put(buffer, bytes, 4);
}

/* deflate's bits, least significant first */
struct bit_writer
{
	struct byte_buffer *out;
	Uint64 bits;
	int count;
};

static void bits_put(struct bit_writer *writer, Uint32 value, int count)
{
	writer->bits |= (Uint64)value << writer->count;
	writer->count += count;
	while (writer->count >= 8)
	{
		unsigned char byte = (unsigned char)writer->bits;

		buffer_put(writer->out, &byte, 1);
		writer->bits >>= 8;
		writer->count -= 8;
	}
}

static Uint32 bits_reversed(Uint32 code, int length)
{
	Uint32 result = 0;
	int bit;

	for (bit = 0; bit < length; bit++)
		result |= ((code >> bit) & 1) << (length - 1 - bit);
	return result;
}

/* a symbol of the fixed literal/length code (RFC 1951 3.2.6) */
static void put_literal_length(struct bit_writer *writer, int symbol)
{
	if (symbol < 144)
		bits_put(writer, bits_reversed(0x30 + (Uint32)symbol, 8), 8);
	else if (symbol < 256)
		bits_put(writer, bits_reversed(0x190 + (Uint32)(symbol - 144), 9), 9);
	else if (symbol < 280)
		bits_put(writer, bits_reversed((Uint32)(symbol - 256), 7), 7);
	else
		bits_put(writer, bits_reversed(0xc0 + (Uint32)(symbol - 280), 8), 8);
}

static const unsigned short length_base[29] =
{
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const unsigned char length_extra[29] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const unsigned short distance_base[30] =
{
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
	4097, 6145, 8193, 12289, 16385, 24577
};
static const unsigned char distance_extra[30] =
{
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static void put_match(struct bit_writer *writer, int length, int distance)
{
	int code = 28, distance_code = 29;

	while (length_base[code] > length)
		code--;
	put_literal_length(writer, 257 + code);
	if (length_extra[code])
		bits_put(writer, (Uint32)(length - length_base[code]), length_extra[code]);
	while (distance_base[distance_code] > distance)
		distance_code--;
	bits_put(writer, bits_reversed((Uint32)distance_code, 5), 5);
	if (distance_extra[distance_code])
		bits_put(writer, (Uint32)(distance - distance_base[distance_code]), distance_extra[distance_code]);
}

#define WINDOW_SIZE 32768
#define HASH_BITS 15
#define HASH_SIZE (1 << HASH_BITS)
#define MAXIMUM_CHAIN 16
#define MAXIMUM_MATCH 258

static Uint32 hash3(const unsigned char *data)
{
	return (((Uint32)data[0] << 16 | (Uint32)data[1] << 8 | data[2]) * 2654435761u) >> (32 - HASH_BITS);
}

/* a zlib stream of data: one block of the fixed codes, matches found by a
hash chain over the last 32 KB; 1 on success */
static int zlib_compress(const unsigned char *data, size_t size, struct byte_buffer *out)
{
	struct bit_writer writer = { out, 0, 0 };
	Sint32 *head = malloc(sizeof(Sint32) * HASH_SIZE);
	Sint32 *previous = malloc(sizeof(Sint32) * WINDOW_SIZE);
	size_t position = 0;
	unsigned char header[2] = { 0x78, 0x01 };
	uLong adler;

	if (!head || !previous)
	{
		free(head);
		free(previous);
		return 0;
	}
	memset(head, 0xff, sizeof(Sint32) * HASH_SIZE);
	buffer_put(out, header, 2);
	/* final block, fixed Huffman codes */
	bits_put(&writer, 1, 1);
	bits_put(&writer, 1, 2);
	while (position < size && !out->failed)
	{
		int best_length = 0, best_distance = 0;

		if (position + 3 <= size)
		{
			Uint32 hash = hash3(data + position);
			Sint32 candidate = head[hash];
			int chain = MAXIMUM_CHAIN;
			size_t limit = size - position < MAXIMUM_MATCH ? size - position : MAXIMUM_MATCH;

			while (candidate >= 0 && chain-- > 0 && position - (size_t)candidate <= WINDOW_SIZE - 1)
			{
				const unsigned char *a = data + candidate, *b = data + position;
				size_t length = 0;

				if (a[best_length] == b[best_length])
				{
					while (length < limit && a[length] == b[length])
						length++;
					if ((int)length > best_length)
					{
						best_length = (int)length;
						best_distance = (int)(position - (size_t)candidate);
						if (length == limit)
							break;
					}
				}
				candidate = previous[candidate & (WINDOW_SIZE - 1)];
			}
		}
		if (best_length >= 3)
		{
			size_t end = position + (size_t)best_length;

			put_match(&writer, best_length, best_distance);
			for (; position < end; position++)
			{
				if (position + 3 <= size)
				{
					Uint32 hash = hash3(data + position);

					previous[position & (WINDOW_SIZE - 1)] = head[hash];
					head[hash] = (Sint32)position;
				}
			}
		}
		else
		{
			put_literal_length(&writer, data[position]);
			if (position + 3 <= size)
			{
				Uint32 hash = hash3(data + position);

				previous[position & (WINDOW_SIZE - 1)] = head[hash];
				head[hash] = (Sint32)position;
			}
			position++;
		}
	}
	put_literal_length(&writer, 256);
	if (writer.count)
		bits_put(&writer, 0, 8 - writer.count);
	free(head);
	free(previous);
	adler = z_adler32(0L, Z_NULL, 0);
	{
		/* (in pieces: uInt is 32 bits) */
		size_t done = 0;

		while (done < size)
		{
			size_t piece = size - done > 0x40000000u ? 0x40000000u : size - done;

			adler = z_adler32(adler, data + done, (uInt)piece);
			done += piece;
		}
	}
	buffer_put_u32(out, (Uint32)adler);
	return !out->failed;
}

static int paeth(int a, int b, int c)
{
	int p = a + b - c;
	int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);

	if (pa <= pb && pa <= pc)
		return a;
	return pb <= pc ? b : c;
}

static void png_chunk(struct byte_buffer *out, const char *type, const unsigned char *data, size_t size)
{
	uLong crc = z_crc32(0L, Z_NULL, 0);

	buffer_put_u32(out, (Uint32)size);
	buffer_put(out, type, 4);
	crc = z_crc32(crc, (const Bytef *)type, 4);
	if (size)
	{
		buffer_put(out, data, size);
		crc = z_crc32(crc, data, (uInt)size);
	}
	buffer_put_u32(out, (Uint32)crc);
}

/* the picture as an RGB PNG in out: each row filtered by whichever of PNG's
filters gives the smallest sum of magnitudes (the usual heuristic) */
static int png_encode(const unsigned char *bgra, int width, int height, struct byte_buffer *out)
{
	size_t stride = (size_t)width * 3 + 1;
	unsigned char *filtered = malloc(stride * (size_t)height);
	unsigned char *rows[2], *trial[5];
	struct byte_buffer compressed = { 0 };
	int y, x, filter, ok;
	static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	unsigned char header[13];

	rows[0] = calloc((size_t)width * 3, 1);
	rows[1] = malloc((size_t)width * 3);
	for (filter = 0; filter < 5; filter++)
		trial[filter] = malloc((size_t)width * 3);
	ok = filtered && rows[0] && rows[1];
	for (filter = 0; filter < 5; filter++)
		ok = ok && trial[filter];
	for (y = 0; ok && y < height; y++)
	{
		const unsigned char *source = bgra + (size_t)y * (size_t)width * 4;
		/* (rows[] alternate: this row's, the one above's) */
		unsigned char *row = rows[y & 1];
		unsigned char *above = rows[(y + 1) & 1];
		unsigned int best_sum = 0xffffffffu;
		int best = 0;

		if (y == 0)
			memset(above, 0, (size_t)width * 3);
		for (x = 0; x < width; x++)
		{
			row[x * 3] = source[x * 4 + 2];
			row[x * 3 + 1] = source[x * 4 + 1];
			row[x * 3 + 2] = source[x * 4];
		}
		for (filter = 0; filter < 5; filter++)
		{
			unsigned int sum = 0;
			int i, n = width * 3;

			for (i = 0; i < n; i++)
			{
				int left = i >= 3 ? row[i - 3] : 0;
				int up = above[i];
				int corner = i >= 3 ? above[i - 3] : 0;
				int predicted = filter == 0 ? 0 : filter == 1 ? left : filter == 2 ? up :
					filter == 3 ? (left + up) / 2 : paeth(left, up, corner);
				unsigned char value = (unsigned char)(row[i] - predicted);

				trial[filter][i] = value;
				sum += value < 128 ? value : 256 - value;
			}
			if (sum < best_sum)
			{
				best_sum = sum;
				best = filter;
			}
		}
		filtered[(size_t)y * stride] = (unsigned char)best;
		memcpy(filtered + (size_t)y * stride + 1, trial[best], (size_t)width * 3);
	}
	if (ok)
		ok = zlib_compress(filtered, stride * (size_t)height, &compressed);
	if (ok)
	{
		header[0] = (unsigned char)(width >> 24);
		header[1] = (unsigned char)(width >> 16);
		header[2] = (unsigned char)(width >> 8);
		header[3] = (unsigned char)width;
		header[4] = (unsigned char)(height >> 24);
		header[5] = (unsigned char)(height >> 16);
		header[6] = (unsigned char)(height >> 8);
		header[7] = (unsigned char)height;
		header[8] = 8; /* bits a sample */
		header[9] = 2; /* RGB */
		header[10] = header[11] = header[12] = 0;
		buffer_put(out, signature, sizeof(signature));
		png_chunk(out, "IHDR", header, sizeof(header));
		png_chunk(out, "IDAT", compressed.data, compressed.size);
		png_chunk(out, "IEND", NULL, 0);
		ok = !out->failed;
	}
	free(compressed.data);
	free(filtered);
	free(rows[0]);
	free(rows[1]);
	for (filter = 0; filter < 5; filter++)
		free(trial[filter]);
	return ok;
}

int capture_png_write_bgra(const char *path, const unsigned char *pixels, int width, int height)
{
	struct byte_buffer png = { 0 };
	int ok = width > 0 && height > 0 && png_encode(pixels, width, height, &png);

	if (ok)
	{
		SDL_IOStream *file = SDL_IOFromFile(path, "wb");

		ok = file && SDL_WriteIO(file, png.data, png.size) == png.size;
		if (file && !SDL_CloseIO(file))
			ok = 0;
		if (!ok)
			SDL_RemovePath(path);
	}
	free(png.data);
	return ok;
}

/* ---------- names and folders */

/* <config.toml's folder><name>, or override if set; made if missing */
static void capture_folder(const char *name, const char *override, char *path, size_t size)
{
	if (override && *override)
	{
		SDL_strlcpy(path, override, size);
	}
	else
	{
		config_folder(path, size);
		SDL_strlcat(path, name, size);
	}
	SDL_CreateDirectory(path);
}

/* <folder>/<date>_<time>[_<map>][_<n>]<extension>, a name not yet taken
(of base, without the extension, in base) */
static void capture_name(const char *folder, const char *extension, char *path, size_t size, char *base,
	size_t base_size)
{
	SDL_Time now = 0;
	SDL_DateTime date;
	char stamp[64], map[64], candidate[PATH_SIZE];
	int number;

	memset(&date, 0, sizeof(date));
	if (SDL_GetCurrentTime(&now))
		SDL_TimeToDateTime(now, &date, true);
	snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d_%02d-%02d-%02d", date.year, date.month, date.day, date.hour,
		date.minute, date.second);
	map[0] = 0;
	capture_game_map_name(map, sizeof(map));
	if (map[0])
	{
		SDL_strlcat(stamp, "_", sizeof(stamp));
		SDL_strlcat(stamp, map, sizeof(stamp));
	}
	for (number = 1; number < 100; number++)
	{
		if (number == 1)
			snprintf(base, base_size, "%s", stamp);
		else
			snprintf(base, base_size, "%s_%d", stamp, number);
		snprintf(candidate, sizeof(candidate), "%s/%s%s", folder, base, extension);
		if (!SDL_GetPathInfo(candidate, NULL))
			break;
	}
	SDL_strlcpy(path, candidate, size);
}

/* ffmpeg: capture.ffmpeg_path, else beside the game, else on the PATH; 1
if found (that it is there: SDL_PathInfo's 64-bit members are laid out
otherwise in the 32-bit build, -malign-double, so its type is not asked) */
static int ffmpeg_find(char *path, size_t size)
{
#ifdef _WIN32
	static const char program[] = "ffmpeg.exe";
	const char separator = ';';
#else
	static const char program[] = "ffmpeg";
	const char separator = ':';
#endif
	const char *configured = config_string("capture.ffmpeg_path");
	const char *base = SDL_GetBasePath();
	const char *search = SDL_getenv("PATH");

	if (*configured)
	{
		SDL_strlcpy(path, configured, size);
		return SDL_GetPathInfo(path, NULL);
	}
	if (base)
	{
		snprintf(path, size, "%s%s", base, program);
		if (SDL_GetPathInfo(path, NULL))
			return 1;
	}
	while (search && *search)
	{
		const char *end = strchr(search, separator);
		size_t length = end ? (size_t)(end - search) : strlen(search);

		if (length && length < size - sizeof(program) - 1)
		{
			snprintf(path, size, "%.*s/%s", (int)length, search, program);
			if (SDL_GetPathInfo(path, NULL))
				return 1;
		}
		search = end ? end + 1 : NULL;
	}
	return 0;
}

/* ---------- screenshots */

static SDL_AtomicInt screenshot_requested;
static SDL_AtomicInt screenshots_writing;

struct screenshot
{
	unsigned char *pixels;
	int width;
	int height;
	char path[PATH_SIZE];
};

static int SDLCALL screenshot_thread(void *parameter)
{
	struct screenshot *shot = parameter;
	Uint64 start = SDL_GetTicks();

	if (capture_png_write_bgra(shot->path, shot->pixels, shot->width, shot->height))
	{
		platform_log("capture: screenshot %s (%dx%d, %u ms)", shot->path, shot->width, shot->height,
			(unsigned)(SDL_GetTicks() - start));
		notice(1, "SCREENSHOT SAVED");
	}
	else
	{
		platform_log("capture: cannot write the screenshot %s", shot->path);
		notice(2, "SCREENSHOT FAILED");
	}
	free(shot->pixels);
	free(shot);
	SDL_AddAtomicInt(&screenshots_writing, -1);
	return 0;
}

void capture_request_screenshot(void)
{
	SDL_SetAtomicInt(&screenshot_requested, 1);
}

static void screenshot_take(unsigned int framebuffer, int width, int height)
{
	struct screenshot *shot;
	char folder[PATH_SIZE], base[256];
	SDL_Thread *thread;

	if (SDL_GetAtomicInt(&screenshots_writing) >= 4)
	{
		notice(2, "SCREENSHOT BUSY");
		return;
	}
	shot = calloc(1, sizeof(*shot));
	if (shot)
		shot->pixels = malloc((size_t)width * (size_t)height * 4);
	if (!shot || !shot->pixels)
	{
		free(shot);
		notice(2, "SCREENSHOT FAILED");
		return;
	}
	shot->width = width;
	shot->height = height;
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
	glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, shot->pixels);
	capture_folder("screenshots", NULL, folder, sizeof(folder));
	capture_name(folder, ".png", shot->path, sizeof(shot->path), base, sizeof(base));
	SDL_AddAtomicInt(&screenshots_writing, 1);
	thread = SDL_CreateThread(screenshot_thread, "screenshot", shot);
	if (thread)
	{
		SDL_DetachThread(thread);
	}
	else
	{
		screenshot_thread(shot);
	}
}

/* ---------- recording: the writer */

struct recording
{
	/* set at the start, read by both threads */
	int width;
	int height;
	int fps;
	int crf;
	size_t frame_bytes;
	char ffmpeg[PATH_SIZE];
	char final_path[PATH_SIZE];
	char video_path[PATH_SIZE];
	char audio_path[PATH_SIZE];

	SDL_Thread *thread;
	SDL_Mutex *lock;
	SDL_Condition *wake;
	/* the queue (lock): slots[] of frame_bytes each, free ones on a stack,
	full ones in order with how many frames each stands for */
	int slot_count;
	unsigned char *slots[QUEUE_MAXIMUM];
	int free_slots[QUEUE_MAXIMUM];
	int free_count;
	int queued[QUEUE_MAXIMUM];
	int queued_repeat[QUEUE_MAXIMUM];
	int queue_head;
	int queue_count;
	int stopping;

	/* the sound (audio_lock) */
	float *audio_ring;
	size_t audio_read;
	size_t audio_count;
	Uint64 audio_lost;
	Uint64 audio_frames;

	/* the writer's */
	Uint64 frames_written;
	SDL_AtomicInt finished;
};

static SDL_Mutex *audio_lock;
/* the recording the mixer feeds (audio_lock) */
static struct recording *audio_recording;

/* ffmpeg's input written whole; 1 on success */
static int pipe_write(SDL_IOStream *stream, const unsigned char *data, size_t size)
{
	while (size)
	{
		size_t written = SDL_WriteIO(stream, data, size);

		data += written;
		size -= written;
		if (!size)
			break;
		if (SDL_GetIOStatus(stream) != SDL_IO_STATUS_NOT_READY)
			return 0;
		/* (Windows' pipe, if it does not wait: try again shortly) */
		SDL_DelayNS(200000);
	}
	return 1;
}

/* SDL hands the pipe over without waiting (O_NONBLOCK); a writer thread
of its own can wait, and a larger pipe takes a frame in fewer turns */
static void pipe_make_waiting(SDL_IOStream *stream)
{
#ifndef _WIN32
	Sint64 descriptor = SDL_GetNumberProperty(SDL_GetIOProperties(stream), SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER,
		-1);

	if (descriptor >= 0)
	{
		int flags = fcntl((int)descriptor, F_GETFL);

		if (flags != -1)
			fcntl((int)descriptor, F_SETFL, flags & ~O_NONBLOCK);
#ifdef F_SETPIPE_SZ
		fcntl((int)descriptor, F_SETPIPE_SZ, 1 << 20);
#endif
	}
#else
	(void)stream;
#endif
}

static SDL_Process *process_start(const char *const *arguments, int with_input)
{
	SDL_PropertiesID properties = SDL_CreateProperties();
	SDL_Process *process;

	SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)arguments);
	SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
		with_input ? SDL_PROCESS_STDIO_APP : SDL_PROCESS_STDIO_NULL);
	SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
	SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);
	process = SDL_CreateProcessWithProperties(properties);
	SDL_DestroyProperties(properties);
	if (!process)
		platform_log("capture: cannot start ffmpeg (%s)", SDL_GetError());
	return process;
}

/* the ring's sound, to the file; with the lost frames as silence */
static void audio_drain(struct recording *recording, SDL_IOStream *file, float *scratch, size_t scratch_frames)
{
	for (;;)
	{
		size_t frames, first;
		Uint64 lost;

		SDL_LockMutex(audio_lock);
		frames = recording->audio_count < scratch_frames ? recording->audio_count : scratch_frames;
		first = AUDIO_RING_FRAMES - recording->audio_read;
		if (first > frames)
			first = frames;
		memcpy(scratch, recording->audio_ring + recording->audio_read * AUDIO_CHANNELS,
			first * AUDIO_CHANNELS * sizeof(float));
		memcpy(scratch + first * AUDIO_CHANNELS, recording->audio_ring,
			(frames - first) * AUDIO_CHANNELS * sizeof(float));
		recording->audio_read = (recording->audio_read + frames) % AUDIO_RING_FRAMES;
		recording->audio_count -= frames;
		lost = recording->audio_lost;
		recording->audio_lost = 0;
		SDL_UnlockMutex(audio_lock);
		if (file && frames)
			SDL_WriteIO(file, scratch, frames * AUDIO_CHANNELS * sizeof(float));
		recording->audio_frames += frames;
		if (lost)
		{
			static const float silence[256 * AUDIO_CHANNELS];

			recording->audio_frames += lost;
			while (file && lost)
			{
				size_t piece = lost > 256 ? 256 : (size_t)lost;

				SDL_WriteIO(file, silence, piece * AUDIO_CHANNELS * sizeof(float));
				lost -= piece;
			}
		}
		if (frames < scratch_frames)
			break;
	}
}

static int SDLCALL recording_thread(void *parameter)
{
	struct recording *recording = parameter;
	char size_text[32], rate_text[16], crf_text[16];
	const char *arguments[32];
	int count = 0, video_ok, exit_code = -1, has_audio;
	SDL_Process *process;
	SDL_IOStream *input = NULL, *audio_file;
	size_t scratch_frames = 4096;
	float *scratch = malloc(scratch_frames * AUDIO_CHANNELS * sizeof(float));

	snprintf(size_text, sizeof(size_text), "%dx%d", recording->width, recording->height);
	snprintf(rate_text, sizeof(rate_text), "%d", recording->fps);
	snprintf(crf_text, sizeof(crf_text), "%d", recording->crf);
	arguments[count++] = recording->ffmpeg;
	arguments[count++] = "-hide_banner";
	arguments[count++] = "-loglevel";
	arguments[count++] = "error";
	arguments[count++] = "-y";
	arguments[count++] = "-f";
	arguments[count++] = "rawvideo";
	arguments[count++] = "-pix_fmt";
	arguments[count++] = "bgr0";
	arguments[count++] = "-video_size";
	arguments[count++] = size_text;
	arguments[count++] = "-framerate";
	arguments[count++] = rate_text;
	arguments[count++] = "-i";
	arguments[count++] = "pipe:0";
	arguments[count++] = "-c:v";
	arguments[count++] = "libx264";
	arguments[count++] = "-preset";
	arguments[count++] = "veryfast";
	arguments[count++] = "-crf";
	arguments[count++] = crf_text;
	arguments[count++] = "-pix_fmt";
	arguments[count++] = "yuv420p";
	arguments[count++] = recording->video_path;
	arguments[count] = NULL;
	process = process_start(arguments, 1);
	if (process)
	{
		input = SDL_GetProcessInput(process);
		if (input)
			pipe_make_waiting(input);
	}
	video_ok = input != NULL;
	audio_file = SDL_IOFromFile(recording->audio_path, "wb");

	SDL_LockMutex(recording->lock);
	for (;;)
	{
		int slot, repeat;

		while (!recording->queue_count && !recording->stopping)
			SDL_WaitConditionTimeout(recording->wake, recording->lock, 50);
		SDL_UnlockMutex(recording->lock);
		if (scratch)
			audio_drain(recording, audio_file, scratch, scratch_frames);
		SDL_LockMutex(recording->lock);
		if (!recording->queue_count)
		{
			if (recording->stopping)
				break;
			continue;
		}
		slot = recording->queued[recording->queue_head];
		repeat = recording->queued_repeat[recording->queue_head];
		recording->queue_head = (recording->queue_head + 1) % QUEUE_MAXIMUM;
		recording->queue_count--;
		SDL_UnlockMutex(recording->lock);
		while (video_ok && repeat-- > 0)
		{
			video_ok = pipe_write(input, recording->slots[slot], recording->frame_bytes);
			if (video_ok)
				recording->frames_written++;
		}
		SDL_LockMutex(recording->lock);
		recording->free_slots[recording->free_count++] = slot;
	}
	SDL_UnlockMutex(recording->lock);

	/* the mixer stops feeding it; the rest of the sound */
	SDL_LockMutex(audio_lock);
	if (audio_recording == recording)
		audio_recording = NULL;
	SDL_UnlockMutex(audio_lock);
	if (scratch)
		audio_drain(recording, audio_file, scratch, scratch_frames);
	has_audio = audio_file && recording->audio_frames > 0;
	if (audio_file)
		SDL_CloseIO(audio_file);

	if (process)
	{
		if (input)
			SDL_CloseIO(input);
		SDL_WaitProcess(process, true, &exit_code);
		SDL_DestroyProcess(process);
	}
	video_ok = video_ok && exit_code == 0 && recording->frames_written > 0;
	if (video_ok && has_audio)
	{
		/* the picture as it is, the sound as AAC, as long as the picture:
		sound that came short of it (an audio device that fell behind real
		time, as SDL's dummy driver under load) stretched to it, and padded
		with silence to its last frame */
		const char *mux[40];
		char filter[64];
		double video_seconds = (double)recording->frames_written / recording->fps;
		double audio_seconds = (double)recording->audio_frames / AUDIO_RATE;
		double ratio = video_seconds > 0.0 ? audio_seconds / video_seconds : 1.0;

		if (ratio >= 0.5 && ratio <= 0.99)
			snprintf(filter, sizeof(filter), "atempo=%.5f,apad", ratio);
		else
			snprintf(filter, sizeof(filter), "apad");
		count = 0;
		mux[count++] = recording->ffmpeg;
		mux[count++] = "-hide_banner";
		mux[count++] = "-loglevel";
		mux[count++] = "error";
		mux[count++] = "-y";
		mux[count++] = "-i";
		mux[count++] = recording->video_path;
		mux[count++] = "-f";
		mux[count++] = "f32le";
		mux[count++] = "-ar";
		mux[count++] = "48000";
		mux[count++] = "-ac";
		mux[count++] = "2";
		mux[count++] = "-i";
		mux[count++] = recording->audio_path;
		mux[count++] = "-map";
		mux[count++] = "0:v";
		mux[count++] = "-map";
		mux[count++] = "1:a";
		mux[count++] = "-af";
		mux[count++] = filter;
		mux[count++] = "-shortest";
		mux[count++] = "-c:v";
		mux[count++] = "copy";
		mux[count++] = "-c:a";
		mux[count++] = "aac";
		mux[count++] = "-b:a";
		mux[count++] = "192k";
		mux[count++] = "-movflags";
		mux[count++] = "+faststart";
		mux[count++] = recording->final_path;
		mux[count] = NULL;
		process = process_start(mux, 0);
		exit_code = -1;
		if (process)
		{
			SDL_WaitProcess(process, true, &exit_code);
			SDL_DestroyProcess(process);
		}
		if (exit_code == 0)
		{
			SDL_RemovePath(recording->video_path);
		}
		else
		{
			platform_log("capture: cannot add the sound (ffmpeg %d); the recording has none", exit_code);
			has_audio = 0;
		}
	}
	if (video_ok && !has_audio)
		video_ok = SDL_RenamePath(recording->video_path, recording->final_path);
	SDL_RemovePath(recording->audio_path);
	if (video_ok)
	{
		platform_log("capture: recording %s: %llu frames written (%.1f s at %d fps), %.1f s of sound%s",
			recording->final_path, (unsigned long long)recording->frames_written,
			(double)recording->frames_written / recording->fps, recording->fps,
			(double)recording->audio_frames / AUDIO_RATE, has_audio ? "" : " (none in the file)");
		notice(1, "RECORDING SAVED");
	}
	else
	{
		platform_log("capture: the recording %s failed (ffmpeg exit %d, %llu frames written)", recording->final_path,
			exit_code, (unsigned long long)recording->frames_written);
		SDL_RemovePath(recording->video_path);
		notice(2, "RECORDING FAILED");
	}
	free(scratch);
	SDL_SetAtomicInt(&recording->finished, 1);
	return 0;
}

static void recording_free(struct recording *recording)
{
	int slot;

	for (slot = 0; slot < recording->slot_count; slot++)
		free(recording->slots[slot]);
	free(recording->audio_ring);
	if (recording->wake)
		SDL_DestroyCondition(recording->wake);
	if (recording->lock)
		SDL_DestroyMutex(recording->lock);
	free(recording);
}

/* ---------- recording: the game thread's side */

struct pixel_buffer
{
	GLuint id;
	int pending;
	int repeat;
	Uint64 issued;
};

static struct
{
	SDL_AtomicInt toggle_requested;
	/* the recording being made, and the last one being saved */
	struct recording *active;
	struct recording *saving;
	int exit_registered;

	struct pixel_buffer buffers[PBO_COUNT];
	int next_buffer;
	int read_width;
	int read_height;
	Uint64 presents;
	Uint64 start_ticks;
	Uint64 scheduled;
	int carry;
	double limit_seconds;

	/* for the log */
	Uint64 dropped;
	Uint64 reads;
	Uint64 capture_counter;
	Uint64 capture_counter_maximum;
	Uint64 capture_presents;

	/* debug.record_seconds: once, at the first frame of play */
	int automatic_done;
} capture;

void capture_request_recording_toggle(void)
{
	SDL_SetAtomicInt(&capture.toggle_requested, 1);
}

void capture_audio(const float *samples, unsigned int frames, unsigned int channels, unsigned int rate)
{
	struct recording *recording;

	if (!audio_lock || channels != AUDIO_CHANNELS || rate != AUDIO_RATE)
		return;
	SDL_LockMutex(audio_lock);
	recording = audio_recording;
	if (recording)
	{
		size_t room = AUDIO_RING_FRAMES - recording->audio_count;
		size_t take = frames < room ? frames : room;
		size_t write = (recording->audio_read + recording->audio_count) % AUDIO_RING_FRAMES;
		size_t first = AUDIO_RING_FRAMES - write;

		if (first > take)
			first = take;
		memcpy(recording->audio_ring + write * AUDIO_CHANNELS, samples, first * AUDIO_CHANNELS * sizeof(float));
		memcpy(recording->audio_ring, samples + first * AUDIO_CHANNELS,
			(take - first) * AUDIO_CHANNELS * sizeof(float));
		recording->audio_count += take;
		recording->audio_lost += frames - take;
	}
	SDL_UnlockMutex(audio_lock);
}

/* the last recording's writer, once it is done (wait: until it is) */
static void recording_reap(int wait)
{
	struct recording *saving = capture.saving;

	if (!saving || (!wait && !SDL_GetAtomicInt(&saving->finished)))
		return;
	SDL_WaitThread(saving->thread, NULL);
	recording_free(saving);
	capture.saving = NULL;
}

static void recording_stop(int harvest);

/* quitting: the recording stopped, and saved before the game goes */
static void capture_exit(void)
{
	if (capture.active)
		recording_stop(0);
	recording_reap(1);
	{
		int waited;

		for (waited = 0; waited < 200 && SDL_GetAtomicInt(&screenshots_writing) > 0; waited++)
			SDL_Delay(10);
	}
}

static int record_quality_crf(void)
{
	const char *quality = config_string("capture.record_quality");

	if (!strcmp(quality, "low"))
		return 28;
	if (!strcmp(quality, "high"))
		return 18;
	return 23;
}

static void recording_start(int width, int height)
{
	struct recording *recording;
	char folder[PATH_SIZE], base[256];
	int fps = (int)config_integer("capture.record_fps");
	int slot;

	recording_reap(0);
	if (capture.saving)
	{
		notice(2, "RECORDING STILL SAVING");
		return;
	}
	recording = calloc(1, sizeof(*recording));
	if (!recording)
		return;
	if (!ffmpeg_find(recording->ffmpeg, sizeof(recording->ffmpeg)))
	{
		platform_log("capture: no ffmpeg (on the PATH, beside the game, or capture.ffmpeg_path)");
		notice(2, "RECORDING NEEDS FFMPEG");
		free(recording);
		return;
	}
	/* (H.264 in 4:2:0 wants even sizes: the last odd row or column is left
	out) */
	recording->width = width & ~1;
	recording->height = height & ~1;
	recording->fps = fps < 1 ? 60 : fps > 240 ? 240 : fps;
	recording->crf = record_quality_crf();
	recording->frame_bytes = (size_t)recording->width * (size_t)recording->height * 4;
	recording->slot_count = (int)(QUEUE_BUDGET_BYTES / recording->frame_bytes);
	if (recording->slot_count < QUEUE_MINIMUM)
		recording->slot_count = QUEUE_MINIMUM;
	if (recording->slot_count > QUEUE_MAXIMUM)
		recording->slot_count = QUEUE_MAXIMUM;
	for (slot = 0; slot < recording->slot_count; slot++)
	{
		recording->slots[slot] = malloc(recording->frame_bytes);
		if (!recording->slots[slot])
			break;
		recording->free_slots[recording->free_count++] = slot;
	}
	recording->slot_count = slot;
	recording->audio_ring = malloc(sizeof(float) * AUDIO_RING_FRAMES * AUDIO_CHANNELS);
	recording->lock = SDL_CreateMutex();
	recording->wake = SDL_CreateCondition();
	if (!audio_lock)
		audio_lock = SDL_CreateMutex();
	if (recording->slot_count < QUEUE_MINIMUM || !recording->audio_ring || !recording->lock || !recording->wake ||
		!audio_lock)
	{
		notice(2, "RECORDING FAILED");
		recording_free(recording);
		return;
	}
	capture_folder("recordings", config_string("capture.record_directory"), folder, sizeof(folder));
	capture_name(folder, ".mp4", recording->final_path, sizeof(recording->final_path), base, sizeof(base));
	snprintf(recording->video_path, sizeof(recording->video_path), "%s/%s.video.mp4", folder, base);
	snprintf(recording->audio_path, sizeof(recording->audio_path), "%s/%s.audio.f32", folder, base);

	capture.read_width = width < recording->width ? width : recording->width;
	capture.read_height = height < recording->height ? height : recording->height;
	for (slot = 0; slot < PBO_COUNT; slot++)
	{
		glGenBuffers(1, &capture.buffers[slot].id);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, capture.buffers[slot].id);
		glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)recording->frame_bytes, NULL, GL_STREAM_READ);
		capture.buffers[slot].pending = 0;
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	capture.next_buffer = 0;
	capture.start_ticks = SDL_GetTicksNS();
	capture.scheduled = 0;
	capture.carry = 0;
	capture.dropped = 0;
	capture.reads = 0;
	capture.capture_counter = 0;
	capture.capture_counter_maximum = 0;
	capture.capture_presents = 0;

	recording->thread = SDL_CreateThread(recording_thread, "recording", recording);
	if (!recording->thread)
	{
		for (slot = 0; slot < PBO_COUNT; slot++)
			glDeleteBuffers(1, &capture.buffers[slot].id);
		notice(2, "RECORDING FAILED");
		recording_free(recording);
		return;
	}
	SDL_LockMutex(audio_lock);
	audio_recording = recording;
	SDL_UnlockMutex(audio_lock);
	capture.active = recording;
	if (!capture.exit_registered)
	{
		atexit(capture_exit);
		capture.exit_registered = 1;
	}
	platform_log("capture: recording %s (%dx%d at %d fps, crf %d, %d frames queued at most, ffmpeg %s)",
		recording->final_path, recording->width, recording->height, recording->fps, recording->crf,
		recording->slot_count, recording->ffmpeg);
}

/* a pixel buffer's frame into the queue (or dropped, its time given to the
next one queued) */
static void buffer_harvest(struct pixel_buffer *buffer)
{
	struct recording *recording = capture.active;
	int repeat = buffer->repeat + capture.carry;
	int slot = -1;
	const unsigned char *pixels;

	buffer->pending = 0;
	SDL_LockMutex(recording->lock);
	if (recording->free_count)
		slot = recording->free_slots[--recording->free_count];
	SDL_UnlockMutex(recording->lock);
	if (slot < 0)
	{
		capture.carry = repeat;
		capture.dropped += (Uint64)buffer->repeat;
		return;
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->id);
	pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0,
		(GLsizeiptr)capture.read_width * capture.read_height * 4, GL_MAP_READ_BIT);
	if (pixels)
	{
		unsigned char *frame = recording->slots[slot];
		int x0 = (recording->width - capture.read_width) / 2, y0 = (recording->height - capture.read_height) / 2;
		int row;

		if (capture.read_width != recording->width || capture.read_height != recording->height)
			memset(frame, 0, recording->frame_bytes);
		for (row = 0; row < capture.read_height; row++)
		{
			memcpy(frame + ((size_t)(row + y0) * (size_t)recording->width + (size_t)x0) * 4,
				pixels + (size_t)row * (size_t)capture.read_width * 4, (size_t)capture.read_width * 4);
		}
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	SDL_LockMutex(recording->lock);
	if (pixels)
	{
		int index = (recording->queue_head + recording->queue_count) % QUEUE_MAXIMUM;

		recording->queued[index] = slot;
		recording->queued_repeat[index] = repeat;
		recording->queue_count++;
		capture.carry = 0;
		SDL_SignalCondition(recording->wake);
	}
	else
	{
		recording->free_slots[recording->free_count++] = slot;
		capture.carry = repeat;
		capture.dropped += (Uint64)buffer->repeat;
	}
	SDL_UnlockMutex(recording->lock);
}

static void recording_stop(int harvest)
{
	struct recording *recording = capture.active;
	int index;

	for (index = 0; index < PBO_COUNT; index++)
	{
		/* (the oldest first) */
		struct pixel_buffer *buffer = &capture.buffers[(capture.next_buffer + index) % PBO_COUNT];

		if (buffer->pending && harvest)
			buffer_harvest(buffer);
		buffer->pending = 0;
		if (harvest)
			glDeleteBuffers(1, &buffer->id);
		buffer->id = 0;
	}
	SDL_LockMutex(recording->lock);
	/* (time a dropped last frame stood for, given to the one before) */
	if (capture.carry && recording->queue_count)
		recording->queued_repeat[(recording->queue_head + recording->queue_count - 1) % QUEUE_MAXIMUM] += capture.carry;
	recording->stopping = 1;
	SDL_SignalCondition(recording->wake);
	SDL_UnlockMutex(recording->lock);
	{
		double frequency = (double)SDL_GetPerformanceFrequency();
		double seconds = (double)(SDL_GetTicksNS() - capture.start_ticks) / 1e9;

		platform_log("capture: stopped after %.2f s: %llu frames due, %llu reads, %llu dropped; the game thread spent "
			"%.3f ms a present on it (at most %.3f ms) over %llu presents",
			seconds, (unsigned long long)capture.scheduled, (unsigned long long)capture.reads,
			(unsigned long long)capture.dropped,
			capture.capture_presents ? 1000.0 * (double)capture.capture_counter / frequency /
				(double)capture.capture_presents : 0.0,
			1000.0 * (double)capture.capture_counter_maximum / frequency,
			(unsigned long long)capture.capture_presents);
	}
	capture.saving = recording;
	capture.active = NULL;
}

/* the frames due by now: read the picture once for all of them */
static void recording_frame(unsigned int framebuffer, int width, int height)
{
	struct recording *recording = capture.active;
	Uint64 elapsed = SDL_GetTicksNS() - capture.start_ticks;
	Uint64 due = elapsed * (Uint64)recording->fps / SDL_NS_PER_SECOND + 1;
	int index;

	capture.presents++;
	/* reads two presents old: done by now */
	for (index = 0; index < PBO_COUNT; index++)
	{
		struct pixel_buffer *buffer = &capture.buffers[(capture.next_buffer + index) % PBO_COUNT];

		if (buffer->pending && capture.presents - buffer->issued >= 2)
			buffer_harvest(buffer);
	}
	if (due > capture.scheduled)
	{
		struct pixel_buffer *buffer = &capture.buffers[capture.next_buffer];
		int read_width = width < capture.read_width ? width : capture.read_width;
		int read_height = height < capture.read_height ? height : capture.read_height;

		if (buffer->pending)
			buffer_harvest(buffer);
		/* (the picture's size changed: the middle of it, as at the start) */
		if (read_width == capture.read_width && read_height == capture.read_height)
		{
			glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->id);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
			glReadPixels((width - read_width) / 2, (height - read_height) / 2, read_width, read_height, GL_BGRA,
				GL_UNSIGNED_BYTE, NULL);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			buffer->pending = 1;
			buffer->repeat = (int)(due - capture.scheduled);
			buffer->issued = capture.presents;
			capture.next_buffer = (capture.next_buffer + 1) % PBO_COUNT;
			capture.reads++;
		}
		else
		{
			/* (smaller than at the start: the frame is the last one again) */
			capture.carry += (int)(due - capture.scheduled);
			capture.dropped += due - capture.scheduled;
		}
		capture.scheduled = due;
	}
	if (capture.limit_seconds > 0.0 && (double)elapsed / 1e9 >= capture.limit_seconds)
	{
		platform_log("capture: %.1f seconds recorded (debug.record_seconds)", capture.limit_seconds);
		recording_stop(1);
		notice(1, "RECORDING STOPPED");
	}
}

void capture_frame(unsigned int framebuffer, int width, int height)
{
	Uint64 start = SDL_GetPerformanceCounter();
	int recording = capture.active != NULL;

	if (width <= 0 || height <= 0)
		return;
	if (SDL_GetAtomicInt(&screenshot_requested))
	{
		SDL_SetAtomicInt(&screenshot_requested, 0);
		screenshot_take(framebuffer, width, height);
	}
	if (SDL_GetAtomicInt(&capture.toggle_requested))
	{
		SDL_SetAtomicInt(&capture.toggle_requested, 0);
		if (capture.active)
		{
			recording_stop(1);
			notice(1, "RECORDING STOPPED");
		}
		else
		{
			capture.limit_seconds = 0.0;
			recording_start(width, height);
		}
	}
	/* debug.record_seconds: from the first frame of play */
	if (!capture.automatic_done && !capture.active)
	{
		double seconds = config_real("debug.record_seconds");

		if (seconds <= 0.0)
		{
			capture.automatic_done = 1;
		}
		else if (capture_game_playing())
		{
			capture.automatic_done = 1;
			recording_start(width, height);
			capture.limit_seconds = seconds;
		}
	}
	if (capture.active)
	{
		recording_frame(framebuffer, width, height);
		if (recording)
		{
			Uint64 spent = SDL_GetPerformanceCounter() - start;

			capture.capture_counter += spent;
			if (spent > capture.capture_counter_maximum)
				capture.capture_counter_maximum = spent;
			capture.capture_presents++;
		}
	}
	recording_reap(0);
}

/* ---------- the red dot */

void capture_present_overlay(int x, int y, int width, int height)
{
	int radius, center_x, center_y, row;

	if (!capture.active || !config_boolean("capture.record_indicator"))
		return;
	/* (steady for 0.75 s of each second) */
	if ((SDL_GetTicks() - capture.start_ticks / 1000000) % 1000 >= 750)
		return;
	radius = height / 60;
	if (radius < 5)
		radius = 5;
	center_x = x + width - radius * 3;
	center_y = y + height - radius * 3;
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.85f, 0.05f, 0.05f, 1.0f);
	for (row = -radius; row <= radius; row++)
	{
		int half = (int)SDL_sqrt((double)(radius * radius - row * row));

		glScissor(center_x - half, center_y + row, half * 2 + 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_SCISSOR_TEST);
}

#endif
