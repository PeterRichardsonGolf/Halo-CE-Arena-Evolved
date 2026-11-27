/* ae_forge_net_rules.h */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_forge_net_rules.h"

void ae_forge_net_rules_limits(struct ae_forge_net_limits *limits, int live_max, int object_max)
{
	limits->bound = 10000.0f;
	limits->live_max = live_max;
	limits->object_max = object_max;
	limits->effect_count = 5;
}

int ae_forge_net_rules_text(char const *line, char *out)
{
	int each;

	out[0] = 0;
	if (!line)
		return 0;
	for (each = 0; each < AE_FORGE_NET_TEXT_SIZE; each++)
	{
		if (!line[each])
		{
			memcpy(out, line, (size_t)each + 1);
			return 1;
		}
	}
	return 0;
}

/* ---------- reading a line's fields */

enum { MAX_FIELDS = 12, FIELD_SIZE = 40 };

struct fields
{
	int count;
	char text[MAX_FIELDS][FIELD_SIZE];
};

/* the text after the tag split at its colons: 0 with more fields than there can be, or one too long */
static int split(char const *text, struct fields *fields)
{
	int length = 0;

	fields->count = 0;
	if (!*text)
		return 1;
	for (;;)
	{
		if (fields->count >= MAX_FIELDS)
			return 0;
		if (*text == ':' || !*text)
		{
			fields->text[fields->count][length] = 0;
			fields->count++;
			length = 0;
			if (!*text)
				return 1;
		}
		else
		{
			if (length >= FIELD_SIZE - 1)
				return 0;
			fields->text[fields->count][length++] = *text;
		}
		text++;
	}
}

static int whole(char const *text, long low, long high, long *value, int *reason)
{
	char *end;
	long number;
	char const *at = text;

	if (*at == '-')
		at++;
	if (!*at)
	{
		*reason = AE_FORGE_NET_NUMBER;
		return 0;
	}
	for (; *at; at++)
	{
		if (*at < '0' || *at > '9')
		{
			*reason = AE_FORGE_NET_NUMBER;
			return 0;
		}
	}
	if (strlen(text) > 11)
	{
		*reason = AE_FORGE_NET_RANGE;
		return 0;
	}
	number = strtol(text, &end, 10);
	if (*end || number < low || number > high)
	{
		*reason = *end ? AE_FORGE_NET_NUMBER : AE_FORGE_NET_RANGE;
		return 0;
	}
	*value = number;
	return 1;
}

static int hex(char const *text, unsigned long high, unsigned long *value, int *reason)
{
	char const *at;
	unsigned long number;
	char *end;

	if (!*text || strlen(text) > 8)
	{
		*reason = *text ? AE_FORGE_NET_RANGE : AE_FORGE_NET_NUMBER;
		return 0;
	}
	for (at = text; *at; at++)
	{
		if (!((*at >= '0' && *at <= '9') || (*at >= 'a' && *at <= 'f') || (*at >= 'A' && *at <= 'F')))
		{
			*reason = AE_FORGE_NET_NUMBER;
			return 0;
		}
	}
	number = strtoul(text, &end, 16);
	if (*end || number > high)
	{
		*reason = *end ? AE_FORGE_NET_NUMBER : AE_FORGE_NET_RANGE;
		return 0;
	}
	*value = number;
	return 1;
}

/* a real read by hand (no locale: a comma locale reads none): [-]digits[.digits], at most 12 digits in all; low..high inclusive */
static int real_range(char const *text, float low, float high, float *value, int *reason)
{
	char const *at = text;
	int negative = 0, digits = 0, seen_point = 0;
	double number = 0.0, scale = 1.0;

	if (*at == '-')
	{
		negative = 1;
		at++;
	}
	if (!*at)
	{
		*reason = AE_FORGE_NET_NUMBER;
		return 0;
	}
	for (; *at; at++)
	{
		if (*at == '.' && !seen_point)
		{
			seen_point = 1;
		}
		else if (*at >= '0' && *at <= '9')
		{
			if (++digits > 12)
			{
				*reason = AE_FORGE_NET_RANGE;
				return 0;
			}
			if (seen_point)
			{
				scale /= 10.0;
				number += (double)(*at - '0') * scale;
			}
			else
				number = number * 10.0 + (double)(*at - '0');
		}
		else
		{
			*reason = (*at == 'n' || *at == 'N' || *at == 'i' || *at == 'I') ? AE_FORGE_NET_FINITE : AE_FORGE_NET_NUMBER;
			return 0;
		}
	}
	if (!digits)
	{
		*reason = AE_FORGE_NET_NUMBER;
		return 0;
	}
	if (negative)
		number = -number;
	if (!__builtin_isfinite(number) || number < (double)low || number > (double)high)
	{
		*reason = __builtin_isfinite(number) ? AE_FORGE_NET_RANGE : AE_FORGE_NET_FINITE;
		return 0;
	}
	*value = (float)number;
	return 1;
}

/* the fields in turn */
struct reader
{
	struct fields const *fields;
	struct ae_forge_net_limits const *limits;
	struct ae_forge_net_record *out;
	int at;
	int ok;
};

#define LIVE_RANGE(r) 0, (long)(r)->limits->live_max - 1

static int next_whole(struct reader *r, long low, long high)
{
	long value = 0;

	if (!r->ok)
		return 0;
	if (r->at >= r->fields->count || !whole(r->fields->text[r->at], low, high, &value, &r->out->reason))
	{
		if (r->at >= r->fields->count)
			r->out->reason = AE_FORGE_NET_FIELDS;
		r->ok = 0;
		return 0;
	}
	r->at++;
	return (int)value;
}

