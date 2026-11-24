/*
AE_MCC_AUDIO_POOL.INL

Included at the end of mcc_audio.c (ae_mcc_audio_pool.h): its workers call that unit's static decoder and encoder.
Everything here but ae_audio_run runs on the loading thread; ae_audio_run runs on a worker and touches only its job
(the payload copy, the samples mcc_vorbis allocates from the C library, the ADPCM buffer from the pool's allocator) and
the pure decoder and encoder, whose loading-screen pulses do nothing off the loading thread (mcc_loading.c).
*/

#include "ae_mcc_convert_pool.h"
#include "../src/ae_worker.h"
#include "../src/port_config.h"
#include "../../third_party/zlib/zlib_prefixed.h"

#define AE_AUDIO_WORKERS_AUTO 4
#define AE_AUDIO_HELD_MOST (64u << 20)
#define AE_AUDIO_WAIT_MOST 30000u
#define AE_AUDIO_DRAIN_MOST 5000u

struct ae_audio_job
{
	int channels;
	uint32_t rate, output_rate;
	unsigned char *input;
	uint32_t size;
	struct mcc_pcm pcm;
	int pcm_ok;
	unsigned char *adpcm;
	uint32_t adpcm_size;
	int adpcm_ok;
	unsigned int run_milliseconds;
};

static struct
{
	int on, workers;
	struct mcc_runtime *runtime;
	/* the walk ahead: the next tag, pitch range and permutation to look at */
	uint32_t tag, range, permutation;
	int walked;
	struct ae_audio_job *taken;
	uint32_t from_workers, here, read_failed;
	unsigned int waited, longest;
	int out_of_step;
} ae_audio;

static void ae_audio_job_free(void *data)
{
	struct ae_audio_job *job = (struct ae_audio_job *)data;

	if (!job)
		return;
	if (job->input)
		ae_mcc_convert_free(job->input);
	mcc_pcm_close(&job->pcm);
	if (job->adpcm)
		ae_mcc_convert_free(job->adpcm);
	ae_mcc_convert_free(job);
}

/* on a worker: the payload decoded (mcc_pcm_open) and encoded (mcc_audio_store on storage of its own, sized as the
loading thread's reserve sizes it); 0 when the decoder failed (the loading thread decodes it again, for its message) */
static int ae_audio_run(void *data, unsigned int *bytes)
{
	struct ae_audio_job *job = (struct ae_audio_job *)data;
	struct mcc_audio_storage storage;
	struct sound_permutation permutation;
	uint64_t frames64;
	uint32_t frames;
	unsigned int start = ae_worker_milliseconds();

	job->pcm_ok = mcc_pcm_open(job->input, job->size, 3, job->channels, job->rate, &job->pcm);
	if (!job->pcm_ok)
	{
		mcc_pcm_close(&job->pcm);
		return 0;
	}
	*bytes += job->pcm.frames * (uint32_t)job->pcm.channels * (uint32_t)sizeof(short);
	/* (a result mcc_audio_store refuses is left to the loading thread's own store, with these samples) */
	frames64 = job->pcm.rate ? ((uint64_t)job->pcm.frames * job->output_rate + job->pcm.rate - 1) / job->pcm.rate : 0;
	if (!frames64 || frames64 > MCC_AUDIO_FRAME_LIMIT)
		return 1;
	frames = (uint32_t)frames64;
	job->adpcm_size = (frames + 63) / 64 * 36 * (uint32_t)job->channels;
	if (job->adpcm_size >= MCC_AUDIO_LIMIT || !(job->adpcm = (unsigned char *)ae_mcc_convert_alloc(job->adpcm_size)))
		return 1;
	*bytes += job->adpcm_size;
	storage.bytes = job->adpcm;
	storage.used = 0;
	storage.capacity = job->adpcm_size;
	storage.limit = job->adpcm_size;
	storage.handle = 0;
	storage.failure = NULL;
	memset(&permutation, 0, sizeof(permutation));
	job->adpcm_ok = mcc_audio_store(&storage, &job->pcm, job->channels, job->output_rate, &permutation) &&
		storage.used == job->adpcm_size;
	job->run_milliseconds = ae_worker_milliseconds() - start;
	return 1;
}

