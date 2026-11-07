/*
AE_ANNOUNCER.C

(spike) An announcer pack (ae_announcer.h). Log: "announcer: ...".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"

#include "ae_announcer.h"
#include "ae_medals.h"

/* (a path of the host, not one of the game's drives) */
#undef fopen

void platform_log(const char *format, ...);
const char *platform_data_root(void);
void platform_ui_tone_start(short const *samples, unsigned long frames, unsigned long channels,
	unsigned long sample_rate);

#define QUEUE_MOST 3
#define GAP_MILLISECONDS 120
#define CLIP_BYTES_MOST (8 * 1024 * 1024)

struct clip
{
	boolean tried;
	short *samples;
	unsigned long frames;
	unsigned long channels;
	unsigned long rate;
};

static struct
{
	struct clip clips[AE_MEDAL_COUNT];
	int queue[QUEUE_MOST];
	int queued;
	unsigned long free_at;
} announcer;

static char const *pack_name(void)
{
	char const *pack = getenv("HALO_ANNOUNCER_PACK");
	/* (a pack only when one is named; no folder of that name, no pack) */
	return pack && *pack && !strchr(pack, '/') && !strchr(pack, '\\') ? pack : NULL;
}

static unsigned long little(unsigned char const *at, int bytes)
{
	unsigned long value = 0;
	int index;

	for (index = 0; index < bytes; index++)
		value |= (unsigned long)at[index] << (index * 8);
	return value;
}

static void clip_parse(struct clip *clip, unsigned char const *bytes, long size)
{
	long at = 12;
	int bits = 0;

	if (size < 44 || memcmp(bytes, "RIFF", 4) || memcmp(bytes + 8, "WAVE", 4))
		return;
	while (at + 8 <= size)
	{
		unsigned long length = little(bytes + at + 4, 4);

		if (length > (unsigned long)(size - at - 8))
			return;
		if (!memcmp(bytes + at, "fmt ", 4) && length >= 16)
		{
			clip->channels = little(bytes + at + 10, 2);
			clip->rate = little(bytes + at + 12, 4);
			bits = (int)little(bytes + at + 22, 2);
		}
		else if (!memcmp(bytes + at, "data", 4))
		{
			if (bits != 16 || clip->channels < 1 || clip->channels > 2 || !clip->rate || length < 2)
				return;
			clip->samples = malloc(length);
			if (clip->samples)
			{
				memcpy(clip->samples, bytes + at + 8, length);
				clip->frames = length / (2 * clip->channels);
			}
			return;
		}
		at += 8 + (long)length + (long)(length & 1);
	}
}

static struct clip *clip_of(int medal)
{
	struct clip *clip;
	char path[1024];
	char const *pack = pack_name();
	char const *key;
	FILE *file;
	long size;
	unsigned char *bytes;

	if (!pack || medal < 0 || medal >= AE_MEDAL_COUNT)
		return NULL;
	clip = &announcer.clips[medal];
	if (clip->tried)
		return clip->samples ? clip : NULL;
	clip->tried = TRUE;
	key = ae_medal_key(medal);
	if (!key)
		return NULL;
	snprintf(path, sizeof(path), "%s/announcers/%s/%s.wav", platform_data_root(), pack, key);
	file = fopen(path, "rb");
	if (!file)
		return NULL;
	if (!fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0 && size <= CLIP_BYTES_MOST && !fseek(file, 0, SEEK_SET) &&
		(bytes = malloc((size_t)size)) != NULL)
	{
		if (fread(bytes, 1, (size_t)size, file) == (size_t)size)
			clip_parse(clip, bytes, size);
		free(bytes);
	}
	fclose(file);
	return clip->samples ? clip : NULL;
}

void ae_announcer_new_map(void)
{
	int medal, count = 0;

	announcer.queued = 0;
	if (!pack_name())
		return;
	for (medal = 0; medal < AE_MEDAL_COUNT; medal++)
		count += clip_of(medal) != NULL;
	platform_log("announcer: pack %s: %d clips of %d medals", pack_name(), count, AE_MEDAL_COUNT);
}

void ae_announcer_medal(int medal)
{
	if (!clip_of(medal))
		return;
	/* (full: the oldest waiting goes, so the voice does not fall behind the game) */
	if (announcer.queued == QUEUE_MOST)
	{
		memmove(announcer.queue, announcer.queue + 1, sizeof(announcer.queue[0]) * (QUEUE_MOST - 1));
		announcer.queued--;
	}
	announcer.queue[announcer.queued++] = medal;
}

void ae_announcer_update(void)
{
	unsigned long now;
	struct clip *clip;

	if (!announcer.queued)
		return;
	now = system_milliseconds();
	if ((long)(now - announcer.free_at) < 0)
		return;
	clip = clip_of(announcer.queue[0]);
	announcer.queued--;
	memmove(announcer.queue, announcer.queue + 1, sizeof(announcer.queue[0]) * (size_t)announcer.queued);
	if (!clip)
		return;
	platform_ui_tone_start(clip->samples, clip->frames, clip->channels, clip->rate);
	announcer.free_at = now + (unsigned long)((unsigned long long)clip->frames * 1000 / clip->rate) + GAP_MILLISECONDS;
}

int ae_announcer_replaces(short multiplayer_sound)
{
	/* (double kill, triple kill, killtacular, running riot, killing spree) */
	return pack_name() != NULL && multiplayer_sound >= 0xE && multiplayer_sound <= 0x12;
}