static unsigned int next_hex(struct reader *r, unsigned long high)
{
	unsigned long value = 0;

	if (!r->ok)
		return 0;
	if (r->at >= r->fields->count || !hex(r->fields->text[r->at], high, &value, &r->out->reason))
	{
		if (r->at >= r->fields->count)
			r->out->reason = AE_FORGE_NET_FIELDS;
		r->ok = 0;
		return 0;
	}
	r->at++;
	return (unsigned int)value;
}

static float next_real_range(struct reader *r, float low, float high)
{
	float value = 0.0f;

	if (!r->ok)
		return 0.0f;
	if (r->at >= r->fields->count || !real_range(r->fields->text[r->at], low, high, &value, &r->out->reason))
	{
		if (r->at >= r->fields->count)
			r->out->reason = AE_FORGE_NET_FIELDS;
		r->ok = 0;
		return 0.0f;
	}
	r->at++;
	return value;
}

static float next_real(struct reader *r)
{
	return next_real_range(r, -r->limits->bound, r->limits->bound);
}

/* every field read, and no more */
static int finished(struct reader *r, int *fields_given)
{
	if (!r->ok)
		return 0;
	if (fields_given)
		*fields_given = r->at;
	if (r->at != r->fields->count)
	{
		r->out->reason = AE_FORGE_NET_FIELDS;
		return 0;
	}
	return 1;
}

static struct
{
	char const *name;
	int tag;
} const tags[AE_FORGE_NET_TAGS] =
{
	{ "FT", AE_FORGE_NET_FT }, { "FE", AE_FORGE_NET_FE }, { "FQ", AE_FORGE_NET_FQ }, { "FS", AE_FORGE_NET_FS }, { "FH", AE_FORGE_NET_FH },
	{ "FM", AE_FORGE_NET_FM }, { "FD", AE_FORGE_NET_FD }, { "FU", AE_FORGE_NET_FU }, { "FA", AE_FORGE_NET_FA }, { "FJ", AE_FORGE_NET_FJ },
	{ "FV", AE_FORGE_NET_FV }, { "FW", AE_FORGE_NET_FW }, { "FN", AE_FORGE_NET_FN }, { "FK", AE_FORGE_NET_FK }, { "FZ", AE_FORGE_NET_FZ },
	{ "FB", AE_FORGE_NET_FB }, { "FX", AE_FORGE_NET_FX },
	{ "FGN", AE_FORGE_NET_FGN }, { "FGT", AE_FORGE_NET_FGT }, { "FGL", AE_FORGE_NET_FGL }, { "FGM", AE_FORGE_NET_FGM }, { "FGV", AE_FORGE_NET_FGV },
	{ "FGA", AE_FORGE_NET_FGA },
};

char const *ae_forge_net_rules_tag_name(int tag)
{
	return tag >= 0 && tag < AE_FORGE_NET_TAGS ? tags[tag].name : "";
}

#define ANGLE 100000

