/*
CAPTURE.C

Screenshots and video recording (capture.h), on the desktop builds.

A screenshot (F9) reads the back buffer at the next present, before the red
dot is drawn over the window. The game thread makes its file at once, a
name nothing else has (screenshots/<date>_<time>_<map>.png beside
config.toml, made empty with an exclusive create), and the screenshot
thread encodes the PNG (our own encoder below: PNG's filters and deflate
with the fixed Huffman codes) and writes it. The debug screenshots
(debug.screenshot_format = "png") go through the same thread; none is ever
encoded on the game thread.

A recording (F10) pipes raw frames to an ffmpeg child (capture_child.h)
that encodes H.264 into recordings/<name>.video.mp4, at a fixed rate
(capture.record_fps, 30 or 60): each present, the frames due by the wall
clock since the start are counted, and the picture is read once for all of
them (a slow frame stands for several, a fast frame none is due for is
skipped), so the video plays at real speed whatever the game's frame rate.
The read goes into a ring of pixel buffer objects and is copied out two
presents later into a bounded queue of frames that a writer thread feeds to
ffmpeg. The game thread never waits for ffmpeg: with the queue full, the
frame is dropped (counted, and its time given to the next frame queued, so
the timeline stays real). Pixel buffers and queue fit a memory budget, or
the recording does not start. The picture keeps the size it had at the
start: a window grown since is cut to its middle, one shrunk is centred on
black.

The mixer's output (dsound_sdl.c) goes, from the audio thread and without
a lock (one writer, one reader), into a ring that the writer thread drains
to a raw float file beside the video; sound the ring had no room for is
written as silence where it was lost. When the recording stops the writer
closes ffmpeg's input and runs ffmpeg again to put the two together (the
video copied, the sound encoded as AAC) into <name>.mp4, and removes the
parts.

If ffmpeg cannot start or dies, the recording stops at once ("RECORDING
FAILED", the dot gone). An ffmpeg that takes no frame for 10 seconds, or
does not finish in time, is killed: only the child the capture started.
Quitting (capture_shutdown) gives a recording being saved 30 seconds in
all (its encoder's end and the mux), after which its writer kills its
ffmpeg and removes its files, and the screenshots 5; a thread that still
does not end keeps its files, left as .part files, which a later start
removes (capture_parts_clean). Everything being written is a .part file
until it is whole.

Notices ("SCREENSHOT SAVED", "RECORDING NEEDS FFMPEG", ...) are drawn by the
game for 1.5 seconds (halo_capture_notice, main.c), and are in the frames
recorded then; the red dot is not.
*/

#include "platform.h"
#include "port_config.h"
#include "capture.h"

#include <stdarg.h>
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

int capture_png_queue_bgra(const char *path, unsigned char *pixels, int width, int height)
{
	(void)path;
	(void)pixels;
	(void)width;
	(void)height;
	return -1;
}

void capture_shutdown(void)
{
}

void capture_resume(void)
{
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
#include "capture_child.h"

#include <SDL3/SDL.h>
#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/* the game (port/linux/game/capture_game.c) */
int capture_game_playing(void);
void capture_game_map_name(char *name, int size);

#define AUDIO_RATE 48000
#define AUDIO_CHANNELS 2
/* the audio ring: 2^18 frames, 5.5 seconds; and the places sound was lost */
#define AUDIO_RING_FRAMES (1u << 18)
#define AUDIO_EVENTS 64u
#define PBO_COUNT 3
/* the memory a recording's frames take, pixel buffers and queue together:
the 32-bit builds have less address space to spare */
#define RECORDING_BUDGET_BYTES (sizeof(void *) >= 8 ? (size_t)160 << 20 : (size_t)48 << 20)
#define QUEUE_MINIMUM 2
#define QUEUE_MAXIMUM 12
/* the screenshots waiting for the screenshot thread */
#define SHOT_QUEUE_MAXIMUM 8
#define SHOT_QUEUE_32BIT 4
/* the largest picture taken, each way */
#define MAXIMUM_DIMENSION 16384
#define NOTICE_MS 1500
#define PATH_SIZE 1024
/* an ffmpeg that takes no frame for this long is killed */
#define STALL_MS 10000
/* the encoder's end, once its input is closed */
#define ENCODER_END_MS 30000
/* quitting: how long a recording being saved, and the screenshots, are
waited for, and the threads' end after they are given up */
#define SHUTDOWN_RECORDING_MS 30000
#define SHUTDOWN_SCREENSHOTS_MS 5000
#define SHUTDOWN_CANCEL_MS 3000
/* a .part file, or a name made and never filled, untouched this long is
one an earlier session left (capture_parts_clean) */
#define STALE_SECONDS 600

/* quitting has begun (capture_shutdown): nothing new is taken */
static int capture_shut_down;

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

/* ---------- sizes and paths */

/* a * b * c, if it fits; 0 if not (or if any is 0) */
static int size_product(size_t *result, size_t a, size_t b, size_t c)
{
	if (!a || !b || !c || a > SDL_SIZE_MAX / b || a * b > SDL_SIZE_MAX / c)
		return 0;
	*result = a * b * c;
	return 1;
}

/* the bytes of a width by height picture of 4 bytes a pixel, if it is one
the capture takes; 0 if not */
static int picture_bytes(size_t *result, int width, int height)
{
	if (width <= 0 || height <= 0 || width > MAXIMUM_DIMENSION || height > MAXIMUM_DIMENSION)
		return 0;
	return size_product(result, (size_t)width, (size_t)height, 4);
}

/* snprintf into a path, 0 if it does not fit */
static int path_format(char *path, size_t size, const char *format, ...)
{
	va_list arguments;
	int length;

	va_start(arguments, format);
	length = vsnprintf(path, size, format, arguments);
	va_end(arguments);
	if (length < 0 || (size_t)length >= size)
	{
		path[0] = 0;
		return 0;
	}
	return 1;
}

#ifndef _WIN32
static void descriptor_close_on_exec(Sint64 descriptor)
{
	if (descriptor >= 0)
	{
		int flags = fcntl((int)descriptor, F_GETFD);

		if (flags != -1)
			fcntl((int)descriptor, F_SETFD, flags | FD_CLOEXEC);
	}
}
#endif

/* a file to write, never handed to a child (ffmpeg); Windows' are not
inherited in any case */
static SDL_IOStream *file_create(const char *path)
{
	SDL_IOStream *file = SDL_IOFromFile(path, "wb");

#ifndef _WIN32
	if (file)
	{
		SDL_PropertiesID properties = SDL_GetIOProperties(file);
		FILE *stdio = SDL_GetPointerProperty(properties, SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, NULL);

		descriptor_close_on_exec(stdio ? fileno(stdio) :
			SDL_GetNumberProperty(properties, SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, -1));
	}
#endif
	return file;
}

/* ---------- the child process: Linux and macOS (Windows':
port/windows/src/win32_capture.c) */

#ifndef _WIN32

struct capture_child
{
	SDL_Process *process;
	SDL_IOStream *input;
};

struct capture_child *capture_child_start(const char *const *arguments, int with_input)
{
	SDL_PropertiesID properties = SDL_CreateProperties();
	struct capture_child *child = calloc(1, sizeof(*child));

	if (!child || !properties)
	{
		free(child);
		if (properties)
			SDL_DestroyProperties(properties);
		return NULL;
	}
	SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)arguments);
	SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,
		with_input ? SDL_PROCESS_STDIO_APP : SDL_PROCESS_STDIO_NULL);
	SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
	SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_INHERITED);
	child->process = SDL_CreateProcessWithProperties(properties);
	SDL_DestroyProperties(properties);
	if (!child->process)
	{
		platform_log("capture: cannot start %s (%s)", arguments[0], SDL_GetError());
		free(child);
		return NULL;
	}
	if (with_input)
	{
		child->input = SDL_GetProcessInput(child->process);
		if (child->input)
		{
			/* SDL hands the pipe over without waiting (O_NONBLOCK): the
			writer thread can wait, and a larger pipe takes a frame in fewer
			turns; and no later child (the mux) gets it */
			Sint64 descriptor = SDL_GetNumberProperty(SDL_GetIOProperties(child->input),
				SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, -1);

			if (descriptor >= 0)
			{
				int flags = fcntl((int)descriptor, F_GETFL);

				if (flags != -1)
					fcntl((int)descriptor, F_SETFL, flags & ~O_NONBLOCK);
#ifdef F_SETPIPE_SZ
				fcntl((int)descriptor, F_SETPIPE_SZ, 1 << 20);
#endif
				descriptor_close_on_exec(descriptor);
			}
		}
	}
	return child;
}