/* the next Vorbis permutation mcc_audio_prepare will decode, read and submitted; 0 when the walk ends, or the pool has
no room. The walk follows mcc_audio_prepare's loops and stops where that function stops a load */
static int ae_audio_submit_next(void)
{
	struct mcc_runtime *runtime = ae_audio.runtime;

	while (!ae_audio.walked && ae_audio.tag < runtime->report.tag_count)
	{
		uint32_t *entry = (uint32_t *)(runtime->tag_index + ae_audio.tag * 0x20);
		struct sound_definition *sound;
		struct sound_pitch_range *range;
		struct sound_permutation *permutation;
		struct ae_audio_job *job;
		char const *tag_name;
		char resource_name[288], group_name[288];
		int channels, ok;
		uint32_t rate;

		if (entry[0] != (uint32_t)SOUND_DEFINITION_TAG)
		{
			ae_audio.tag++;
			ae_audio.range = ae_audio.permutation = 0;
			continue;
		}
		sound = mcc_runtime_pointer(runtime, entry[5], sizeof(*sound));
		if (!sound || sound->encoding < 0 || sound->encoding > 1 || sound->sample_rate < NONE || sound->sample_rate > 1)
		{
			ae_audio.walked = 1;
			break;
		}
		if (ae_audio.range >= (uint32_t)sound->pitch_ranges.count)
		{
			ae_audio.tag++;
			ae_audio.range = ae_audio.permutation = 0;
			continue;
		}
		range = mcc_runtime_pointer(runtime, (uint32_t)sound->pitch_ranges.address + ae_audio.range * sizeof(*range),
			sizeof(*range));
		if (!range)
		{
			ae_audio.walked = 1;
			break;
		}
		if (ae_audio.permutation >= (uint32_t)range->permutations.count)
		{
			ae_audio.range++;
			ae_audio.permutation = 0;
			continue;
		}
		permutation = mcc_runtime_pointer(runtime,
			(uint32_t)range->permutations.address + ae_audio.permutation * sizeof(*permutation), sizeof(*permutation));
		if (!permutation || permutation->samples.size < 0 || permutation->samples.size > 0x4000000)
		{
			ae_audio.walked = 1;
			break;
		}
		if (!permutation->samples.size || permutation->compression != 3)
		{
			ae_audio.permutation++;
			continue;
		}
		if (sound->sample_rate == NONE)
		{
			ae_audio.walked = 1;
			break;
		}
		if (!ae_mcc_convert_pool_room((unsigned int)permutation->samples.size))
			return 0;
		channels = sound->encoding + 1;
		rate = sound->sample_rate == 1 ? 44100 : 22050;
		job = (struct ae_audio_job *)ae_mcc_convert_alloc(sizeof(*job));
		if (!job)
			return 0;
		memset(job, 0, sizeof(*job));
		job->channels = channels;
		job->rate = rate;
		job->output_rate = channels == 1 ? 22050 : rate;
		job->size = (uint32_t)permutation->samples.size;
		job->input = (unsigned char *)ae_mcc_convert_alloc(job->size);
		if (!job->input)
		{
			ae_audio_job_free(job);
			return 0;
		}
		if (permutation->samples.pad & 1)
		{
			tag_name = mcc_runtime_pointer(runtime, entry[4], 256);
			if (!tag_name || !memchr(tag_name, 0, 256))
				tag_name = "(unavailable)";
			snprintf(resource_name, sizeof(resource_name), "%s__%lu__%lu", tag_name, (unsigned long)ae_audio.range,
				(unsigned long)ae_audio.permutation);
			snprintf(group_name, sizeof(group_name), "%s__permutations", tag_name);
			ok = mcc_resources_read(runtime, 2, resource_name, permutation->samples.file_offset,
				permutation->samples.size, job->input, group_name);
		}
		else
			ok = mcc_runtime_read(runtime, permutation->samples.file_offset, permutation->samples.size, job->input);
		ae_audio.permutation++;
		/* (a payload that cannot be read is left to mcc_audio_prepare, which reports it) */
		if (!ok)
		{
			ae_audio.read_failed++;
			ae_audio_job_free(job);
			continue;
		}
		if (!ae_mcc_convert_pool_submit(ae_audio_run, job, job->size))
		{
			ae_audio_job_free(job);
			return 0;
		}
		return 1;
	}
	return 0;
}

static void ae_audio_top_up(void)
{
	while (ae_audio.on && ae_audio_submit_next())
	{
	}
}

static void ae_audio_drain(void)
{
	int left;

	if (ae_audio.taken)
		ae_audio_job_free(ae_audio.taken);
	ae_audio.taken = NULL;
	if (!ae_audio.on)
		return;
	ae_audio.on = 0;
	left = ae_mcc_convert_pool_stop(AE_AUDIO_DRAIN_MOST, ae_audio_job_free);
	if (left)
		error(_error_silent, "mcc audio: %d sound conversion jobs still running at the drain are left to end", left);
}

void ae_mcc_audio_pool_begin(struct mcc_runtime *runtime)
{
	long setting = config_integer("debug.mcc_convert_threads");
	int workers;

	ae_audio_drain();
	memset(&ae_audio, 0, sizeof(ae_audio));
	ae_audio.runtime = runtime;
	if (setting < 0)
		workers = ae_worker_cores() - 2 < AE_AUDIO_WORKERS_AUTO ? ae_worker_cores() - 2 : AE_AUDIO_WORKERS_AUTO;
	else
		workers = setting > AE_MCC_CONVERT_WORKERS_MOST ? AE_MCC_CONVERT_WORKERS_MOST : (int)setting;
	if (workers <= 0)
		return;
	ae_worker_main_set();
	ae_audio.workers = ae_mcc_convert_pool_start(workers, workers * 2, AE_AUDIO_HELD_MOST);
	if (!ae_audio.workers)
	{
		error(_error_silent, "mcc audio: no sound conversion workers could start; converting on this thread");
		return;
	}
	ae_audio.on = 1;
	error(_error_silent, "mcc audio: Ogg Vorbis sounds converted on %d worker threads (debug.mcc_convert_threads %ld)",
		ae_audio.workers, setting);
	ae_audio_top_up();
}