int ae_forge_net_rules_parse(char const *text, struct ae_forge_net_limits const *limits, struct ae_forge_net_record *out)
{
	struct fields fields;
	struct reader reader;
	int tag = AE_FORGE_NET_NONE, each;
	char const *body;

	memset(out, 0, sizeof(*out));
	out->tag = AE_FORGE_NET_NONE;
	out->obj_abs = -1;
	out->goal = out->link = out->who = out->start = out->follow = out->alert = -1;
	if (!text || text[0] != '!' || text[1] != 'F')
	{
		out->reason = AE_FORGE_NET_NOT_FORGE;
		return AE_FORGE_NET_NONE;
	}
	for (each = 0; each < AE_FORGE_NET_TAGS; each++)
	{
		/* (the two-letter tags: the third byte of the line; the settings' three-letter ones: `FG` and their own) */
		if (tags[each].name[2] == 0 ? text[2] == tags[each].name[1] : text[2] == 'G' && text[3] == tags[each].name[2])
			tag = tags[each].tag;
	}
	if (text[2] == 'G' && tag == AE_FORGE_NET_NONE)
	{
		out->reason = AE_FORGE_NET_UNKNOWN_TAG;
		return AE_FORGE_NET_NONE;
	}
	if (tag == AE_FORGE_NET_NONE)
	{
		out->reason = AE_FORGE_NET_UNKNOWN_TAG;
		return AE_FORGE_NET_NONE;
	}
	/* "!FQ" is the whole of its line; the others have a colon and their fields */
	if (tag == AE_FORGE_NET_FQ)
	{
		if (text[3])
		{
			out->reason = AE_FORGE_NET_FIELDS;
			return AE_FORGE_NET_NONE;
		}
		out->tag = tag;
		return tag;
	}
	{
		/* (`!FM:` and the like: the colon the third byte; `!FGN:`: the fourth) */
		int colon = tags[tag].name[2] ? 4 : 3;

		if (text[colon] != ':')
		{
			out->reason = AE_FORGE_NET_FIELDS;
			return AE_FORGE_NET_NONE;
		}
		body = text + colon + 1;
	}
	if (tag == AE_FORGE_NET_FGN)
	{
		/* the display name: printable bytes, 40 at most (all that follows the colon, colons too) */
		int length = 0;

		for (; body[length]; length++)
		{
			if (length >= 40 || body[length] < 32 || body[length] > 126)
			{
				out->reason = AE_FORGE_NET_RANGE;
				return AE_FORGE_NET_NONE;
			}
		}
		memcpy(out->name, body, (size_t)length + 1);
		out->tag = tag;
		out->reason = AE_FORGE_NET_OK;
		return tag;
	}
	if (!split(body, &fields))
	{
		out->reason = AE_FORGE_NET_FIELDS;
		return AE_FORGE_NET_NONE;
	}
	reader.fields = &fields;
	reader.limits = limits;
	reader.out = out;
	reader.at = 0;
	reader.ok = 1;
	switch (tag)
	{
	case AE_FORGE_NET_FT:
		out->player = next_whole(&reader, 0, AE_FORGE_NET_MAX_PLAYERS - 1);
		out->on = next_whole(&reader, 0, 1);
		out->x = next_real(&reader);
		out->y = next_real(&reader);
		out->z = next_real(&reader);
		out->yaw = next_whole(&reader, -ANGLE, ANGLE);
		out->pitch = next_whole(&reader, -ANGLE, ANGLE);
		break;
	case AE_FORGE_NET_FE:
		out->effect = next_hex(&reader, (unsigned long)limits->effect_count - 1);
		out->x = next_real(&reader);
		out->y = next_real(&reader);
		out->z = next_real(&reader);
		break;
	case AE_FORGE_NET_FS:
		out->hash = next_hex(&reader, 0xFFFFFFFFul);
		out->x = next_real(&reader);
		out->y = next_real(&reader);
		out->z = next_real(&reader);
		out->yaw = next_whole(&reader, -ANGLE, ANGLE);
		out->pitch = next_whole(&reader, -ANGLE, ANGLE);
		out->roll = next_whole(&reader, -ANGLE, ANGLE);
		out->team = next_whole(&reader, -32768, 32767);
		break;
	case AE_FORGE_NET_FH:
		out->live_idx = (int)next_hex(&reader, (unsigned long)limits->live_max - 1);
		out->hash = next_hex(&reader, 0xFFFFFFFFul);
		out->object_code = next_hex(&reader, 0x1FFFFFFul);
		out->x = next_real(&reader);
		out->y = next_real(&reader);
		out->z = next_real(&reader);
		out->yaw = next_whole(&reader, -ANGLE, ANGLE);
		out->pitch = next_whole(&reader, -ANGLE, ANGLE);
		out->roll = next_whole(&reader, -ANGLE, ANGLE);
		out->team = next_whole(&reader, -32768, 32767);
		break;
	case AE_FORGE_NET_FM:
		out->live_idx = next_whole(&reader, -1, limits->live_max - 1);
		out->obj_abs = next_whole(&reader, -1, limits->object_max - 1);
		out->x = next_real(&reader);
		out->y = next_real(&reader);
		out->z = next_real(&reader);
		out->yaw = next_whole(&reader, -ANGLE, ANGLE);
		out->pitch = next_whole(&reader, -ANGLE, ANGLE);
		out->roll = next_whole(&reader, -ANGLE, ANGLE);
		/* (the held flag last: a peer of before sends none) */
		if (reader.ok && reader.at < fields.count)
			out->held = next_whole(&reader, 0, 1);
		break;
	case AE_FORGE_NET_FD:
		out->live_idx = next_whole(&reader, -1, limits->live_max - 1);
		if (reader.ok && reader.at < fields.count)
			out->obj_abs = next_whole(&reader, -1, limits->object_max - 1);
		break;
	case AE_FORGE_NET_FU:
		out->live_idx = next_whole(&reader, 0, limits->live_max - 1);
		out->usage = next_whole(&reader, 0, 65535);
		break;
	case AE_FORGE_NET_FV:
	case AE_FORGE_NET_FN:
	case AE_FORGE_NET_FK:
		out->live_idx = next_whole(&reader, 0, limits->live_max - 1);
		out->on = next_whole(&reader, 0, 1);
		break;
	case AE_FORGE_NET_FZ:
		out->live_idx = next_whole(&reader, 0, limits->live_max - 1);
		out->thousandths = next_whole(&reader, 50, 20000);
		break;
	case AE_FORGE_NET_FW:
		out->live_idx = next_whole(&reader, 0, limits->live_max - 1);
		out->shape = next_whole(&reader, 0, 3);
		out->effect = next_whole(&reader, 0, 7);
		out->damage = next_whole(&reader, 0, 1000);
		out->size[0] = next_whole(&reader, 25, 20000);
		out->size[1] = next_whole(&reader, 25, 20000);
		out->size[2] = next_whole(&reader, 25, 20000);
		/* (the flags last: a build of before sends none) */
		if (reader.ok && reader.at < fields.count)
			out->flags = next_whole(&reader, 0, 255);
		break;
	case AE_FORGE_NET_FA:
		out->live_idx = (int)next_hex(&reader, (unsigned long)limits->live_max - 1);
		out->hash = next_hex(&reader, 0xFFFFFFFFul);
		out->weapon_hash = next_hex(&reader, 0xFFFFFFFFul);
		out->team = (int)next_hex(&reader, AE_FORGE_NET_AI_TEAM_MAX);
		out->unit_count = (int)next_hex(&reader, AE_FORGE_NET_AI_UNIT_COUNT);
		out->respawn_alive = (int)next_hex(&reader, AE_FORGE_NET_AI_UNIT_COUNT);
		out->respawn_delay = (int)next_hex(&reader, AE_FORGE_NET_AI_RESPAWN_SECONDS);
		out->respawn_total = (int)next_hex(&reader, 999);
		out->gametypes = (int)next_hex(&reader, 0xFFFFul);
		out->detached = (int)next_hex(&reader, 1);
		out->spawn_effect = (int)next_hex(&reader, 0x1FFFul);
		break;
	case AE_FORGE_NET_FJ:
		out->live_idx = (int)next_hex(&reader, (unsigned long)limits->live_max - 1);
		out->goal = (int)next_hex(&reader, 4);
		out->link = (int)next_hex(&reader, 31);
		out->who = (int)next_hex(&reader, 0x8F);
		out->start = (int)next_hex(&reader, 255);
		out->follow = (int)next_hex(&reader, 255);
		/* (ALERT TIME last: a peer of before reads the six it knows) */
		if (reader.ok && reader.at < fields.count)
			out->alert = (int)next_hex(&reader, 255);
		break;
	case AE_FORGE_NET_FGT:
		out->kind = next_whole(&reader, 0, 2);
		out->slot = next_whole(&reader, 0, 2);
		out->object_code = next_hex(&reader, 0xFFFFFFFFul);
		break;
	case AE_FORGE_NET_FGL:
		out->slot = next_whole(&reader, 0, 2);
		out->team = next_whole(&reader, 0, 3);
		break;
	case AE_FORGE_NET_FGM:
		out->slot = next_whole(&reader, 0, 2);
		out->usage = next_whole(&reader, 0, 2);
		break;
	case AE_FORGE_NET_FGV:
		out->fog_start = next_real_range(&reader, 0.0f, 16384.0f);
		out->fog_opaque = next_real_range(&reader, 0.0f, 32.0f);
		break;
	case AE_FORGE_NET_FGA:
		out->atmosphere = next_whole(&reader, 0, 5);
		out->color = (int)next_hex(&reader, 0xFFFFFFul);
		out->density = next_whole(&reader, 0, 100);
		out->fog_start = next_real_range(&reader, 0.0f, 100000.0f);
		out->fog_opaque = next_real_range(&reader, 0.0f, 100000.0f);
		out->indoors = next_whole(&reader, 0, 1);
		out->sky = next_whole(&reader, 0, 2);
		break;
	case AE_FORGE_NET_FB:
		out->base_count = next_hex(&reader, 0xFFFFFFul);
		out->fingerprint = next_hex(&reader, 0xFFFFFFFFul);
		break;
	case AE_FORGE_NET_FX:
		out->effect = next_whole(&reader, 0, 99);
		break;
	default:
		out->reason = AE_FORGE_NET_UNKNOWN_TAG;
		return AE_FORGE_NET_NONE;
	}
	if (tag == AE_FORGE_NET_FJ && reader.ok && (out->who & 0x0F) > 2)
	{
		out->reason = AE_FORGE_NET_RANGE;
		return AE_FORGE_NET_NONE;
	}
	if (!finished(&reader, &out->fields))
		return AE_FORGE_NET_NONE;
	out->tag = tag;
	out->reason = AE_FORGE_NET_OK;
	return tag;
}