int capture_child_write(struct capture_child *child, const void *data, size_t size)
{
	const unsigned char *bytes = data;

	if (!child->input)
		return 0;
	while (size)
	{
		size_t written = SDL_WriteIO(child->input, bytes, size);

		bytes += written;
		size -= written;
		if (!size)
			break;
		if (SDL_GetIOStatus(child->input) != SDL_IO_STATUS_NOT_READY)
			return 0;
		/* (a pipe that did not become a waiting one) */
		SDL_DelayNS(200000);
	}
	return 1;
}

void capture_child_close_input(struct capture_child *child)
{
	if (child->input)
	{
		SDL_CloseIO(child->input);
		child->input = NULL;
	}
}

int capture_child_wait(struct capture_child *child, int timeout_ms, int *exit_code)
{
	Uint64 deadline = SDL_GetTicks() + (Uint64)(timeout_ms < 0 ? 0 : timeout_ms);

	for (;;)
	{
		if (SDL_WaitProcess(child->process, false, exit_code))
			return 1;
		if (timeout_ms >= 0 && SDL_GetTicks() >= deadline)
			return 0;
		SDL_Delay(10);
	}
}

void capture_child_kill(struct capture_child *child)
{
	SDL_KillProcess(child->process, true);
}

void capture_child_free(struct capture_child *child)
{
	if (child->input)
		SDL_CloseIO(child->input);
	SDL_DestroyProcess(child->process);
	free(child);
}

int capture_file_reserve(const char *path)
{
	int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);

	if (descriptor < 0)
		return errno == EEXIST ? 0 : -1;
	close(descriptor);
	return 1;
}

int capture_file_executable(const char *path)
{
	int descriptor;

	if (access(path, X_OK) != 0)
		return 0;
	/* (not a folder; asked so because struct stat is laid out otherwise
	under the 32-bit build's ABI, posix.h) */
	descriptor = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (descriptor >= 0)
	{
		close(descriptor);
		return 0;
	}
	return 1;
}

