/*
CALLOUT_VOICE.C

port: the callouts' voice pack (source/game/callouts.c): a folder of WAVs
in the data root's voices/<pack>/ (game.callout_voice), each clip named for
what it says (one.wav, rockets.wav, ...), read into memory and played one
at a time through the mixer's UI voice (dsound_sdl.c). The played mod's own
mods/<mod>/voices/<pack>/ comes first, clip by clip. 16-bit PCM WAVs, mono
or stereo at any rate (the mixer resamples); a clip missing or of another
format is left out, and the pack's are logged once, as it is read.
*/

#include "platform.h"
#include "port_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXIMUM_CALLOUT_CLIPS 64
#define MAXIMUM_PACK_NAME 64
/* (a callout is a word or two: no clip is near this) */
#define MAXIMUM_CLIP_FILE_BYTES (16L * 1024 * 1024)

struct callout_clip
{
	short *samples;	/* interleaved */
	unsigned long frames;
	unsigned long channels;
	unsigned long sample_rate;
};

static struct callout_clip clips[MAXIMUM_CALLOUT_CLIPS];
static int clip_count;
/* the pack and the mod read, "" for none */
static char loaded_pack[MAXIMUM_PACK_NAME];
static char loaded_mod[MAXIMUM_PACK_NAME];
static int loaded;

/* a name that is one folder's alone */
static int folder_name_valid(const char *name)
{
	return name && name[0] && strlen(name) < MAXIMUM_PACK_NAME && !strchr(name, '/') && !strchr(name, '\\') &&
		!strstr(name, "..");
}

static unsigned long little_endian(const unsigned char *bytes, int count)
{
	unsigned long value = 0;
	int index;

	for (index = count - 1; index >= 0; index--)
		value = (value << 8) | bytes[index];
	return value;
}

/* 1: read; 0: no such file; -1: not a WAV this reads */
static int wav_read(const char *path, struct callout_clip *clip)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data = NULL;
	long length = -1;
	unsigned long offset;
	unsigned long channels = 0, sample_rate = 0, bits = 0;
	const unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;
	int result = -1;

	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) >= 12 && length <= MAXIMUM_CLIP_FILE_BYTES &&
		fseek(file, 0, SEEK_SET) == 0)
	{
		data = (unsigned char *)malloc((size_t)length);
		if (data && fread(data, 1, (size_t)length, file) != (size_t)length)
		{
			free(data);
			data = NULL;
		}
	}
	fclose(file);
	if (!data)
		return -1;

	if (memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4))
		goto done;
	/* (the chunks: "fmt " and "data", each padded to an even size) */
	for (offset = 12; offset + 8 <= (unsigned long)length;)
	{
		unsigned long size = little_endian(data + offset + 4, 4);
		const unsigned char *body = data + offset + 8;

		if (size > (unsigned long)length - offset - 8)
			size = (unsigned long)length - offset - 8;
		if (!memcmp(data + offset, "fmt ", 4) && size >= 16)
		{
			unsigned long format = little_endian(body, 2);

			/* (PCM, or WAVE_FORMAT_EXTENSIBLE's PCM) */
			if (format == 0xFFFE && size >= 26)
				format = little_endian(body + 24, 2);
			if (format != 1)
				goto done;
			channels = little_endian(body + 2, 2);
			sample_rate = little_endian(body + 4, 4);
			bits = little_endian(body + 14, 2);
		}
		else if (!memcmp(data + offset, "data", 4))
		{
			pcm = body;
			pcm_bytes = size;
		}
		offset += 8 + size + (size & 1);
	}
	if (!pcm || bits != 16 || channels < 1 || channels > 2 || sample_rate < 4000 || sample_rate > 192000 ||
		pcm_bytes < channels * 2)
	{
		goto done;
	}

	clip->channels = channels;
	clip->sample_rate = sample_rate;
	clip->frames = pcm_bytes / (channels * 2);
	clip->samples = (short *)malloc(clip->frames * channels * sizeof(short));
	if (clip->samples)
	{
		unsigned long sample;

		for (sample = 0; sample < clip->frames * channels; sample++)
			clip->samples[sample] = (short)little_endian(pcm + sample * 2, 2);
		result = 1;
	}

done:
	free(data);
	return result;
}

static void clips_free(void)
{
	int index;

	/* (the mixer lets go of the playing one first) */
	platform_ui_voice_stop();
	for (index = 0; index < clip_count; index++)
		free(clips[index].samples);
	memset(clips, 0, sizeof(clips));
	clip_count = 0;
}

int platform_callout_voice_load(char const *pack, char const *const *names, int count)
{
	const char *mod = config_string("game.mod");
	char missing[512];
	int read = 0;
	int index;

	if (!folder_name_valid(mod))
		mod = "";
	if (!folder_name_valid(pack))
		pack = "";
	if (count > MAXIMUM_CALLOUT_CLIPS)
		count = MAXIMUM_CALLOUT_CLIPS;
	if (loaded && clip_count == count && !strcmp(loaded_pack, pack) && !strcmp(loaded_mod, mod))
	{
		for (index = 0; index < count; index++)
			read += clips[index].samples != NULL;
		return read;
	}

	clips_free();
	loaded = 1;
	strcpy(loaded_pack, pack);
	strcpy(loaded_mod, mod);
	clip_count = count;
	missing[0] = 0;
	if (!pack[0])
	{
		platform_log("callouts: no voice pack (game.callout_voice is not a folder's name)");
		return 0;
	}
	for (index = 0; index < count; index++)
	{
		char path[1200];
		int result = 0;

		if (mod[0])
		{
			snprintf(path, sizeof(path), "%s/mods/%s/voices/%s/%s.wav", platform_data_root(), mod, pack, names[index]);
			result = wav_read(path, &clips[index]);
		}
		if (result == 0)
		{
			snprintf(path, sizeof(path), "%s/voices/%s/%s.wav", platform_data_root(), pack, names[index]);
			result = wav_read(path, &clips[index]);
		}
		if (result > 0)
		{
			read++;
		}
		else if (strlen(missing) + strlen(names[index]) + 16 < sizeof(missing))
		{
			strcat(missing, missing[0] ? ", " : "");
			strcat(missing, names[index]);
			if (result < 0)
				strcat(missing, " (not 16-bit PCM)");
		}
	}
	platform_log("callouts: voice '%s'%s%s: %d of %d clips from %s/voices/%s", pack, mod[0] ? " with mod " : "",
		mod, read, count, platform_data_root(), pack);
	if (missing[0])
		platform_log("callouts: voice '%s' lacks %s", pack, missing);
	return read;
}

int platform_callout_voice_play(int clip)
{
	if (clip < 0 || clip >= clip_count || !clips[clip].samples)
		return 0;
	platform_ui_voice_start(clips[clip].samples, clips[clip].frames, clips[clip].channels, clips[clip].sample_rate);
	return 1;
}

int platform_callout_voice_busy(void)
{
	return platform_ui_voice_busy();
}