/* ---------- who may say what */

int ae_forge_net_rules_client_may_say(int tag)
{
	switch (tag)
	{
	case AE_FORGE_NET_FT: case AE_FORGE_NET_FQ: case AE_FORGE_NET_FS: case AE_FORGE_NET_FM: case AE_FORGE_NET_FD: case AE_FORGE_NET_FU:
	case AE_FORGE_NET_FA: case AE_FORGE_NET_FJ: case AE_FORGE_NET_FV: case AE_FORGE_NET_FW: case AE_FORGE_NET_FN: case AE_FORGE_NET_FK:
	case AE_FORGE_NET_FZ: case AE_FORGE_NET_FX:
		return 1;
	default:
		return 0;
	}
}

int ae_forge_net_rules_host_may_say(int tag)
{
	switch (tag)
	{
	case AE_FORGE_NET_FGN: case AE_FORGE_NET_FGT: case AE_FORGE_NET_FGL: case AE_FORGE_NET_FGM: case AE_FORGE_NET_FGV: case AE_FORGE_NET_FGA:
	case AE_FORGE_NET_FT: case AE_FORGE_NET_FE: case AE_FORGE_NET_FH: case AE_FORGE_NET_FM: case AE_FORGE_NET_FD: case AE_FORGE_NET_FU:
	case AE_FORGE_NET_FA: case AE_FORGE_NET_FJ: case AE_FORGE_NET_FV: case AE_FORGE_NET_FW: case AE_FORGE_NET_FN: case AE_FORGE_NET_FK:
	case AE_FORGE_NET_FZ: case AE_FORGE_NET_FB: case AE_FORGE_NET_FX:
		return 1;
	default:
		return 0;
	}
}

int ae_forge_net_rules_is_edit(int tag)
{
	switch (tag)
	{
	case AE_FORGE_NET_FS: case AE_FORGE_NET_FM: case AE_FORGE_NET_FD: case AE_FORGE_NET_FU: case AE_FORGE_NET_FA: case AE_FORGE_NET_FJ:
	case AE_FORGE_NET_FV: case AE_FORGE_NET_FW: case AE_FORGE_NET_FN: case AE_FORGE_NET_FK: case AE_FORGE_NET_FZ:
		return 1;
	default:
		return 0;
	}
}

int ae_forge_net_rules_object_ok(int from_client, int obj_abs, int instance_obj_abs)
{
	if (!from_client || obj_abs < 0)
		return 1;
	return obj_abs == instance_obj_abs;
}

int ae_forge_net_rules_may_edit(int from_client, int host_only)
{
	return !from_client || !host_only;
}

/* ---------- rates and budgets */