#endif

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
	size_t capacity;
	unsigned char *data;

	if (buffer->failed || buffer->size + more <= buffer->capacity)
		return;
	if (more > SDL_SIZE_MAX / 4 - buffer->size)
	{
		buffer->failed = 1;
		return;
	}
	capacity = buffer->capacity ? buffer->capacity : 65536;
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
hash chain over the last 32 KB; 1 on success, 0 on failure or once cancel
is set */
static int zlib_compress(const unsigned char *data, size_t size, struct byte_buffer *out, SDL_AtomicInt *cancel)
{
	struct bit_writer writer = { out, 0, 0 };
	Sint32 *head, *previous;
	size_t position = 0, next_check = 0;
	unsigned char header[2] = { 0x78, 0x01 };
	uLong adler;

	/* (positions are Sint32: a stream past 2 GB is not taken) */
	if (size > 0x7fffffffu)
		return 0;
	head = malloc(sizeof(Sint32) * HASH_SIZE);
	previous = malloc(sizeof(Sint32) * WINDOW_SIZE);
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

		if (position >= next_check)
		{
			if (SDL_GetAtomicInt(cancel))
				break;
			next_check = position + 65536;
		}
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

				if ((size_t)best_length < limit && a[best_length] == b[best_length])
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
	free(head);
	free(previous);
	if (position < size)
		return 0;
	put_literal_length(&writer, 256);
	if (writer.count)
		bits_put(&writer, 0, 8 - writer.count);
	adler = z_adler32(0L, Z_NULL, 0);
	adler = z_adler32(adler, data, (uInt)size);
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
filters gives the smallest sum of magnitudes (the usual heuristic); 0 on
failure or once cancel is set */
static int png_encode(const unsigned char *bgra, int width, int height, struct byte_buffer *out,
	SDL_AtomicInt *cancel)
{
	size_t row_bytes = 0, stride = 0, filtered_size = 0;
	unsigned char *filtered = NULL;
	unsigned char *rows[2] = { NULL, NULL }, *trial[5] = { NULL, NULL, NULL, NULL, NULL };
	struct byte_buffer compressed = { 0 };
	int y, x, filter, ok;
	static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	unsigned char header[13];

	ok = width > 0 && height > 0 && width <= MAXIMUM_DIMENSION && height <= MAXIMUM_DIMENSION &&
		size_product(&row_bytes, (size_t)width, 3, 1) &&
		size_product(&filtered_size, row_bytes + 1, (size_t)height, 1);
	if (ok)
	{
		stride = row_bytes + 1;
		filtered = malloc(filtered_size);
		rows[0] = calloc(row_bytes, 1);
		rows[1] = malloc(row_bytes);
		for (filter = 0; filter < 5; filter++)
			trial[filter] = malloc(row_bytes);
		ok = filtered && rows[0] && rows[1];
		for (filter = 0; filter < 5; filter++)
			ok = ok && trial[filter];
	}
	for (y = 0; ok && y < height; y++)
	{
		const unsigned char *source = bgra + (size_t)y * (size_t)width * 4;
		/* (rows[] alternate: this row's, the one above's) */
		unsigned char *row = rows[y & 1];
		unsigned char *above = rows[(y + 1) & 1];
		unsigned int best_sum = 0xffffffffu;
		int best = 0;

		if ((y & 63) == 0 && SDL_GetAtomicInt(cancel))
		{
			ok = 0;
			break;
		}
		if (y == 0)
			memset(above, 0, row_bytes);
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
		memcpy(filtered + (size_t)y * stride + 1, trial[best], row_bytes);
	}
	if (ok)
		ok = zlib_compress(filtered, filtered_size, &compressed, cancel);
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

/* ---------- names and folders */

/* <config.toml's folder><name>, or override if set; made if missing; 0 if
the path is too long */
static int capture_folder(const char *name, const char *override, char *path, size_t size)
{
	if (override && *override)
	{
		if (!path_format(path, size, "%s", override))
			return 0;
	}
	else
	{
		config_folder(path, size);
		if (SDL_strlcat(path, name, size) >= size)
			return 0;
	}
	SDL_CreateDirectory(path);
	return 1;
}

/* the names the capture makes (capture_name_reserve), which
capture_name_owned knows again: CAPTURE_STAMP_LENGTH characters of date and
time ("2026-10-07_21-05-12"), then "_" and the map's name, the two cut to
CAPTURE_NAME_STAMP_SIZE - 1 characters, then "_2" to "_99" for a name taken;
so what comes between the date and time and the extension is at most
CAPTURE_NAME_MIDDLE_MAXIMUM characters */
#define CAPTURE_STAMP_LENGTH 19
#define CAPTURE_NAME_STAMP_SIZE 64
#define CAPTURE_NAME_NUMBER_MAXIMUM 99
#define CAPTURE_NAME_MIDDLE_MAXIMUM (CAPTURE_NAME_STAMP_SIZE - 1 - CAPTURE_STAMP_LENGTH + 3 /* "_99" */)

/* names folder/<date>_<time>[_<map>][_<n>]<extension> for each of
extensions and makes them, empty, here and now (an exclusive create) so
that nothing else takes them: paths[i] each, and base without the
extension. 0 if no number up to 99 is free, or a path is too long or
cannot be made (*failure the notice). */
static int capture_name_reserve(const char *folder, const char *const *extensions, int count,
	char (*paths)[PATH_SIZE], char *base, size_t base_size, const char **failure)
{
	SDL_Time now = 0;
	SDL_DateTime date;
	char stamp[CAPTURE_NAME_STAMP_SIZE], map[64];
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
	for (number = 1; number <= CAPTURE_NAME_NUMBER_MAXIMUM; number++)
	{
		int index, made = 0, taken = 0;

		if (!(number == 1 ? path_format(base, base_size, "%s", stamp) :
			path_format(base, base_size, "%s_%d", stamp, number)))
		{
			*failure = "CAPTURE PATH TOO LONG";
			return 0;
		}
		for (index = 0; index < count; index++)
		{
			int result;

			if (!path_format(paths[index], PATH_SIZE, "%s/%s%s", folder, base, extensions[index]))
			{
				*failure = "CAPTURE PATH TOO LONG";
				result = -1;
			}
			else
			{
				result = capture_file_reserve(paths[index]);
				if (result < 0)
				{
					platform_log("capture: cannot make %s", paths[index]);
					*failure = "CAPTURE FOLDER NOT WRITABLE";
				}
			}
			if (result <= 0)
			{
				/* (the names made of this number given back) */
				while (made > 0)
					SDL_RemovePath(paths[--made]);
				if (result < 0)
					return 0;
				taken = 1;
				break;
			}
			made++;
		}
		if (!taken)
			return 1;
	}
	platform_log("capture: %s to %s_99 are all taken in %s", stamp, stamp, folder);
	*failure = "CAPTURE NAMES ALL TAKEN";
	return 0;
}

/* program on the PATH (".exe" added on Windows if it has no extension);
1 if found, an executable file */
static int ffmpeg_search(const char *program, char *path, size_t size)
{
#ifdef _WIN32
	const char separator = ';';
	const char *extension = strchr(program, '.') ? "" : ".exe";
#else
	const char separator = ':';
	const char *extension = "";
#endif
	const char *search = SDL_getenv("PATH");

	while (search && *search)
	{
		const char *end = strchr(search, separator);
		const char *entry = search;
		size_t length = end ? (size_t)(end - search) : strlen(search);

#ifdef _WIN32
		/* (an entry in quotes, as "C:\Program Files\ffmpeg\bin") */
		if (length >= 2 && entry[0] == '"' && entry[length - 1] == '"')
		{
			entry++;
			length -= 2;
		}
#endif
		if (length && path_format(path, size, "%.*s/%s%s", (int)length, entry, program, extension) &&
			capture_file_executable(path))
			return 1;
		search = end ? end + 1 : NULL;
	}
	return 0;
}

/* ffmpeg: capture.ffmpeg_path (a path, or a bare name looked for on the
PATH), else beside the game, else on the PATH; 1 if found, an executable
file */
static int ffmpeg_find(char *path, size_t size)
{
	const char *configured = config_string("capture.ffmpeg_path");
	const char *base = SDL_GetBasePath();

	if (*configured)
	{
		if (!strchr(configured, '/') && !strchr(configured, '\\'))
			return ffmpeg_search(configured, path, size);
		if (path_format(path, size, "%s", configured) && capture_file_executable(path))
			return 1;
#ifdef _WIN32
		{
			/* (the name after the last separator, either kind) */
			const char *name = configured, *at;

			for (at = configured; *at; at++)
			{
				if (*at == '\\' || *at == '/')
					name = at + 1;
			}
			if (!strchr(name, '.') && path_format(path, size, "%s.exe", configured) &&
				capture_file_executable(path))
				return 1;
		}
#endif
		return 0;
	}
#ifdef _WIN32
	if (base && path_format(path, size, "%sffmpeg.exe", base) && capture_file_executable(path))
		return 1;
#else
	if (base && path_format(path, size, "%sffmpeg", base) && capture_file_executable(path))
		return 1;
#endif
	return ffmpeg_search("ffmpeg", path, size);
}

/* ---------- the screenshot thread */

struct shot_job
{
	unsigned char *pixels;
	int width;
	int height;
	/* F9's: its file was made (capture_name_reserve), and a notice tells how
	it went */
	int reserved;
	char path[PATH_SIZE];
	/* written here first, then renamed to path: a file left half written
	is a .part (capture_parts_clean) */
	char part[PATH_SIZE];
};

static struct
{
	int started;
	SDL_Mutex *lock;
	SDL_Condition *wake;
	SDL_Condition *idle;
	SDL_Thread *thread;
	/* (lock) the queue */
	struct shot_job jobs[SHOT_QUEUE_MAXIMUM];
	int head;
	int count;
	/* (lock) the job being written, which only the thread touches */
	int busy;
	char busy_path[PATH_SIZE];
	int stopping;
	SDL_AtomicInt cancel;
	SDL_AtomicInt exited;
	unsigned long skipped;
} shots;

static SDL_AtomicInt screenshot_requested;

/* the capture's folders (capture_initialize), for capture_parts_clean */
static char clean_folders[3][PATH_SIZE];
static int clean_folder_count;

/* whether name is one the capture makes, whole: <date>_<time>
("2026-10-07_21-05-12"), then nothing or "_" and the map's name and the
number (capture_name_reserve; letters, digits, "-" and "_" only), then one
of its suffixes. 1: a part (.part); 2: a name made, whose file is only the
capture's while empty; 0: anything else. */
static int capture_name_owned(const char *name)
{
	/* (longest first: ".mp4.part" ends ".video.mp4.part" too) */
	static const char *const suffixes[] = { ".video.mp4.part", ".audio.f32.part", ".png.part", ".mp4.part", ".png", ".mp4" };
	static const int kinds[] = { 1, 1, 1, 1, 2, 2 };
	static const char stamp[CAPTURE_STAMP_LENGTH + 1] = "0000-00-00_00-00-00";
	size_t length = strlen(name), stamp_length = sizeof(stamp) - 1, index, at;

	if (length <= stamp_length)
		return 0;
	for (at = 0; at < stamp_length; at++)
	{
		if (stamp[at] == '0' ? name[at] < '0' || name[at] > '9' : name[at] != stamp[at])
			return 0;
	}
	for (index = 0; index < sizeof(suffixes) / sizeof(suffixes[0]); index++)
	{
		size_t suffix_length = strlen(suffixes[index]);
		size_t middle = length - stamp_length - suffix_length;
		int whole = 1;

		if (length < stamp_length + suffix_length || strcmp(name + length - suffix_length, suffixes[index]))
			continue;
		/* (nothing, or "_" and the map name's characters and the number, as
		capture_name_reserve makes them: CAPTURE_NAME_MIDDLE_MAXIMUM at most) */
		if (middle)
		{
			whole = middle >= 2 && middle <= CAPTURE_NAME_MIDDLE_MAXIMUM && name[stamp_length] == '_';
			for (at = stamp_length + 1; whole && at < stamp_length + middle; at++)
			{
				char c = name[at];

				whole = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
					c == '_';
			}
		}
		if (whole)
			return kinds[index];
	}
	return 0;
}

/* in one of the capture's own folders, what an earlier session left: its
.part files, and names it made and never filled, untouched for
STALE_SECONDS (a recording or screenshot under way, in this game or
another, writes its .part all the time); nothing else (capture_name_owned,
capture_folder_clean) */
static void capture_parts_clean(const char *folder)
{
	int removed = capture_folder_clean(folder, STALE_SECONDS, capture_name_owned);

	if (removed > 0)
		platform_log("capture: removed %d unfinished files an earlier session left in %s", removed, folder);
	else if (removed < 0)
		platform_log("capture: %s not cleared of an earlier session's files (a link, or not readable)", folder);
}

static int SDLCALL screenshot_thread(void *parameter)
{
	int index;

	(void)parameter;
	for (index = 0; index < clean_folder_count; index++)
		capture_parts_clean(clean_folders[index]);
	SDL_LockMutex(shots.lock);
	for (;;)
	{
		struct shot_job job;
		struct byte_buffer png = { 0 };
		Uint64 start;
		int ok, opened = 0;

		while (!shots.count && !shots.stopping)
			SDL_WaitCondition(shots.wake, shots.lock);
		if (!shots.count || SDL_GetAtomicInt(&shots.cancel))
			break;
		job = shots.jobs[shots.head];
		shots.head = (shots.head + 1) % SHOT_QUEUE_MAXIMUM;
		shots.count--;
		shots.busy = 1;
		SDL_strlcpy(shots.busy_path, job.path, sizeof(shots.busy_path));
		SDL_UnlockMutex(shots.lock);

		start = SDL_GetTicks();
		ok = png_encode(job.pixels, job.width, job.height, &png, &shots.cancel);
		if (ok && !SDL_GetAtomicInt(&shots.cancel))
		{
			SDL_IOStream *file = file_create(job.part);

			opened = 1;
			ok = file && SDL_WriteIO(file, png.data, png.size) == png.size;
			if (file && !SDL_CloseIO(file))
				ok = 0;
			/* (over the name made, or an older debug frame's) */
			if (ok)
				SDL_RemovePath(job.path);
			ok = ok && SDL_RenamePath(job.part, job.path);
		}
		else
		{
			ok = 0;
		}
		free(png.data);
		free(job.pixels);
		if (ok)
		{
			platform_log("capture: screenshot %s (%dx%d, %u ms)", job.path, job.width, job.height,
				(unsigned)(SDL_GetTicks() - start));
			if (job.reserved)
				notice(1, "SCREENSHOT SAVED");
		}
		else
		{
			/* (no partial file left: the one begun, and the name made) */
			if (opened)
				SDL_RemovePath(job.part);
			if (job.reserved)
				SDL_RemovePath(job.path);
			if (!SDL_GetAtomicInt(&shots.cancel))
			{
				platform_log("capture: cannot write the screenshot %s", job.path);
				if (job.reserved)
					notice(2, "SCREENSHOT FAILED");
			}
		}
		SDL_LockMutex(shots.lock);
		shots.busy = 0;
		if (!shots.count)
			SDL_BroadcastCondition(shots.idle);
	}
	/* cancelled: the jobs not begun, and their files, given up */
	while (shots.count)
	{
		struct shot_job *job = &shots.jobs[shots.head];

		if (job->reserved)
			SDL_RemovePath(job->path);
		free(job->pixels);
		job->pixels = NULL;
		shots.head = (shots.head + 1) % SHOT_QUEUE_MAXIMUM;
		shots.count--;
	}
	SDL_BroadcastCondition(shots.idle);
	SDL_UnlockMutex(shots.lock);
	SDL_SetAtomicInt(&shots.exited, 1);
	return 0;
}

/* (the game thread) the capture's start, at its first frame: the
screenshot thread, which first clears what an earlier session left in the
capture's folders, and the teardown at exit; 1 while the thread runs */
static int capture_initialize(void)
{
	static int exit_registered;

	if (shots.started)
		return shots.thread != NULL;
	shots.started = 1;
	if (!exit_registered)
	{
		atexit(capture_shutdown);
		exit_registered = 1;
	}
	clean_folder_count = 0;
	if (capture_folder("screenshots", NULL, clean_folders[clean_folder_count], PATH_SIZE))
		clean_folder_count++;
	if (capture_folder("recordings", NULL, clean_folders[clean_folder_count], PATH_SIZE))
		clean_folder_count++;
	if (*config_string("capture.record_directory") &&
		capture_folder("recordings", config_string("capture.record_directory"), clean_folders[clean_folder_count],
		PATH_SIZE))
		clean_folder_count++;
	if (!shots.lock)
		shots.lock = SDL_CreateMutex();
	if (!shots.wake)
		shots.wake = SDL_CreateCondition();
	if (!shots.idle)
		shots.idle = SDL_CreateCondition();
	if (shots.lock && shots.wake && shots.idle)
		shots.thread = SDL_CreateThread(screenshot_thread, "screenshot", NULL);
	if (!shots.thread)
		platform_log("capture: no screenshot thread (%s): screenshots are skipped", SDL_GetError());
	return shots.thread != NULL;
}

/* a job for the screenshot thread (pixels its own on success); 0 if the
queue is full or there is no thread (counted) */
static int shot_queue(const char *path, unsigned char *pixels, int width, int height, int reserved)
{
	int maximum = sizeof(void *) >= 8 ? SHOT_QUEUE_MAXIMUM : SHOT_QUEUE_32BIT;
	int queued = 0;

	if (!capture_initialize())
	{
		shots.skipped++;
		return 0;
	}
	SDL_LockMutex(shots.lock);
	if (strlen(path) + sizeof(".part") > PATH_SIZE)
	{
		shots.skipped++;
		platform_log("capture: screenshot %s skipped: its path is too long", path);
	}
	else if (!shots.stopping && shots.count + shots.busy < maximum)
	{
		struct shot_job *job = &shots.jobs[(shots.head + shots.count) % SHOT_QUEUE_MAXIMUM];

		job->pixels = pixels;
		job->width = width;
		job->height = height;
		job->reserved = reserved;
		SDL_strlcpy(job->path, path, sizeof(job->path));
		snprintf(job->part, sizeof(job->part), "%s.part", path);
		shots.count++;
		queued = 1;
		SDL_SignalCondition(shots.wake);
	}
	else
	{
		shots.skipped++;
		platform_log("capture: screenshot %s skipped (%lu so far): the screenshot thread is behind", path,
			shots.skipped);
	}
	SDL_UnlockMutex(shots.lock);
	return queued;
}

int capture_png_queue_bgra(const char *path, unsigned char *pixels, int width, int height)
{
	size_t bytes;

	if (capture_shut_down || !picture_bytes(&bytes, width, height))
		return 0;
	return shot_queue(path, pixels, width, height, 0);
}

void capture_request_screenshot(void)
{
	SDL_SetAtomicInt(&screenshot_requested, 1);
}

static void screenshot_take(unsigned int framebuffer, int width, int height)
{
	static const char *const extension[1] = { ".png" };
	char folder[PATH_SIZE], base[256], path[1][PATH_SIZE];
	const char *failure = "SCREENSHOT FAILED";
	unsigned char *pixels;
	size_t bytes;

	if (!picture_bytes(&bytes, width, height) || !(pixels = malloc(bytes)))
	{
		notice(2, "SCREENSHOT FAILED");
		return;
	}
	if (!capture_folder("screenshots", NULL, folder, sizeof(folder)))
	{
		free(pixels);
		notice(2, "CAPTURE PATH TOO LONG");
		return;
	}
	if (!capture_name_reserve(folder, extension, 1, path, base, sizeof(base), &failure))
	{
		free(pixels);
		notice(2, failure);
		return;
	}
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
	glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
	if (!shot_queue(path[0], pixels, width, height, 1))
	{
		SDL_RemovePath(path[0]);
		free(pixels);
		notice(2, "SCREENSHOT SKIPPED");
	}
}

/* ---------- the sound: one writer (the mixer), one reader (the
recording's writer thread), no lock */

static float audio_ring[AUDIO_RING_FRAMES * AUDIO_CHANNELS];
/* positions in frames, wrapping at 2^32 (25 hours) */
static SDL_AtomicU32 audio_write_position;
static SDL_AtomicU32 audio_read_position;
/* where sound the ring had no room for was lost, and how much */
static Uint32 audio_event_position[AUDIO_EVENTS];
static Uint32 audio_event_frames[AUDIO_EVENTS];
static SDL_AtomicU32 audio_event_write;
static SDL_AtomicU32 audio_event_read;
/* the mixer's way in: AUDIO_CLOSED (no recording takes the sound),
AUDIO_OPEN, AUDIO_IN (the mixer is in capture_audio), AUDIO_CLOSING (it is,
and the game thread is waiting to close). The mixer goes OPEN -> IN with a
compare-and-swap, and back IN -> OPEN, or CLOSING -> CLOSED when the game
thread asked meanwhile; the game thread goes OPEN -> CLOSED, or IN ->
CLOSING and waits for that one call to leave, and CLOSED -> OPEN. So the
close waits for one call at most, and while CLOSED the mixer cannot enter:
the ring is the game thread's to reset, and the loss not yet told its to
hand over. */
#define AUDIO_CLOSED 0
#define AUDIO_OPEN 1
#define AUDIO_IN 2
#define AUDIO_CLOSING 3
static SDL_AtomicInt audio_gate;
static SDL_AtomicInt audio_lost_frames;
/* the mixer's while it is in, the game thread's while closed: a loss the
event queue had no room for (nothing goes in after it until it is told) */
static Uint32 producer_lost_position;
static Uint32 producer_lost_frames;

static int audio_event_push(Uint32 position, Uint32 frames)
{
	Uint32 write = SDL_GetAtomicU32(&audio_event_write);

	if (write - SDL_GetAtomicU32(&audio_event_read) >= AUDIO_EVENTS)
		return 0;
	audio_event_position[write % AUDIO_EVENTS] = position;
	audio_event_frames[write % AUDIO_EVENTS] = frames;
	SDL_SetAtomicU32(&audio_event_write, write + 1);
	return 1;
}

/* (the audio thread) out of capture_audio: the gate open again, or closed
if the game thread is waiting to close it */
static void audio_leave(void)
{
	if (!SDL_CompareAndSwapAtomicInt(&audio_gate, AUDIO_IN, AUDIO_OPEN))
		SDL_SetAtomicInt(&audio_gate, AUDIO_CLOSED);
}

/* (the audio thread) never waits: what does not fit is lost, its place
noted */
void capture_audio(const float *samples, unsigned int frames, unsigned int channels, unsigned int rate)
{
	Uint32 write, room;

	if (!SDL_CompareAndSwapAtomicInt(&audio_gate, AUDIO_OPEN, AUDIO_IN))
		return;
	if (channels != AUDIO_CHANNELS || rate != AUDIO_RATE || !frames || frames > AUDIO_RING_FRAMES)
	{
		audio_leave();
		return;
	}
	write = SDL_GetAtomicU32(&audio_write_position);
	room = AUDIO_RING_FRAMES - (write - SDL_GetAtomicU32(&audio_read_position));
	if (producer_lost_frames && audio_event_push(producer_lost_position, producer_lost_frames))
		producer_lost_frames = 0;
	if (producer_lost_frames || frames > room)
	{
		if (!producer_lost_frames)
			producer_lost_position = write;
		producer_lost_frames += frames;
		SDL_AddAtomicInt(&audio_lost_frames, (int)frames);
		if (audio_event_push(producer_lost_position, producer_lost_frames))
			producer_lost_frames = 0;
	}
	else
	{
		Uint32 at = write % AUDIO_RING_FRAMES;
		Uint32 first = AUDIO_RING_FRAMES - at < frames ? AUDIO_RING_FRAMES - at : frames;

		memcpy(audio_ring + (size_t)at * AUDIO_CHANNELS, samples, (size_t)first * AUDIO_CHANNELS * sizeof(float));
		memcpy(audio_ring, samples + (size_t)first * AUDIO_CHANNELS,
			(size_t)(frames - first) * AUDIO_CHANNELS * sizeof(float));
		SDL_SetAtomicU32(&audio_write_position, write + frames);
	}
	audio_leave();
}

/* (the game thread) the mixer shut out, once the call in it (one at most)
has left capture_audio */
static void audio_close(void)
{
	for (;;)
	{
		int gate = SDL_GetAtomicInt(&audio_gate);

		if (gate == AUDIO_CLOSED)
			return;
		if (gate == AUDIO_OPEN && SDL_CompareAndSwapAtomicInt(&audio_gate, AUDIO_OPEN, AUDIO_CLOSED))
			return;
		if (gate == AUDIO_IN)
			SDL_CompareAndSwapAtomicInt(&audio_gate, AUDIO_IN, AUDIO_CLOSING);
		else if (gate == AUDIO_CLOSING)
			SDL_DelayNS(50000);
	}
}

/* (the game thread, no recording taking the sound) the ring emptied, then
the sound taken from now on */
static void audio_begin(void)
{
	audio_close();
	SDL_SetAtomicU32(&audio_read_position, SDL_GetAtomicU32(&audio_write_position));
	SDL_SetAtomicU32(&audio_event_read, SDL_GetAtomicU32(&audio_event_write));
	producer_lost_frames = 0;
	SDL_SetAtomicInt(&audio_lost_frames, 0);
	SDL_SetAtomicInt(&audio_gate, AUDIO_OPEN);
}

/* (the game thread) no more sound: the mixer shut out, and the loss it had
not told yet (it comes after all the sound in the ring) handed over, for
the writer's last drain */
static Uint32 audio_end(void)
{
	Uint32 lost;

	audio_close();
	lost = producer_lost_frames;
	producer_lost_frames = 0;
	return lost;
}

/* (the writer thread) the ring's sound to the file in order, a loss as
silence where it was; the frames written */
static Uint64 audio_drain(SDL_IOStream *file)
{
	static const float silence[512 * AUDIO_CHANNELS];
	Uint64 written = 0;

	for (;;)
	{
		Uint32 event_read = SDL_GetAtomicU32(&audio_event_read);
		int has_event = event_read != SDL_GetAtomicU32(&audio_event_write);
		/* (read after the events: a loss seen is at or before it) */
		Uint32 write = SDL_GetAtomicU32(&audio_write_position);
		Uint32 read = SDL_GetAtomicU32(&audio_read_position);
		Uint32 limit = write;

		if (has_event)
		{
			Uint32 position = audio_event_position[event_read % AUDIO_EVENTS];

			if ((Sint32)(position - read) < 0)
				position = read;
			if ((Sint32)(write - position) >= 0)
				limit = position;
		}
		while (read != limit)
		{
			Uint32 at = read % AUDIO_RING_FRAMES;
			Uint32 piece = limit - read;

			if (piece > AUDIO_RING_FRAMES - at)
				piece = AUDIO_RING_FRAMES - at;
			if (file)
				SDL_WriteIO(file, audio_ring + (size_t)at * AUDIO_CHANNELS, (size_t)piece * AUDIO_CHANNELS * sizeof(float));
			read += piece;
			written += piece;
			SDL_SetAtomicU32(&audio_read_position, read);
		}
		if (!has_event || read != limit)
			break;
		{
			Uint32 lost = audio_event_frames[event_read % AUDIO_EVENTS];

			while (lost)
			{
				Uint32 piece = lost > 512 ? 512 : lost;

				if (file)
					SDL_WriteIO(file, silence, (size_t)piece * AUDIO_CHANNELS * sizeof(float));
				lost -= piece;
				written += piece;
			}
		}
		SDL_SetAtomicU32(&audio_event_read, event_read + 1);
	}
	return written;
}

/* ---------- recording: the writer */

/* the files: the result (made, empty, at the start: its name), the video
and the sound while recording, and the two put together; the last three are
.part files until the result takes the mux's, so that one a recording left
behind is known for what it is (capture_parts_clean) */
#define RECORDING_FINAL 0
#define RECORDING_VIDEO 1
#define RECORDING_AUDIO 2
#define RECORDING_MUX 3
#define RECORDING_PATHS 4

/* struct recording's terminal */
#define RECORDING_RUNNING 0
#define RECORDING_CANCELLED 1
#define RECORDING_COMMITTED 2

struct recording
{
	/* set at the start, read by both threads */
	int width;
	int height;
	int fps;
	int crf;
	size_t frame_bytes;
	char ffmpeg[PATH_SIZE];
	/* RECORDING_*: the first three made at the start */
	char paths[RECORDING_PATHS][PATH_SIZE];

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
	/* (lock) the ffmpeg running for it, which the game thread may kill */
	struct capture_child *child;

	/* the writer's */
	Uint64 frames_written;
	Uint64 audio_frames;
	/* ffmpeg failed: the game thread stops the recording */
	SDL_AtomicInt failed;
	/* how it ends, decided once with a compare-and-swap (recording_cancel,
	recording_commit): RECORDING_RUNNING until it is given up (its files
	removed) or its result published under the final name, whichever comes
	first; the other then gives way */
	SDL_AtomicInt terminal;
	/* quitting: by when it is saved (SDL_GetTicks, never 0), else given up;
	0 none */
	SDL_AtomicU32 deadline;
	/* (set before stopping) sound lost at the end, the mixer's last loss */
	Uint32 final_lost;
	/* when the write to ffmpeg under way began (SDL_GetTicks, never 0), 0
	none */
	SDL_AtomicU32 write_since;
	SDL_AtomicInt finished;
};

static void recording_child_set(struct recording *recording, struct capture_child *child)
{
	SDL_LockMutex(recording->lock);
	recording->child = child;
	SDL_UnlockMutex(recording->lock);
}

/* (any thread) the recording's ffmpeg killed, if one runs: that child only
(the writer reaps it under the same lock, so a child reaped is never
signalled) */
static void recording_child_kill(struct recording *recording)
{
	SDL_LockMutex(recording->lock);
	if (recording->child)
		capture_child_kill(recording->child);
	SDL_UnlockMutex(recording->lock);
}

/* (any thread) the recording given up, unless its result was published
first (then it is kept: a whole recording) */
static void recording_cancel(struct recording *recording)
{
	SDL_CompareAndSwapAtomicInt(&recording->terminal, RECORDING_RUNNING, RECORDING_CANCELLED);
}

/* (the writer) the result about to be published under the final name: 1
if it may be (from now on a cancel gives way), 0 if it was given up first
(then everything is removed) */
static int recording_commit(struct recording *recording)
{
	return SDL_CompareAndSwapAtomicInt(&recording->terminal, RECORDING_RUNNING, RECORDING_COMMITTED);
}

/* given up: cancelled, or quitting's deadline passed (before a commit) */
static int recording_given_up(struct recording *recording)
{
	Uint32 deadline = SDL_GetAtomicU32(&recording->deadline);

	if (deadline && (Sint32)((Uint32)SDL_GetTicks() - deadline) >= 0)
		recording_cancel(recording);
	return SDL_GetAtomicInt(&recording->terminal) == RECORDING_CANCELLED;
}

/* (the writer) the child's end within timeout_ms, else it is killed; its
exit code, -1 if it was killed or given up. Each look at it is under the
lock, as recording_child_kill's kill. */
static int recording_child_finish(struct recording *recording, struct capture_child *child, int timeout_ms)
{
	Uint64 deadline = SDL_GetTicks() + (Uint64)timeout_ms, kill_deadline = 0;
	int exit_code = -1, ended = 0, killed = 0;

	for (;;)
	{
		SDL_LockMutex(recording->lock);
		ended = capture_child_wait(child, 0, &exit_code);
		if (ended || (killed && SDL_GetTicks() >= kill_deadline))
		{
			recording->child = NULL;
		}
		else if (!killed && (recording_given_up(recording) || SDL_GetTicks() >= deadline))
		{
			platform_log("capture: ffmpeg did not finish in time; killed");
			/* (while quitting, that is the recording given up: no video
			kept without its sound) */
			if (SDL_GetAtomicU32(&recording->deadline))
				recording_cancel(recording);
			capture_child_kill(child);
			killed = 1;
			kill_deadline = SDL_GetTicks() + 5000;
		}
		SDL_UnlockMutex(recording->lock);
		if (!recording->child && (ended || killed))
			break;
		SDL_Delay(20);
	}
	capture_child_free(child);
	return ended && !killed ? exit_code : -1;
}

static int SDLCALL recording_thread(void *parameter)
{
	struct recording *recording = parameter;
	char size_text[32], rate_text[16], crf_text[16];
	const char *arguments[32];
	int count = 0, video_ok, exit_code = -1, has_audio, published = 0;
	struct capture_child *child;
	SDL_IOStream *audio_file;

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
	arguments[count++] = "-f";
	arguments[count++] = "mp4";
	arguments[count++] = recording->paths[RECORDING_VIDEO];
	arguments[count] = NULL;
	child = capture_child_start(arguments, 1);
	if (child)
		recording_child_set(recording, child);
	else
		SDL_SetAtomicInt(&recording->failed, 1);
	audio_file = file_create(recording->paths[RECORDING_AUDIO]);

	SDL_LockMutex(recording->lock);
	for (;;)
	{
		int slot, repeat;

		while (!recording->queue_count && !recording->stopping)
			SDL_WaitConditionTimeout(recording->wake, recording->lock, 50);
		SDL_UnlockMutex(recording->lock);
		recording->audio_frames += audio_drain(audio_file);
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
		while (repeat-- > 0 && !SDL_GetAtomicInt(&recording->failed) && !recording_given_up(recording))
		{
			int ok;

			SDL_SetAtomicU32(&recording->write_since, (Uint32)SDL_GetTicks() | 1u);
			ok = capture_child_write(child, recording->slots[slot], recording->frame_bytes);
			SDL_SetAtomicU32(&recording->write_since, 0);
			if (ok)
			{
				recording->frames_written++;
			}
			else
			{
				platform_log("capture: ffmpeg stopped taking frames (it ended, or was killed)");
				SDL_SetAtomicInt(&recording->failed, 1);
			}
		}
		SDL_LockMutex(recording->lock);
		recording->free_slots[recording->free_count++] = slot;
	}
	SDL_UnlockMutex(recording->lock);

	/* the rest of the sound (the game thread stopped the mixer's: audio_end),
	and the loss at its end as silence */
	recording->audio_frames += audio_drain(audio_file);
	{
		static const float silence[512 * AUDIO_CHANNELS];
		Uint32 lost = recording->final_lost;

		while (lost)
		{
			Uint32 piece = lost > 512 ? 512 : lost;

			if (audio_file)
				SDL_WriteIO(audio_file, silence, (size_t)piece * AUDIO_CHANNELS * sizeof(float));
			lost -= piece;
			recording->audio_frames += piece;
		}
	}
	has_audio = audio_file && recording->audio_frames > 0;
	if (audio_file)
		SDL_CloseIO(audio_file);

	if (child)
	{
		capture_child_close_input(child);
		exit_code = recording_child_finish(recording, child, ENCODER_END_MS);
	}
	video_ok = !SDL_GetAtomicInt(&recording->failed) && !recording_given_up(recording) && exit_code == 0 &&
		recording->frames_written > 0;
	/* the result: the video and the sound put together (the mux's .part),
	or the video alone when there is no sound; never when given up */
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
		mux[count++] = "-f";
		mux[count++] = "mp4";
		mux[count++] = "-i";
		mux[count++] = recording->paths[RECORDING_VIDEO];
		mux[count++] = "-f";
		mux[count++] = "f32le";
		mux[count++] = "-ar";
		mux[count++] = "48000";
		mux[count++] = "-ac";
		mux[count++] = "2";
		mux[count++] = "-i";
		mux[count++] = recording->paths[RECORDING_AUDIO];
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
		mux[count++] = "-f";
		mux[count++] = "mp4";
		mux[count++] = recording->paths[RECORDING_MUX];
		mux[count] = NULL;
		child = recording_given_up(recording) ? NULL : capture_child_start(mux, 0);
		exit_code = -1;
		if (child)
		{
			recording_child_set(recording, child);
			/* (30 seconds, and half a second for each minute of video) */
			exit_code = recording_child_finish(recording, child, 30000 + (int)(video_seconds * 1000.0 / 120.0));
		}
		if (exit_code == 0 && !recording_given_up(recording))
		{
			/* (over the name made at the start; only if not given up
			first, which a cancel from now on gives way to) */
			published = 1;
			video_ok = recording_commit(recording);
			if (video_ok)
			{
				SDL_RemovePath(recording->paths[RECORDING_FINAL]);
				video_ok = SDL_RenamePath(recording->paths[RECORDING_MUX], recording->paths[RECORDING_FINAL]);
			}
		}
		else if (recording_given_up(recording))
		{
			video_ok = 0;
		}
		else
		{
			platform_log("capture: cannot add the sound (ffmpeg %d); the recording has none", exit_code);
			has_audio = 0;
		}
	}
	/* the video alone, by the same rule */
	if (video_ok && !published)
	{
		video_ok = recording_commit(recording);
		if (video_ok)
		{
			SDL_RemovePath(recording->paths[RECORDING_FINAL]);
			video_ok = SDL_RenamePath(recording->paths[RECORDING_VIDEO], recording->paths[RECORDING_FINAL]);
		}
	}
	/* the parts, whatever happened; the result too if there is none */
	SDL_RemovePath(recording->paths[RECORDING_VIDEO]);
	SDL_RemovePath(recording->paths[RECORDING_AUDIO]);
	SDL_RemovePath(recording->paths[RECORDING_MUX]);
	if (video_ok)
	{
		platform_log("capture: recording %s: %llu frames written (%.1f s at %d fps), %.1f s of sound%s",
			recording->paths[RECORDING_FINAL], (unsigned long long)recording->frames_written,
			(double)recording->frames_written / recording->fps, recording->fps,
			(double)recording->audio_frames / AUDIO_RATE, has_audio ? "" : " (none in the file)");
		notice(1, "RECORDING SAVED");
	}
	else
	{
		SDL_RemovePath(recording->paths[RECORDING_FINAL]);
		if (recording_given_up(recording))
		{
			platform_log("capture: the recording %s was given up unfinished; its files are removed",
				recording->paths[RECORDING_FINAL]);
		}
		else
		{
			platform_log("capture: the recording %s failed (ffmpeg exit %d, %llu frames written)",
				recording->paths[RECORDING_FINAL], exit_code, (unsigned long long)recording->frames_written);
			notice(2, "RECORDING FAILED");
		}
	}
	SDL_SetAtomicInt(&recording->finished, 1);
	return 0;
}

static void recording_free(struct recording *recording)
{
	int slot;

	for (slot = 0; slot < recording->slot_count; slot++)
		free(recording->slots[slot]);
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
	/* what was read: its size, and where it goes in the frame */
	int read_width;
	int read_height;
	int frame_x;
	int frame_y;
};

static struct
{
	SDL_AtomicInt toggle_requested;
	/* the recording being made, and the last one being saved */
	struct recording *active;
	struct recording *saving;

	struct pixel_buffer buffers[PBO_COUNT];
	int next_buffer;
	Uint64 presents;
	Uint64 start_ticks;
	Uint64 scheduled;
	int carry;
	double limit_seconds;
	/* the stall watchdog killed this recording's ffmpeg */
	struct recording *stall_killed;

	/* for the log */
	Uint64 dropped;
	Uint64 reads;
	Uint64 resized;
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

/* the last recording's writer, once it is done (waiting up to timeout_ms);
1 if there is none left */
static int recording_reap(int timeout_ms)
{
	struct recording *saving = capture.saving;
	Uint64 deadline = SDL_GetTicks() + (Uint64)(timeout_ms > 0 ? timeout_ms : 0);

	if (!saving)
		return 1;
	while (!SDL_GetAtomicInt(&saving->finished))
	{
		if (SDL_GetTicks() >= deadline)
			return 0;
		SDL_Delay(10);
	}
	SDL_WaitThread(saving->thread, NULL);
	if (capture.stall_killed == saving)
		capture.stall_killed = NULL;
	recording_free(saving);
	capture.saving = NULL;
	return 1;
}

/* an ffmpeg that has taken no frame for STALL_MS is killed (its write then
fails, and the recording with it) */
static void recording_watch(struct recording *recording)
{
	Uint32 since;

	if (!recording || capture.stall_killed == recording)
		return;
	since = SDL_GetAtomicU32(&recording->write_since);
	/* (signed: the write's start, made odd, can be a millisecond ahead) */
	if (since && (Sint32)((Uint32)SDL_GetTicks() - since) > STALL_MS)
	{
		platform_log("capture: ffmpeg has taken no frame for %d seconds; killed", STALL_MS / 1000);
		capture.stall_killed = recording;
		recording_child_kill(recording);
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
	static const char *const extensions[3] = { ".mp4", ".video.mp4.part", ".audio.f32.part" };
	struct recording *recording;
	char folder[PATH_SIZE], base[256];
	const char *failure = "RECORDING FAILED";
	size_t budget = RECORDING_BUDGET_BYTES, frames = 0;
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
		platform_log("capture: no ffmpeg (capture.ffmpeg_path, beside the game, or on the PATH)");
		notice(2, "RECORDING NEEDS FFMPEG");
		free(recording);
		return;
	}
	/* (H.264 in 4:2:0 wants even sizes: the last odd row or column is left
	out) */
	recording->width = width & ~1;
	recording->height = height & ~1;
	recording->fps = config_integer("capture.record_fps") == 30 ? 30 : 60;
	recording->crf = record_quality_crf();
	/* the pixel buffers and at least QUEUE_MINIMUM frames in the budget */
	if (recording->width < 2 || recording->height < 2 ||
		!picture_bytes(&recording->frame_bytes, recording->width, recording->height) ||
		(frames = budget / recording->frame_bytes) < PBO_COUNT + QUEUE_MINIMUM)
	{
		platform_log("capture: a %dx%d picture is too large to record here (%u MB for its frames)",
			width, height, (unsigned)(budget >> 20));
		notice(2, "RECORDING TOO LARGE");
		free(recording);
		return;
	}
	recording->slot_count = (int)(frames - PBO_COUNT > QUEUE_MAXIMUM ? QUEUE_MAXIMUM : frames - PBO_COUNT);
	for (slot = 0; slot < recording->slot_count; slot++)
	{
		recording->slots[slot] = malloc(recording->frame_bytes);
		if (!recording->slots[slot])
			break;
		recording->free_slots[recording->free_count++] = slot;
	}
	recording->slot_count = slot;
	recording->lock = SDL_CreateMutex();
	recording->wake = SDL_CreateCondition();
	if (recording->slot_count < QUEUE_MINIMUM || !recording->lock || !recording->wake)
	{
		notice(2, "RECORDING FAILED");
		recording_free(recording);
		return;
	}
	if (!capture_folder("recordings", config_string("capture.record_directory"), folder, sizeof(folder)))
	{
		notice(2, "CAPTURE PATH TOO LONG");
		recording_free(recording);
		return;
	}
	if (!capture_name_reserve(folder, extensions, 3, recording->paths, base, sizeof(base), &failure))
	{
		notice(2, failure);
		recording_free(recording);
		return;
	}
	if (!path_format(recording->paths[RECORDING_MUX], PATH_SIZE, "%s.part", recording->paths[RECORDING_FINAL]))
	{
		int index;

		for (index = 0; index < 3; index++)
			SDL_RemovePath(recording->paths[index]);
		notice(2, "CAPTURE PATH TOO LONG");
		recording_free(recording);
		return;
	}

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
	capture.resized = 0;
	capture.capture_counter = 0;
	capture.capture_counter_maximum = 0;
	capture.capture_presents = 0;

	audio_begin();
	recording->thread = SDL_CreateThread(recording_thread, "recording", recording);
	if (!recording->thread)
	{
		int index;

		audio_end();
		for (slot = 0; slot < PBO_COUNT; slot++)
			glDeleteBuffers(1, &capture.buffers[slot].id);
		for (index = 0; index < 3; index++)
			SDL_RemovePath(recording->paths[index]);
		notice(2, "RECORDING FAILED");
		recording_free(recording);
		return;
	}
	capture.active = recording;
	platform_log("capture: recording %s (%dx%d at %d fps, crf %d, %d frames queued at most, ffmpeg %s)",
		recording->paths[RECORDING_FINAL], recording->width, recording->height, recording->fps, recording->crf,
		recording->slot_count, recording->ffmpeg);
}

/* a pixel buffer's frame into the queue (or dropped, its time given to the
next one queued) */
static void buffer_harvest(struct pixel_buffer *buffer)
{
	struct recording *recording = capture.active;
	int repeat = buffer->repeat + capture.carry;
	int slot = -1;
	const unsigned char *pixels = NULL;
	size_t read_bytes = 0;

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
	if (picture_bytes(&read_bytes, buffer->read_width, buffer->read_height))
		pixels = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)read_bytes, GL_MAP_READ_BIT);
	if (pixels)
	{
		unsigned char *frame = recording->slots[slot];
		int row;

		if (buffer->read_width != recording->width || buffer->read_height != recording->height)
			memset(frame, 0, recording->frame_bytes);
		for (row = 0; row < buffer->read_height; row++)
		{
			memcpy(frame + ((size_t)(row + buffer->frame_y) * (size_t)recording->width + (size_t)buffer->frame_x) * 4,
				pixels + (size_t)row * (size_t)buffer->read_width * 4, (size_t)buffer->read_width * 4);
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

/* the recording stopped (harvest: the reads in flight kept, with GL; not
while quitting); its writer saves it */
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
	recording->final_lost = audio_end();
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

		platform_log("capture: stopped after %.2f s: %llu frames due, %llu reads, %llu dropped, %llu reads of "
			"another size; %d frames of sound lost; the game thread spent %.3f ms a present on it (at most %.3f ms) "
			"over %llu presents",
			seconds, (unsigned long long)capture.scheduled, (unsigned long long)capture.reads,
			(unsigned long long)capture.dropped, (unsigned long long)capture.resized,
			SDL_GetAtomicInt(&audio_lost_frames),
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
		/* (a picture whose size changed since the start: its middle, or it
		in the middle of black, at the recording's size) */
		int read_width = width < recording->width ? width : recording->width;
		int read_height = height < recording->height ? height : recording->height;

		if (buffer->pending)
			buffer_harvest(buffer);
		if ((width & ~1) != recording->width || (height & ~1) != recording->height)
			capture.resized++;
		glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->id);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
		glReadPixels((width - read_width) / 2, (height - read_height) / 2, read_width, read_height, GL_BGRA,
			GL_UNSIGNED_BYTE, NULL);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		buffer->pending = 1;
		buffer->repeat = (int)(due - capture.scheduled);
		buffer->issued = capture.presents;
		buffer->read_width = read_width;
		buffer->read_height = read_height;
		buffer->frame_x = (recording->width - read_width) / 2;
		buffer->frame_y = (recording->height - read_height) / 2;
		capture.next_buffer = (capture.next_buffer + 1) % PBO_COUNT;
		capture.reads++;
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

	if (width <= 0 || height <= 0 || capture_shut_down)
		return;
	capture_initialize();
	if (SDL_GetAtomicInt(&screenshot_requested))
	{
		SDL_SetAtomicInt(&screenshot_requested, 0);
		screenshot_take(framebuffer, width, height);
	}
	/* ffmpeg failed or died: the recording stops now (the dot with it) */
	if (capture.active && SDL_GetAtomicInt(&capture.active->failed))
	{
		platform_log("capture: ffmpeg failed; the recording stops");
		recording_stop(1);
		notice(2, "RECORDING FAILED");
		recording = 0;
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
	recording_watch(capture.active);
	recording_watch(capture.saving);
	recording_reap(0);
}

/* ---------- quitting */

/* (the game thread) a recording being saved, within what is left of
SHUTDOWN_RECORDING_MS (its writer gives up at that deadline, kills its
ffmpeg and removes its files); 1 if it is done */
static int shutdown_recording(void)
{
	struct recording *saving = capture.saving;
	Uint32 deadline;

	if (!saving || recording_reap(0))
		return 1;
	platform_log("capture: saving the recording before quitting (%d seconds at most)", SHUTDOWN_RECORDING_MS / 1000);
	notice(1, "SAVING RECORDING...");
	deadline = ((Uint32)SDL_GetTicks() + SHUTDOWN_RECORDING_MS) | 1u;
	SDL_SetAtomicU32(&saving->deadline, deadline);
	if (recording_reap(SHUTDOWN_RECORDING_MS))
		return 1;
	/* (a writer stuck in a write: its ffmpeg killed makes the write fail) */
	recording_cancel(saving);
	recording_child_kill(saving);
	if (recording_reap(SHUTDOWN_CANCEL_MS))
		return 1;
	/* the writer still runs and owns its files: they are left (the .part
	ones are removed at a later start, capture_parts_clean) */
	platform_log("capture: the recording's writer did not end; left: %s, %s, %s, %s", saving->paths[RECORDING_FINAL],
		saving->paths[RECORDING_VIDEO], saving->paths[RECORDING_AUDIO], saving->paths[RECORDING_MUX]);
	return 0;
}

/* (the game thread) the screenshots queued, written within
SHUTDOWN_SCREENSHOTS_MS, else given up; 1 if the thread has ended */
static int shutdown_screenshots(void)
{
	Uint64 deadline = SDL_GetTicks() + SHUTDOWN_SCREENSHOTS_MS;

	if (!shots.thread)
		return 1;
	SDL_LockMutex(shots.lock);
	shots.stopping = 1;
	SDL_SignalCondition(shots.wake);
	while ((shots.count || shots.busy) && SDL_GetTicks() < deadline)
		SDL_WaitConditionTimeout(shots.idle, shots.lock, 50);
	if (shots.count || shots.busy)
	{
		platform_log("capture: screenshots still being written after %d seconds; given up",
			SHUTDOWN_SCREENSHOTS_MS / 1000);
		SDL_SetAtomicInt(&shots.cancel, 1);
		SDL_SignalCondition(shots.wake);
	}
	SDL_UnlockMutex(shots.lock);
	deadline = SDL_GetTicks() + SHUTDOWN_CANCEL_MS;
	while (!SDL_GetAtomicInt(&shots.exited) && SDL_GetTicks() < deadline)
		SDL_Delay(10);
	if (SDL_GetAtomicInt(&shots.exited))
	{
		SDL_WaitThread(shots.thread, NULL);
		shots.thread = NULL;
		return 1;
	}
	/* the jobs not begun are the queue's, taken out of it here (their
	names given back); the one being written is the thread's, left as it
	is (its .part removed at a later start) */
	SDL_LockMutex(shots.lock);
	while (shots.count)
	{
		struct shot_job *job = &shots.jobs[shots.head];

		if (job->reserved)
			SDL_RemovePath(job->path);
		free(job->pixels);
		job->pixels = NULL;
		shots.head = (shots.head + 1) % SHOT_QUEUE_MAXIMUM;
		shots.count--;
	}
	platform_log("capture: the screenshot thread did not end; left: %s (and its .part)", shots.busy_path);
	SDL_UnlockMutex(shots.lock);
	return 0;
}

/* whether everything ended at the last shutdown (capture_resume) */
static int shutdown_clean;

void capture_shutdown(void)
{
	int clean;

	if (capture_shut_down)
		return;
	capture_shut_down = 1;
	/* a recording: stopped, and saved within the bounded wait */
	if (capture.active)
		recording_stop(0);
	clean = shutdown_recording();
	clean = shutdown_screenshots() && clean;
	shutdown_clean = clean;
}

void capture_resume(void)
{
	if (!capture_shut_down)
		return;
	if (!shutdown_clean)
	{
		platform_log("capture: stays off: a thread of the last shutdown is still running");
		return;
	}
	/* (the screenshot thread started again at the next frame) */
	SDL_SetAtomicInt(&shots.cancel, 0);
	SDL_SetAtomicInt(&shots.exited, 0);
	shots.stopping = 0;
	shots.started = 0;
	capture_shut_down = 0;
	platform_log("capture: on again");
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