int ae_mcc_audio_pcm_take(unsigned char const *bytes, uint32_t size, int compression, int channels, uint32_t rate,
	struct mcc_pcm *pcm)
{
	struct ae_audio_job *job;
	int result;

	if (!ae_worker_on_main() || !ae_audio.on)
		return -1;
	/* (a result whose store did not come: its pcm was the caller's and is closed; the ADPCM is let go) */
	if (ae_audio.taken)
		ae_audio_job_free(ae_audio.taken);
	ae_audio.taken = NULL;
	ae_audio_top_up();
	if (compression != 3)
		return -1;
	job = (struct ae_audio_job *)ae_mcc_convert_pool_oldest();
	if (!job)
	{
		ae_audio.here++;
		return -1;
	}
	if (job->size != size || job->channels != channels || job->rate != rate || memcmp(job->input, bytes, size))
	{
		error(_error_silent, "mcc audio: a worker's sound is not the one this thread reached; the rest are converted "
			"on this thread");
		ae_audio.out_of_step = 1;
		ae_audio.here++;
		ae_audio_drain();
		return -1;
	}
	{
		unsigned int start = ae_worker_milliseconds();

		result = ae_mcc_convert_pool_wait(AE_AUDIO_WAIT_MOST, mcc_loading_pulse);
		ae_audio.waited += ae_worker_milliseconds() - start;
	}
	if (result == -1)
	{
		memset(pcm, 0, sizeof(*pcm));
		pcm->channels = channels;
		pcm->rate = rate;
		pcm->failure = "Ogg Vorbis conversion on a worker thread did not finish within 30 seconds";
		return 0;
	}
	job = (struct ae_audio_job *)ae_mcc_convert_pool_take();
	ae_audio_top_up();
	if (!job || result != 1)
	{
		ae_audio_job_free(job);
		ae_audio.here++;
		return -1;
	}
	/* (the samples are the caller's now: mcc_pcm_close lets them go to mcc_vorbis, which made them) */
	*pcm = job->pcm;
	memset(&job->pcm, 0, sizeof(job->pcm));
	ae_mcc_convert_free(job->input);
	job->input = NULL;
	job->pcm.rate = pcm->rate;
	job->pcm.frames = pcm->frames;
	job->pcm.channels = pcm->channels;
	ae_audio.taken = job;
	ae_audio.from_workers++;
	if (job->run_milliseconds > ae_audio.longest)
		ae_audio.longest = job->run_milliseconds;
	return 1;
}

int ae_mcc_audio_adpcm_take(struct mcc_pcm const *pcm, int channels, uint32_t rate, unsigned char *out, uint32_t size)
{
	struct ae_audio_job *job;
	int ok;

	if (!ae_worker_on_main() || !(job = ae_audio.taken))
		return 0;
	ae_audio.taken = NULL;
	ok = job->adpcm_ok && pcm->frames == job->pcm.frames && pcm->rate == job->pcm.rate &&
		pcm->channels == job->pcm.channels && channels == job->channels && rate == job->output_rate &&
		size == job->adpcm_size;
	if (ok)
		memcpy(out, job->adpcm, size);
	ae_audio_job_free(job);
	return ok;
}

void ae_mcc_audio_pool_end(void)
{
	ae_audio_drain();
}

void ae_mcc_audio_pool_done(struct mcc_runtime *runtime)
{
	int workers = ae_audio.on ? ae_audio.workers : 0, jobs_most = 0;
	unsigned int bytes_most = 0;

	if (ae_audio.on || ae_audio.out_of_step)
	{
		ae_mcc_convert_pool_held(NULL, NULL, &jobs_most, &bytes_most);
		ae_audio_drain();
		error(_error_silent, "mcc audio: %lu Ogg Vorbis permutations converted on workers, %lu on this thread "
			"(%d workers, most held %d jobs, %u KB; longest job %u ms, this thread waited %u ms)",
			(unsigned long)ae_audio.from_workers, (unsigned long)ae_audio.here, workers, jobs_most, bytes_most >> 10,
			ae_audio.longest, ae_audio.waited);
	}
	if (config_boolean("debug.mcc_convert_digest") && runtime->audio)
	{
		struct mcc_audio_tags *audio = runtime->audio;
		uLong crc = crc32(0L, Z_NULL, 0);
		uint32_t index;

		for (index = 0; index < audio->count; index++)
		{
			if (audio->items[index].bytes && audio->items[index].used)
				crc = crc32(crc, audio->items[index].bytes, (uInt)audio->items[index].used);
		}
		error(_error_silent, "mcc audio: converted sounds CRC %08lx over %lu bytes", (unsigned long)crc,
			(unsigned long)audio->used);
	}
}