void ae_forge_net_rules_bucket_refill(struct ae_forge_net_bucket *bucket, unsigned long now_ms, double rate, double burst)
{
	if (!bucket->started)
	{
		bucket->started = 1;
		bucket->tokens = burst;
		bucket->last_ms = now_ms;
	}
	else if (now_ms >= bucket->last_ms)
	{
		bucket->tokens += (double)(now_ms - bucket->last_ms) * rate / 1000.0;
		if (bucket->tokens > burst)
			bucket->tokens = burst;
		bucket->last_ms = now_ms;
	}
	else
	{
		/* (the clock went back: start over) */
		bucket->last_ms = now_ms;
	}
}

int ae_forge_net_rules_bucket_take(struct ae_forge_net_bucket *bucket, unsigned long now_ms, double rate, double burst)
{
	ae_forge_net_rules_bucket_refill(bucket, now_ms, rate, burst);
	if (bucket->tokens < 1.0)
		return 0;
	bucket->tokens -= 1.0;
	return 1;
}

int ae_forge_net_rules_line_bucket(int tag)
{
	return tag == AE_FORGE_NET_FT ? AE_FORGE_NET_BUCKET_POSE : tag == AE_FORGE_NET_FQ ? AE_FORGE_NET_BUCKET_SYNC : AE_FORGE_NET_BUCKET_EDIT;
}

void ae_forge_net_rules_bucket_rate(int bucket, double *rate, double *burst)
{
	switch (bucket)
	{
	case AE_FORGE_NET_BUCKET_POSE:
		*rate = 25.0;
		*burst = 50.0;
		break;
	case AE_FORGE_NET_BUCKET_SYNC:
		/* (one full feed a join: a second request a minute after, at the soonest) */
		*rate = 1.0 / 60.0;
		*burst = 1.0;
		break;
	default:
		*rate = 32.0;
		*burst = 64.0;
		break;
	}
}

void ae_forge_net_rules_client_reset(struct ae_forge_net_client *client)
{
	memset(client, 0, sizeof(*client));
}

int ae_forge_net_rules_client_line(struct ae_forge_net_client *client, int tag, unsigned long now_ms, int *drop)
{
	int bucket = ae_forge_net_rules_line_bucket(tag), taken;
	double rate, burst, before;

	*drop = 0;
	ae_forge_net_rules_bucket_rate(bucket, &rate, &burst);
	/* (what the bucket held as the line came: a bucket that had half its burst to spare, or its one token, is not over its rate) */
	ae_forge_net_rules_bucket_refill(&client->buckets[bucket], now_ms, rate, burst);
	before = client->buckets[bucket].tokens;
	taken = ae_forge_net_rules_bucket_take(&client->buckets[bucket], now_ms, rate, burst);
	if (taken)
	{
		if (before >= (burst * 0.5 > 1.0 ? burst * 0.5 : 1.0))
			client->over_since_ms[bucket] = 0;
		return 1;
	}
	if (!client->over_since_ms[bucket])
		client->over_since_ms[bucket] = now_ms ? now_ms : 1;
	/* (this bucket over its rate this long: the clock is a 32-bit one on some builds, so the difference is taken as unsigned of that width. A strike that
	another bucket left, long ago, is that bucket's) */
	{
		unsigned int age = (unsigned int)now_ms - (unsigned int)client->over_since_ms[bucket];

		if (age >= (unsigned int)AE_FORGE_NET_STRIKE_MS && age < 0x80000000u)
			*drop = 1;
	}
	return 0;
}

void ae_forge_net_rules_client_machine_left(struct ae_forge_net_client *client)
{
	memset(&client->buckets[AE_FORGE_NET_BUCKET_SYNC], 0, sizeof(client->buckets[AE_FORGE_NET_BUCKET_SYNC]));
	client->over_since_ms[AE_FORGE_NET_BUCKET_SYNC] = 0;
}

int ae_forge_net_rules_client_instance(struct ae_forge_net_client *client, int session_instances)
{
	if (client->instances >= AE_FORGE_NET_CLIENT_INSTANCES || session_instances >= AE_FORGE_NET_SESSION_INSTANCES)
		return 0;
	client->instances++;
	return 1;
}

int ae_forge_net_rules_client_ai(struct ae_forge_net_client *client, int units)
{
	if (units < 0 || client->ai_spawns >= AE_FORGE_NET_CLIENT_AI_SPAWNS || client->ai_units + units > AE_FORGE_NET_CLIENT_AI_UNITS)
		return 0;
	client->ai_spawns++;
	client->ai_units += units;
	return 1;
}

void ae_forge_net_rules_client_removed(struct ae_forge_net_client *client, int was_ai, int units)
{
	if (client->instances > 0)
		client->instances--;
	if (was_ai)
	{
		if (client->ai_spawns > 0)
			client->ai_spawns--;
		client->ai_units = client->ai_units >= units ? client->ai_units - units : 0;
	}
}

int ae_forge_net_rules_joiner_create(struct ae_forge_net_bucket *bucket, unsigned long now_ms)
{
	return ae_forge_net_rules_bucket_take(bucket, now_ms, 180.0, 360.0);
}

/* ---------- who may touch what */

