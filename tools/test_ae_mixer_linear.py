"""audio.resampling = "linear" in the real mixer (port/linux/src/dsound_sdl.c): a stereo ramp through mix_voice
comes out as the same ramp, each channel its own, across the history ring's wrap; built with the address and
undefined-behaviour sanitizers, so a read outside the resampler's history stops the test.

The mixer is taken as upstream's tools/harness/tests/test_mixer.py takes it (from its constants to the end of the
mixing); the C below is Arena Evolved's own."""
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "port/linux/src/dsound_sdl.c"

PROGRAM = r'''
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned long ULONG;
typedef void *LPVOID;
typedef void (*LPFNXMEDIAOBJECTCALLBACK)(void *, void *, DWORD);
typedef struct { void *lpVtbl; } IDirectSoundStream;
typedef struct { void *pvBuffer; DWORD dwMaxSize; } XMEDIAPACKET;
typedef struct
{
	LONG lRoom, lRoomHF;
	float flRoomRolloffFactor, flDecayTime, flDecayHFRatio;
	LONG lReflections;
	float flReflectionsDelay;
	LONG lReverb;
	float flReverbDelay, flDiffusion, flDensity, flHFReference;
} DSI3DL2LISTENER;
#define DSBVOLUME_MIN (-10000)
#define DS3DMODE_NORMAL 0x00000000
#define DS3DMODE_HEADRELATIVE 0x00000001
#define DS3DMODE_DISABLE 0x00000002
static void voice_audio_mix(float *output, unsigned long frames) { (void)output; (void)frames; }
static void capture_audio(const float *output, unsigned int frames, int channels, int rate)
{ (void)output; (void)frames; (void)channels; (void)rate; }
static double config_real(const char *name) { (void)name; return 1.0; }

#include "under_test.inc"

#define CHUNK 512
#define SOURCE_FRAMES 2000
#define OUTPUT_FRAMES 3000
#define STEP 20 /* the ramp's rise a source frame */

int main(void)
{
	static short samples[SOURCE_FRAMES * 2];
	static float output[OUTPUT_FRAMES * 2], buffer[CHUNK * 2], send[CHUNK];
	struct sdl_stream *stream = calloc(1, sizeof(*stream));
	unsigned long done, frame;
	/* a 24 kHz voice: half a source frame an output frame */
	double expected = STEP * 0.5 / 32768.0;

	for (frame = 0; frame < SOURCE_FRAMES; frame++)
	{
		samples[frame * 2] = (short)((long)frame * STEP - 20000);
		samples[frame * 2 + 1] = (short)(20000 - (long)frame * STEP);
	}
	stream->channels = 2;
	stream->sample_rate = 24000;
	stream->frequency = 0;
	stream->volume = 1.0f;
	stream->mix_left = stream->mix_right = 1.0f;
	stream->room = stream->room_hf = DSBVOLUME_MIN;
	stream->packets[0].samples = malloc(sizeof(samples));
	memcpy(stream->packets[0].samples, samples, sizeof(samples));
	stream->packets[0].frames = SOURCE_FRAMES;
	stream->packet_count = 1;
	resampler_reset(stream);
	resampling_linear = TRUE;
	for (done = 0; done < OUTPUT_FRAMES; done += CHUNK)
	{
		unsigned long count = OUTPUT_FRAMES - done < CHUNK ? OUTPUT_FRAMES - done : CHUNK;

		memset(buffer, 0, sizeof(buffer));
		memset(send, 0, sizeof(send));
		mix_voice(stream, buffer, send, count);
		memcpy(output + done * 2, buffer, count * 2 * sizeof(float));
	}
	/* (past the first chunk, where the voice's gains ramp up; well before the ramp's end) */
	for (frame = 2 * CHUNK; frame < OUTPUT_FRAMES - 200; frame++)
	{
		double left = output[frame * 2] - output[(frame - 1) * 2];
		double right = output[frame * 2 + 1] - output[(frame - 1) * 2 + 1];

		if (!isfinite(output[frame * 2]) || fabs(left - expected) > expected * 0.02 ||
			fabs(right + expected) > expected * 0.02 || fabs(output[frame * 2] + output[frame * 2 + 1]) > 1e-4)
		{
			printf("frame %lu: left %g (step %g) right %g (step %g), a step of %g expected\n", frame,
				output[frame * 2], left, output[frame * 2 + 1], right, expected);
			return 1;
		}
	}
	printf("linear: %d frames, each channel its own ramp\n", OUTPUT_FRAMES);
	return 0;
}
'''


def test_linear_resampling_reads_its_history(tmp_path):
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        pytest.skip("needs a C compiler")
    text = SOURCE.read_text()
    (tmp_path / "under_test.inc").write_text(text[text.index("#define OUTPUT_RATE"):text.index("/* ---------- output")])
    (tmp_path / "linear.c").write_text(PROGRAM)
    binary = tmp_path / "linear"
    built = subprocess.run([compiler, "-std=gnu99", "-O1", "-g", "-fsanitize=address,undefined",
                            "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-w", "-I", str(tmp_path),
                            str(tmp_path / "linear.c"), "-o", str(binary), "-lm", "-lpthread"],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr[-3000:]
    ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120)
    assert ran.returncode == 0, (ran.stdout + ran.stderr)[-3000:]
