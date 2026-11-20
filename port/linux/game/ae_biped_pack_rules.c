/*
AE_BIPED_PACK_RULES.C

Biped packs' pure rules (ae_biped_pack_rules.h).
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ae_biped_pack_rules.h"

static char const *const ids[AE_BIPED_PACK_SLOTS] = { "ae_biped_1", "ae_biped_2", "ae_biped_3", "ae_biped_4" };

char const *ae_biped_pack_id(int slot)
{
	return slot >= 1 && slot <= AE_BIPED_PACK_SLOTS ? ids[slot - 1] : NULL;
}

int ae_biped_pack_slot_of_id(char const *id)
{
	int slot;

	for (slot = 1; id && slot <= AE_BIPED_PACK_SLOTS; slot++)
		if (!strcmp(id, ids[slot - 1]))
			return slot;
	return 0;
}

unsigned int ae_biped_pack_slot_bit(int slot)
{
	return slot >= 1 && slot <= AE_BIPED_PACK_SLOTS ? 1u << (slot - 1) : 0;
}

int ae_biped_model_sanitize(int model)
{
	return model >= 0 && model <= AE_BIPED_PACK_SLOTS ? model : AE_BIPED_MODEL_STANDARD;
}

unsigned int ae_biped_pack_attach_mask(int is_client, int model, unsigned int loaded_mask)
{
	loaded_mask &= (1u << AE_BIPED_PACK_SLOTS) - 1;
	if (is_client)
		return loaded_mask;
	return ae_biped_model_sanitize(model) ? loaded_mask : 0;
}

int ae_biped_pack_offered(int comp, int training, int drill, int tournament)
{
	return !comp && !training && !drill && !tournament;
}

int ae_biped_pack_join_refused(int model, unsigned int host_mask, unsigned int told_mask, int told_ae)
{
	model = ae_biped_model_sanitize(model);
	if (!model)
		return 0;
	/* (a machine that is not Arena Evolved has no packs: refused as a weapon pack's game refuses it) */
	if (!told_ae)
		return 1;
	return (told_mask & ((1u << AE_BIPED_PACK_SLOTS) - 1)) != (host_mask & ((1u << AE_BIPED_PACK_SLOTS) - 1));
}

unsigned int ae_biped_pack_join_bits(unsigned int loaded_mask)
{
	return (loaded_mask & ((1u << AE_BIPED_PACK_SLOTS) - 1)) << AE_BIPED_JOIN_SHIFT;
}

unsigned int ae_biped_pack_join_told(unsigned int join_character)
{
	return (join_character & AE_BIPED_JOIN_MASK) >> AE_BIPED_JOIN_SHIFT;
}

static int ratio_outside(float stock, float biped)
{
	float ratio;

	if (!(stock > 0.0f) || !(biped > 0.0f))
		return 1;
	ratio = biped / stock;
	return ratio < AE_BIPED_SIZE_RATIO_MINIMUM || ratio > AE_BIPED_SIZE_RATIO_MAXIMUM;
}

/* a reason added to the text (the reasons of one biped are told together, each after a semicolon) */
static void add_reason(char *text, size_t size, char const *format, ...)
{
	size_t used = strlen(text);
	va_list arguments;

	if (used + 3 >= size)
		return;
	if (used)
	{
		text[used++] = ';';
		text[used++] = ' ';
		text[used] = 0;
	}
	va_start(arguments, format);
	vsnprintf(text + used, size - used, format, arguments);
	va_end(arguments);
}

static void add_size(char *text, size_t size, char const *what, float stock, float biped)
{
	if (ratio_outside(stock, biped))
		add_reason(text, size, "%s %.3f is outside %.0f%% to %.0f%% of the stock player's %.3f", what, biped,
			AE_BIPED_SIZE_RATIO_MINIMUM * 100.0f, AE_BIPED_SIZE_RATIO_MAXIMUM * 100.0f, stock);
}

int ae_biped_pack_fitness(struct ae_biped_facts const *stock, struct ae_biped_facts const *biped, char *reason, int size)
{
	char text[AE_BIPED_PACK_REASON_SIZE * 2];

	text[0] = 0;
	if (!stock || !biped)
		add_reason(text, sizeof(text), "the map's player is not known");
	else
	{
		if (!biped->has_animation_graph)
			add_reason(text, sizeof(text), "the biped has no animation graph");
		if (!biped->uses_player_physics)
			add_reason(text, sizeof(text), "the biped is not built for the player's physics (its flag is off)");
		if (biped->labels_missing > 0)
			add_reason(text, sizeof(text), "it cannot hold %d of the map's %d weapon labels in every seat that holds weapons "
				"('%.31s' first): its weapons would not be drawn in its hands", biped->labels_missing, biped->labels_checked,
				biped->label_first);
		if (biped->seats_missing > 0)
			add_reason(text, sizeof(text), "its graph lacks %d of the stock player's %d seats ('%.31s' first): it could not "
				"ride", biped->seats_missing, biped->seats_stock, biped->seat_first);
		add_size(text, sizeof(text), "collision radius", stock->radius, biped->radius);
		add_size(text, sizeof(text), "standing height", stock->standing_height, biped->standing_height);
		add_size(text, sizeof(text), "standing eye height", stock->standing_eye, biped->standing_eye);
		add_size(text, sizeof(text), "crouching eye height", stock->crouching_eye, biped->crouching_eye);
	}
	if (reason && size > 0)
		snprintf(reason, (size_t)size, "%s", text);
	return text[0] == 0;
}

void ae_biped_pack_vitality_text(struct ae_biped_facts const *stock, struct ae_biped_facts const *biped, char *text, int size)
{
	if (!text || size <= 0)
		return;
	if (!stock || !biped)
		snprintf(text, (size_t)size, "vitality not known");
	else
		snprintf(text, (size_t)size, "the biped's own shield %.0f body %.0f, the stock player's %.0f / %.0f (the stock values "
			"are written over the biped's)", biped->shield_vitality, biped->body_vitality, stock->shield_vitality,
			stock->body_vitality);
}

char const *ae_biped_pack_limits_reason(int own_tags, long own_bytes, char *buffer, int size)
{
	if (own_tags > AE_BIPED_PACK_OWN_TAGS_MAXIMUM)
		snprintf(buffer, (size_t)size, "too many own tags (%d, at most %d)", own_tags, AE_BIPED_PACK_OWN_TAGS_MAXIMUM);
	else if (own_bytes > AE_BIPED_PACK_OWN_BYTES_MAXIMUM)
		snprintf(buffer, (size_t)size, "too many own bytes (%ld, at most %d)", own_bytes, AE_BIPED_PACK_OWN_BYTES_MAXIMUM);
	else
		return NULL;
	return buffer;
}