int ae_forge_net_rules_client_touch(struct ae_forge_net_facts const *facts, int tag, int obj_abs, int held, int sender, int clients_edit_map,
	unsigned long now_ms)
{
	int last;

	if (!facts->active)
		return AE_FORGE_NET_TOUCH_INACTIVE;
	if (!ae_forge_net_rules_object_ok(1, obj_abs, facts->object_abs))
		return AE_FORGE_NET_TOUCH_OBJECT;
	if (facts->map_own && !clients_edit_map)
		return AE_FORGE_NET_TOUCH_MAP_OWN;
	if (facts->parented)
		return AE_FORGE_NET_TOUCH_PARENTED;
	if (facts->occupied)
		return AE_FORGE_NET_TOUCH_OCCUPIED;
	/* (what the host holds in its hand, or another client does, is not moved, scaled or deleted from under them; a line that lets go is that holder's alone) */
	if ((tag == AE_FORGE_NET_FM || tag == AE_FORGE_NET_FD || tag == AE_FORGE_NET_FZ) &&
		(facts->host_holds || (facts->held_by && facts->held_by != sender)))
		return AE_FORGE_NET_TOUCH_HELD;
	if (tag == AE_FORGE_NET_FM && held && facts->held_by != sender && facts->sender_holds >= AE_FORGE_NET_HOLD_MAX)
		return AE_FORGE_NET_TOUCH_HELD;
	/* (the last of a class a game needs is kept from a client: not deleted, moved, scaled or changed; and a spawn or flag stays in the level) */
	last = ae_forge_net_rules_last_of_class(facts->class_own_mask, facts->class_others_mask);
	if (ae_forge_net_rules_protected_kind(facts->kind))
	{
		if (last && (tag == AE_FORGE_NET_FD || tag == AE_FORGE_NET_FM || tag == AE_FORGE_NET_FZ || tag == AE_FORGE_NET_FU))
			return AE_FORGE_NET_TOUCH_LAST;
		if (tag == AE_FORGE_NET_FM && !facts->dest_in_level)
			return AE_FORGE_NET_TOUCH_BOUNDS;
	}
	/* (a slot made this moment is its maker's alone: a line about what stood there before is not about it) */
	if (facts->created_ms && facts->creator != sender && (unsigned int)now_ms - (unsigned int)facts->created_ms < (unsigned int)AE_FORGE_NET_FRESH_MS)
		return AE_FORGE_NET_TOUCH_FRESH;
	return AE_FORGE_NET_TOUCH_OK;
}

int ae_forge_net_rules_last_of_class(int own_mask, int others_mask)
{
	int own = own_mask & AE_FORGE_NET_MODES_ALL ? own_mask & AE_FORGE_NET_MODES_ALL : AE_FORGE_NET_MODES_ALL;
	int others = others_mask & AE_FORGE_NET_MODES_ALL;

	return (own & ~others) != 0;
}

int ae_forge_net_rules_protected_kind(int kind)
{
	return kind == AE_FORGE_NET_KIND_SPAWN || kind == AE_FORGE_NET_KIND_FLAG;
}

int ae_forge_net_rules_delete_ok(int kind, int last_of_class)
{
	return !(ae_forge_net_rules_protected_kind(kind) && last_of_class);
}

int ae_forge_net_rules_instance_team_ok(int kind, int flag_type, int team)
{
	/* (a placement with no team sends -1: the placing takes it as 0, or the free-for-all spawn) */
	switch (kind)
	{
	case AE_FORGE_NET_TEAM_SPAWN:
		return team >= -1 && team <= 2;
	case AE_FORGE_NET_TEAM_TELEPORTER:
		return team >= 0 && team <= 15;
	case AE_FORGE_NET_TEAM_AI:
		return ae_forge_net_rules_ai_team_ok(team);
	case AE_FORGE_NET_TEAM_FLAG:
		/* (a capture flag 0, a ball spawn 2, a race track 3, a hill 8: the number the game reads as a team or a place; any other type a team) */
		return flag_type == 2 ? team >= -1 && team <= 15 : (flag_type == 3 || flag_type == 8) ? team >= -1 && team <= 31 : team >= -1 && team <= 1;
	default:
		return team >= -32768 && team <= 32767;
	}
}

int ae_forge_net_rules_ai_team_ok(int team)
{
	return team >= 0 && team <= AE_FORGE_NET_AI_TEAM_MAX;
}

int ae_forge_net_rules_team_ok(int is_ai_spawn, int team)
{
	return ae_forge_net_rules_instance_team_ok(is_ai_spawn ? AE_FORGE_NET_TEAM_AI : AE_FORGE_NET_TEAM_OTHER, 0, team);
}

int ae_forge_net_rules_ai_live(int unit_count, int respawn_alive)
{
	return unit_count > respawn_alive ? unit_count : respawn_alive;
}

int ae_forge_net_rules_ai_budget_ok(int owned_others, int new_live, int session_others)
{
	return new_live >= 0 && owned_others + new_live <= AE_FORGE_NET_CLIENT_AI_UNITS && session_others + new_live <= AE_FORGE_NET_SESSION_AI_LIVE;
}

/* ---------- machines, identities, relay */

void ae_forge_net_rules_machine_reset(struct ae_forge_net_machine *machine)
{
	machine->state = AE_FORGE_NET_MACHINE_NEW;
	machine->identity = -1;
}

int ae_forge_net_rules_machine_accepts(struct ae_forge_net_machine const *machine, int tag, int level_ready)
{
	if (!level_ready)
		return 0;
	return machine->state == AE_FORGE_NET_MACHINE_SYNCED || tag == AE_FORGE_NET_FQ;
}

void ae_forge_net_rules_machine_synced(struct ae_forge_net_machine *machine)
{
	machine->state = AE_FORGE_NET_MACHINE_SYNCED;
}

