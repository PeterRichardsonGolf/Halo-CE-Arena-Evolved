/*
CALLOUT_VOICE.C

port: the callouts' voice pack (source/game/callouts.c): a folder of WAVs
in the data root's voices/<pack>/ (game.callout_voice), each clip named for
what it says (one.wav, rockets.wav, ...), read into memory and played one
at a time through the mixer's UI voice (dsound_sdl.c). The played mod's own
mods/<mod>/voices/<pack>/ comes first, clip by clip (its copy unreadable,
the voices/ one). 16-bit PCM WAVs, mono or stereo at any rate (the mixer
resamples); a clip missing or unusable is left out, and the pack's are
logged once, with why, as it is read. A pack that is no folder of either
gives way to the first folder of voices/ by name (game.callout_voice stays
as it is).
*/

#include "platform.h"
#include "port_config.h"

#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <errno.h>
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
/* the pack asked for (game.callout_voice) and the mod read, "" for none */
static char loaded_pack[MAXIMUM_PACK_NAME];
static char loaded_mod[MAXIMUM_PACK_NAME];
static int loaded;

/* a name that is one folder's alone */
static int folder_name_valid(const char *name)
{
	return name && name[0] && strlen(name) < MAXIMUM_PACK_NAME && !strchr(name, '/') && !strchr(name, '\\') &&
		!strstr(name, "..");
}

static int folder_exists(const char *path)
{
#ifdef _WIN32
	DWORD attributes = GetFileAttributesA(path);

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
	DIR *opened = opendir(path);

	if (opened)
		closedir(opened);
	return opened != NULL;
#endif
}

static int name_compare(const char *a, const char *b)
{
	while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

static void first_pack_consider(const char *folder, const char *name, char *first)
{
	char path[1200];

	if (name[0] == '.' || !folder_name_valid(name) || (first[0] && name_compare(name, first) >= 0))
		return;
	snprintf(path, sizeof(path), "%s/%s", folder, name);
	if (folder_exists(path))
		strcpy(first, name);
}

/* the first folder of a voices/ folder by name (as the VOICE spinner lists
them), "" for none */
static void first_pack(const char *folder, char *first)
{
	first[0] = 0;
	{
#ifdef _WIN32
		char pattern[1100];
		WIN32_FIND_DATAA found;
		HANDLE search;

		snprintf(pattern, sizeof(pattern), "%s\\*", folder);
		search = FindFirstFileA(pattern, &found);
		if (search != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					first_pack_consider(folder, found.cFileName, first);
			} while (FindNextFileA(search, &found));
			FindClose(search);
		}
#else
		DIR *opened = opendir(folder);
		struct dirent *entry;

		if (opened)
		{
			while ((entry = readdir(opened)) != NULL)
				first_pack_consider(folder, entry->d_name, first);
			closedir(opened);
		}
#endif
	}
}

static unsigned long little_endian(const unsigned char *bytes, int count)
{
	unsigned long value = 0;
	int index;

	for (index = count - 1; index >= 0; index--)
		value = (value << 8) | bytes[index];
	return value;
}

