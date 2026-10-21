#include "harness.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>

typedef unsigned int GLuint;
typedef int GLint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned long long GLuint64;
typedef intptr_t GLintptr;
typedef unsigned long long ULONGLONG;
typedef int BOOL;
typedef struct fence *GLsync;
#define GL_NO_ERROR 0
#define GL_INVALID_OPERATION 0x0502
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_QUERY_BUFFER 0x9192
#define GL_QUERY_BUFFER_BINDING 0x9193
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_ALREADY_SIGNALED 0x911A
#define GL_TIMEOUT_EXPIRED 0x911B
#define GL_CONDITION_SATISFIED 0x911C
#include "config.inc"
/* (the code before the change has neither) */
#ifndef VISIBILITY_UNCOPIED
#define VISIBILITY_UNCOPIED 0xFFFFFFFFUL
#endif
#ifndef VISIBILITY_COPY_FRAMES
#define VISIBILITY_COPY_FRAMES 240
#endif

static struct
{
	unsigned long frame;
	GLuint visibility_known[VISIBILITY_TEST_SLOTS];
	struct visibility_batch visibility_batches[VISIBILITY_BATCH_COUNT];
	unsigned long visibility_batch;
	ULONGLONG visibility_serial;
	BOOL visibility_unavailable;
	BOOL visibility_logging;
	GLuint visibility_buffer;
	volatile GLuint *visibility_mapped;
	struct
	{
		unsigned long submitted, completed, samples, nonzero, maximum;
		unsigned long unavailable, dropped, failures;
	} visibility_stats;
} device;

/* the GPU: each query's count, and the copies into the buffer it has yet to
make (gpu_copy makes them, oldest first) */
struct fence
{
	BOOL signaled;
};
static GLuint query_samples[64];
static GLuint results[VISIBILITY_BATCH_COUNT * VISIBILITY_TEST_SLOTS];
static struct { GLuint query; GLintptr offset; } copies[256];
static unsigned long copy_count;
static struct fence fences[64];
static unsigned long fence_count;
static BOOL copy_error;
static GLenum gl_error;
/* calls that make a threaded driver catch up with every command before
them (they answer the CPU), and the flushes */
static unsigned long driver_answers, flushes, logs;

static GLenum glGetError(void)
{
	GLenum error = gl_error;

	driver_answers++;
	gl_error = GL_NO_ERROR;
	return error;
}

static void glGetQueryBufferObjectuiv(GLuint query, GLuint buffer, GLenum name, GLintptr offset)
{
	CHECK(buffer == device.visibility_buffer && name == GL_QUERY_RESULT, "a copy into another buffer");
	copies[copy_count].query = query;
	copies[copy_count++].offset = offset;
	if (copy_error)
		gl_error = GL_INVALID_OPERATION;
}

static GLsync glFenceSync(GLenum condition, GLbitfield flags)
{
	struct fence *fence = &fences[fence_count++ % 64];

	fence->signaled = FALSE;
	return fence;
}

static GLenum glClientWaitSync(GLsync fence, GLbitfield flags, GLuint64 timeout)
{
	driver_answers++;
	return fence && fence->signaled ? GL_ALREADY_SIGNALED : GL_TIMEOUT_EXPIRED;
}

static void glDeleteSync(GLsync fence) { }
static void glFlush(void) { flushes++; }
static void glBindBuffer(GLenum target, GLuint buffer) { }
static void glGetIntegerv(GLenum name, GLint *value) { driver_answers++; *value = 0; }
static void glGetQueryObjectuiv(GLuint query, GLenum name, GLuint *value)
{
	driver_answers++;
	*value = name == GL_QUERY_RESULT_AVAILABLE ? 1 : query_samples[query];
}

static void platform_log(const char *format, ...)
{
	va_list arguments;

	logs++;
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fputc('\n', stderr);
}

#include "under_test.inc"

/* the GPU makes the first count of its copies waiting (all of them with
ULONG_MAX), and passes its fences once none is left */
static void gpu_copy(unsigned long count)
{
	unsigned long i;

	if (count > copy_count)
		count = copy_count;
	for (i = 0; i < count; i++)
		results[copies[i].offset / sizeof(GLuint)] = query_samples[copies[i].query];
	for (i = count; i < copy_count; i++)
		copies[i - count] = copies[i];
	copy_count -= count;
	if (!copy_count)
	{
		for (i = 0; i < 64; i++)
			fences[i].signaled = TRUE;
	}
}

/* a frame's two tests in the batch being filled (slots 10 and 11, counts by
frame), then its present */
static struct visibility_batch *frame(unsigned long index)
{
	struct visibility_batch *batch = &device.visibility_batches[device.visibility_batch];