int ae_forge_net_rules_identity(unsigned long *keys, int *count, int max, unsigned long key)
{
	int each;

	for (each = 0; each < *count; each++)
	{
		if (keys[each] == key)
			return each;
	}
	if (*count >= max)
		return -1;
	keys[*count] = key;
	return (*count)++;
}

unsigned long ae_forge_net_rules_identity_key(unsigned long address, long machine_index)
{
	/* (an address of 0: a tunnel's peer or one that did not resolve: its own, by its place; the high bit marks it, which no IPv4 address read as 32 bits carries
	with a place after it) */
	return address ? address : 0x100000000ul + (unsigned long)(machine_index + 1);
}

int ae_forge_net_rules_identity_evict(unsigned long const *last_seen_ms, unsigned char const *pinned, int count, unsigned long now_ms)
{
	int each, best = -1;
	unsigned int best_age = 0;

	for (each = 0; each < count; each++)
	{
		unsigned int age = (unsigned int)now_ms - (unsigned int)last_seen_ms[each];

		if (pinned[each])
			continue;
		if (best < 0 || age > best_age)
		{
			best = each;
			best_age = age;
		}
	}
	return best;
}

int ae_forge_net_rules_identityless_may(int tag)
{
	return tag == AE_FORGE_NET_FT || tag == AE_FORGE_NET_FQ || tag == AE_FORGE_NET_FX;
}

int ae_forge_net_rules_active_senders(unsigned long const *last_line_ms, int count, unsigned long now_ms)
{
	int each, active = 0;

	for (each = 0; each < count; each++)
	{
		if (last_line_ms[each] && (unsigned int)now_ms - (unsigned int)last_line_ms[each] < 1000u)
			active++;
	}
	return active;
}

int ae_forge_net_rules_feed_allowance(struct ae_forge_net_bucket *bucket, unsigned long now_ms)
{
	ae_forge_net_rules_bucket_refill(bucket, now_ms, 150.0, 24.0);
	return bucket->tokens < 0.0 ? 0 : (int)bucket->tokens;
}

void ae_forge_net_rules_feed_spent(struct ae_forge_net_bucket *bucket, int lines)
{
	bucket->tokens -= (double)lines;
}

int ae_forge_net_rules_pending_put(struct ae_forge_net_pending_table *table, long key, int sender, char const *text, int final)
{
	int each, free_slot = -1;

	for (each = 0; each < AE_FORGE_NET_PENDING_MAX; each++)
	{
		struct ae_forge_net_pending *slot = &table->slot[each];

		if (!slot->valid)
		{
			if (free_slot < 0)
				free_slot = each;
			continue;
		}
		if (slot->key == key)
		{
			/* (a line that lets go is not undone by a move that was sent before it) */
			if (slot->final && !final)
				return 0;
			free_slot = each;
			break;
		}
	}
	if (free_slot < 0)
	{
		if (!final)
			return 0;
		/* (a final line is state: it takes the place of a stream's line) */
		for (each = 0; each < AE_FORGE_NET_PENDING_MAX; each++)
		{
			if (!table->slot[each].final)
			{
				free_slot = each;
				break;
			}
		}
		if (free_slot < 0)
			return 0;
	}
	table->slot[free_slot].valid = 1;
	table->slot[free_slot].key = key;
	table->slot[free_slot].sender = sender;
	table->slot[free_slot].final = final ? 1 : 0;
	snprintf(table->slot[free_slot].text, sizeof(table->slot[free_slot].text), "%s", text);
	return 1;
}

void ae_forge_net_rules_pending_clear(struct ae_forge_net_pending_table *table, long key)
{
	int each;

	for (each = 0; each < AE_FORGE_NET_PENDING_MAX; each++)
	{
		if (table->slot[each].valid && table->slot[each].key == key)
			table->slot[each].valid = 0;
	}
}

int ae_forge_net_rules_pending_flush(struct ae_forge_net_pending_table *table, struct ae_forge_net_relay *relay, int recipients, int active_senders,
	unsigned long now_ms, char *text, int *sender)
{
	int pass, each;

	for (pass = 0; pass < 2; pass++)
	{
		for (each = 0; each < AE_FORGE_NET_PENDING_MAX; each++)
		{
			struct ae_forge_net_pending *slot = &table->slot[each];

			if (!slot->valid || slot->final != (pass == 0))
				continue;
			if (!ae_forge_net_rules_relay_ok(relay, slot->sender, recipients, active_senders, now_ms))
				continue;
			memcpy(text, slot->text, sizeof(slot->text));
			*sender = slot->sender;
			slot->valid = 0;
			return 1;
		}
	}
	return 0;
}

int ae_forge_net_rules_held_list(unsigned char const *held_by, int n, int identity_plus_one, int *out, int max)
{
	int each, count = 0;

	if (identity_plus_one == 0)
		return 0;
	for (each = 0; each < n && count < max; each++)
	{
		if (held_by[each] == identity_plus_one)
			out[count++] = each;
	}
	return count;
}

int ae_forge_net_rules_count_held(unsigned char const *held_by, int n, int identity_plus_one)
{
	int each, count = 0;

	for (each = 0; each < n; each++)
		count += held_by[each] == identity_plus_one && identity_plus_one != 0;
	return count;
}

int ae_forge_net_rules_relay_ok(struct ae_forge_net_relay *relay, int sender, int recipients, int active_senders, unsigned long now_ms)
{
	double share;
	int each;

	if (sender < 0 || sender >= 32 || recipients <= 0)
		return 1;
	if (active_senders < 1)
		active_senders = 1;
	/* (the session's 1500 lines a second to the clients, shared out among the senders: each sender's own bucket refills at its share) */
	share = 1500.0 / (double)active_senders;
	if (!ae_forge_net_rules_bucket_take(&relay->senders[sender], now_ms, share / (double)recipients, 2.0 * share / (double)recipients))
		return 0;
	for (each = 0; each < recipients; each++)
	{
		if (!ae_forge_net_rules_bucket_take(&relay->session, now_ms, 1500.0, 3000.0))
			return 0;
	}
	return 1;
}