/* 1: read; 0: no such file; -1: not usable, *reason saying why */
static int wav_read(const char *path, struct callout_clip *clip, const char **reason)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data = NULL;
	long length = -1;
	unsigned long offset;
	unsigned long format = 0, channels = 0, sample_rate = 0, bits = 0;
	const unsigned char *pcm = NULL;
	unsigned long pcm_bytes = 0;
	int result = -1;

	if (!file)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return 0;
		*reason = "unreadable";
		return -1;
	}
	*reason = "unreadable";
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) >= 0 && fseek(file, 0, SEEK_SET) == 0)
	{
		if (length < 12)
			*reason = "too short";
		else if (length > MAXIMUM_CLIP_FILE_BYTES)
			*reason = "too large";
		else
		{
			data = (unsigned char *)malloc((size_t)length);
			if (data && fread(data, 1, (size_t)length, file) != (size_t)length)
			{
				free(data);
				data = NULL;
			}
			if (!data)
				*reason = "unreadable";
		}
	}
	fclose(file);
	if (!data)
		return -1;

	*reason = "not a RIFF WAVE file";
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
			format = little_endian(body, 2);
			/* (PCM, or WAVE_FORMAT_EXTENSIBLE's PCM) */
			if (format == 0xFFFE && size >= 26)
				format = little_endian(body + 24, 2);
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
	if (!format)
		*reason = "no fmt chunk";
	else if (format != 1 || bits != 16)
		*reason = "not 16-bit PCM";
	else if (channels < 1 || channels > 2)
		*reason = "not mono or stereo";
	else if (sample_rate < 4000 || sample_rate > 192000)
		*reason = "a sample rate out of 4000..192000";
	else if (!pcm || pcm_bytes < channels * 2)
		*reason = "too short (no samples)";
	else
	{
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
		else
		{
			*reason = "too large (out of memory)";
		}
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

/* the folder the pack's clips come from: the pack asked for when the mod's
voices/ or the data root's has it, else the first folder of the data root's
voices/ (else the mod's); "" for none */
static void pack_resolve(const char *pack, const char *mod, char *resolved)
{
	char folder[1100];

	resolved[0] = 0;
	if (pack[0])
	{
		if (mod[0])
		{
			snprintf(folder, sizeof(folder), "%s/mods/%s/voices/%s", platform_data_root(), mod, pack);
			if (folder_exists(folder))
				strcpy(resolved, pack);
		}
		snprintf(folder, sizeof(folder), "%s/voices/%s", platform_data_root(), pack);
		if (!resolved[0] && folder_exists(folder))
			strcpy(resolved, pack);
		if (resolved[0])
			return;
	}

	snprintf(folder, sizeof(folder), "%s/voices", platform_data_root());
	first_pack(folder, resolved);
	if (!resolved[0] && mod[0])
	{
		snprintf(folder, sizeof(folder), "%s/mods/%s/voices", platform_data_root(), mod);
		first_pack(folder, resolved);
	}
	if (resolved[0])
	{
		platform_log("callouts: no voice pack '%s' in %s/voices%s%s%s; using '%s', the first there "
			"(game.callout_voice stays '%s')", pack, platform_data_root(), mod[0] ? " or mods/" : "", mod,
			mod[0] ? "/voices" : "", resolved, pack);
	}
}

int platform_callout_voice_load(char const *pack, char const *const *names, int count)
{
	const char *mod = config_string("game.mod");
	char resolved[MAXIMUM_PACK_NAME];
	char missing[1024];
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
	pack_resolve(pack, mod, resolved);
	if (!resolved[0])
	{
		platform_log("callouts: no voice pack ('%s' is no folder of %s/voices, which has none)", pack,
			platform_data_root());
		return 0;
	}
	for (index = 0; index < count; index++)
	{
		char path[1200];
		const char *reason = NULL;
		const char *mod_reason = NULL;
		int result = 0;

		if (mod[0])
		{
			snprintf(path, sizeof(path), "%s/mods/%s/voices/%s/%s.wav", platform_data_root(), mod, resolved,
				names[index]);
			result = wav_read(path, &clips[index], &mod_reason);
		}
		/* (none in the mod, or its copy unusable: the voices/ one) */
		if (result <= 0)
		{
			snprintf(path, sizeof(path), "%s/voices/%s/%s.wav", platform_data_root(), resolved, names[index]);
			result = wav_read(path, &clips[index], &reason);
			if (result > 0 && mod_reason)
			{
				platform_log("callouts: voice '%s': mods/%s's %s.wav is %s; using voices/'s", resolved, mod,
					names[index], mod_reason);
			}
			else if (result == 0 && mod_reason)
			{
				result = -1;
				reason = mod_reason;
			}
		}
		if (result > 0)
		{
			read++;
		}
		else if (strlen(missing) + strlen(names[index]) + (reason ? strlen(reason) : 0) + 8 < sizeof(missing))
		{
			strcat(missing, missing[0] ? ", " : "");
			strcat(missing, names[index]);
			if (result < 0 && reason)
			{
				strcat(missing, " (");
				strcat(missing, reason);
				strcat(missing, ")");
			}
		}
	}
	if (mod[0])
	{
		platform_log("callouts: voice '%s' with mod %s: %d of %d clips from %s/mods/%s/voices/%s and %s/voices/%s",
			resolved, mod, read, count, platform_data_root(), mod, resolved, platform_data_root(), resolved);
	}
	else
	{
		platform_log("callouts: voice '%s': %d of %d clips from %s/voices/%s", resolved, read, count,
			platform_data_root(), resolved);
	}
	if (missing[0])
		platform_log("callouts: voice '%s' lacks %s", resolved, missing);
	return read;
}

void platform_callout_voice_unload(void)
{
	if (!loaded && !clip_count)
		return;
	clips_free();
	loaded = 0;
	loaded_pack[0] = 0;
	loaded_mod[0] = 0;
	platform_log("callouts: voice clips freed");
}

int platform_callout_voice_play(int clip)
{
	if (clip < 0 || clip >= clip_count || !clips[clip].samples)
		return 0;
	platform_ui_voice_start(clips[clip].samples, clips[clip].frames, clips[clip].channels, clips[clip].sample_rate);
	return 1;
}

long platform_callout_voice_milliseconds(int clip)
{
	if (clip < 0 || clip >= clip_count || !clips[clip].samples || !clips[clip].sample_rate)
		return 0;
	return (long)((double)clips[clip].frames * 1000.0 / (double)clips[clip].sample_rate + 0.999);
}

void platform_callout_voice_stop(void)
{
	platform_ui_voice_stop();
}

int platform_callout_voice_busy(void)
{
	return platform_ui_voice_busy();
}