	CHECK(device.visibility_batch < VISIBILITY_BATCH_COUNT, "no batch free at frame %lu", index);
	batch->queries[0] = (GLuint)(1 + (index * 2) % 62);
	batch->queries[1] = (GLuint)(2 + (index * 2) % 62);
	query_samples[batch->queries[0]] = (GLuint)(100 + index);
	query_samples[batch->queries[1]] = (GLuint)(200 + index);
	batch->slots[0] = 10;
	batch->slots[1] = 11;
	batch->areas[0] = batch->areas[1] = 1.0f;
	batch->count = 2;
	device.frame = index;
	visibility_read_batch();
	return batch;
}

/* a present with no tests in its frame (none free to go in) */
static void present(unsigned long index)
{
	device.frame = index;
	visibility_read_batch();
}

static void start(BOOL logging)
{
	device.visibility_buffer = 7;
	device.visibility_mapped = results;
	device.visibility_logging = logging;
	device.visibility_known[10] = 77;
	device.visibility_known[11] = 88;
}

static void known(GLuint first, GLuint second, const char *what)
{
	CHECK(device.visibility_known[10] == first && device.visibility_known[11] == second,
		"%s: the slots hold %u and %u, not %u and %u", what, device.visibility_known[10], device.visibility_known[11],
		first, second);
}

/* frames whose counts the GPU copies before the next present, which reads
them */
static void play(unsigned long frames)
{
	unsigned long index;

	for (index = 0; index < frames; index++)
	{
		frame(index);
		if (index)
			known((GLuint)(100 + index - 1), (GLuint)(200 + index - 1), "a frame on");
		gpu_copy(ULONG_MAX);
	}
	CHECK(device.visibility_stats.completed == frames - 1, "%lu batches read, not %lu",
		device.visibility_stats.completed, frames - 1);
}

int main(int argc, char **argv)
{
	const char *case_name = argv[1];

	CASE("quiet")
	{
		/* without debug.gl_debug, no present asks the driver for anything
		(a GL error, a fence's state), and none flushes: presenting does */
		start(FALSE);
		play(12);
		CHECK(driver_answers == 0, "the driver was asked %lu times in 12 presents", driver_answers);
		CHECK(flushes == 0, "%lu flushes in 12 presents", flushes);
	}
	CASE("behind")
	{
		/* counts the GPU has not copied yet are not taken, the slots keep
		their earlier ones, and a batch is not used again until it is read */
		start(FALSE);
		frame(0);
		frame(1);
		frame(2);
		known(77, 88, "nothing copied");
		CHECK(device.visibility_batch == VISIBILITY_BATCH_COUNT, "a batch whose counts are unread was given out");
		gpu_copy(1);
		present(3);
		known(100, 88, "one count copied");
		gpu_copy(ULONG_MAX);
		present(4);
		known(102, 202, "all copied");
	}
	CASE("reused")
	{
		/* a batch used again does not give its earlier counts for the new
		tests' before the GPU copies those */
		start(FALSE);
		play(VISIBILITY_BATCH_COUNT + 2);
		frame(VISIBILITY_BATCH_COUNT + 2);
		frame(VISIBILITY_BATCH_COUNT + 3);
		known(100 + VISIBILITY_BATCH_COUNT + 1, 200 + VISIBILITY_BATCH_COUNT + 1, "the GPU behind a reused batch");
	}
	CASE("lost")
	{
		/* counts never copied into the buffer (a driver that does not show
		the GPU's copies there) are read on the CPU from then on, the batch
		waiting too, rather than its batch dropped (AE, 0.3.5.1: the slots
		would keep their counts for the rest of the game) */
		start(FALSE);
		frame(0);
		copy_count = 0;
		frame(VISIBILITY_COPY_FRAMES);
		CHECK(device.visibility_mapped, "the buffer given up before VISIBILITY_COPY_FRAMES presents");
		gpu_copy(ULONG_MAX);
		frame(VISIBILITY_COPY_FRAMES + 2);
		CHECK(!device.visibility_mapped, "counts that never came through the buffer still looked for there");
		CHECK(!device.visibility_batches[0].failed, "the batch waiting was dropped, not read on the CPU");
		CHECK(logs > 0, "the change to the CPU not reported");
		present(VISIBILITY_COPY_FRAMES + 3);
		known((GLuint)(100 + VISIBILITY_COPY_FRAMES), (GLuint)(200 + VISIBILITY_COPY_FRAMES), "read on the CPU");
		present(VISIBILITY_COPY_FRAMES + 4);
		known((GLuint)(100 + VISIBILITY_COPY_FRAMES + 2), (GLuint)(200 + VISIBILITY_COPY_FRAMES + 2),
			"a batch sealed after the change");
	}
	CASE("logging")
	{
		/* with debug.gl_debug, GL errors are asked for, and a failed copy
		drops its batch */
		start(TRUE);
		play(6);
		CHECK(driver_answers > 0, "GL errors never asked for with debug.gl_debug");
		copy_error = TRUE;
		CHECK(frame(6)->failed, "a batch whose copies failed was kept");
		CHECK(logs > 0 && device.visibility_stats.failures, "a failed copy not reported");
	}
	return 0;
}