int ae_forge_net_rules_format_fm(char *text, int size, int live_idx, int obj_abs, float x, float y, float z, int yaw, int pitch, int roll, int held)
{
	int digits, length = 0;

	for (digits = 2; digits >= 0; digits--)
	{
		length = snprintf(text, (size_t)size, "!FM:%d:%d:%.*f:%.*f:%.*f:%d:%d:%d:%d", live_idx, obj_abs, digits, (double)x, digits, (double)y, digits, (double)z,
			yaw, pitch, roll, held ? 1 : 0);
		if (length > 0 && length < size && length <= AE_FORGE_NET_TEXT_SIZE - 1)
			return length;
	}
	text[0] = 0;
	return 0;
}

int ae_forge_net_rules_format_ft(char *text, int size, int player, int on, float x, float y, float z, int yaw, int pitch)
{
	int digits, length = 0;

	for (digits = 2; digits >= 0; digits--)
	{
		length = snprintf(text, (size_t)size, "!FT:%d:%d:%.*f:%.*f:%.*f:%d:%d", player, on ? 1 : 0, digits, (double)x, digits, (double)y, digits, (double)z, yaw,
			pitch);
		if (length > 0 && length < size && length <= AE_FORGE_NET_TEXT_SIZE - 1)
			return length;
	}
	text[0] = 0;
	return 0;
}

void ae_forge_net_rules_printable(char const *text, char *out, int size)
{
	int each = 0;

	if (size <= 0)
		return;
	for (; text && text[each] && each < size - 1; each++)
		out[each] = (text[each] >= 32 && text[each] <= 126) ? text[each] : '?';
	out[each] = 0;
}

/* ---------- the joiner waits */

int ae_forge_net_rules_compare_state(int state, int host_said, int same, unsigned long asked_ms, unsigned long now_ms)
{
	if (state == AE_FORGE_NET_COMPARE_FAILED)
		return state;
	if (host_said)
		return same ? AE_FORGE_NET_COMPARE_PASSED : AE_FORGE_NET_COMPARE_FAILED;
	if (state == AE_FORGE_NET_COMPARE_PASSED)
		return state;
	if (asked_ms && (unsigned int)now_ms - (unsigned int)asked_ms >= (unsigned int)AE_FORGE_NET_COMPARE_TIMEOUT_MS)
		return AE_FORGE_NET_COMPARE_NOT_COMPARED;
	return state;
}

int ae_forge_net_rules_apply_allowed(int state)
{
	return state == AE_FORGE_NET_COMPARE_PASSED || state == AE_FORGE_NET_COMPARE_NOT_COMPARED;
}

void ae_forge_net_rules_ring_reset(struct ae_forge_net_ring *ring)
{
	ring->head = 0;
	ring->count = 0;
}

int ae_forge_net_rules_ring_push(struct ae_forge_net_ring *ring, char const *text)
{
	int at;

	if (ring->count >= AE_FORGE_NET_RING_LINES)
		return 0;
	at = (ring->head + ring->count) % AE_FORGE_NET_RING_LINES;
	snprintf(ring->lines[at], sizeof(ring->lines[at]), "%s", text);
	ring->count++;
	return 1;
}

int ae_forge_net_rules_ring_pop(struct ae_forge_net_ring *ring, char *text)
{
	if (ring->count <= 0)
		return 0;
	memcpy(text, ring->lines[ring->head], AE_FORGE_NET_TEXT_SIZE + 1);
	ring->head = (ring->head + 1) % AE_FORGE_NET_RING_LINES;
	ring->count--;
	return 1;
}

/* ---------- the base tags */

int ae_forge_net_rules_tags_compare(int have_host, unsigned int host_count, unsigned int host_fingerprint, unsigned int count,
	unsigned int fingerprint)
{
	if (!have_host)
		return AE_FORGE_NET_TAGS_NOT_COMPARED;
	if (host_count != count)
		return AE_FORGE_NET_TAGS_COUNT;
	return host_fingerprint == fingerprint ? AE_FORGE_NET_TAGS_SAME : AE_FORGE_NET_TAGS_FINGERPRINT;
}

void ae_forge_net_rules_tags_message(int result, unsigned int host_count, unsigned int count, char *text, int size)
{
	if (result == AE_FORGE_NET_TAGS_COUNT)
		snprintf(text, (size_t)size, "forge: your tags differ from the host's (%u against %u): this game cannot join it", count, host_count);
	else if (result == AE_FORGE_NET_TAGS_FINGERPRINT)
		snprintf(text, (size_t)size, "forge: your tags are not the host's (the same number, %u, of other tags): this game cannot join it", count);
	else if (result == AE_FORGE_NET_TAGS_NOT_COMPARED)
		snprintf(text, (size_t)size, "forge: base tags not compared (the host did not say its own)");
	else
		snprintf(text, (size_t)size, "forge: base tags the host's (%u)", count);
}

/* ---------- object changes */

int ae_forge_net_rules_changes_count(int count)
{
	return count <= 0 ? 0 : count > AE_FORGE_NET_CHANGES_MAX ? AE_FORGE_NET_CHANGES_MAX : count;
}
