/*
AE_BIPED_PACK_RULES.H

Biped packs: a pack of tags only that carries one player biped (its model, textures, animations, collision and sounds),
which a game's host may choose as PLAYER MODEL for everyone. Pure rules (no engine headers:
port/linux/tests/ae_biped_pack_rules_test.c runs them); the engine side is ae_weapon_pack.c's.

SLOTS. A machine reads up to AE_BIPED_PACK_SLOTS packs, packs\ae_biped_<n>.aepack, n = 1 to 4, each added to a
multiplayer map after every other pack. The host's choice is a slot (0: STANDARD, the map's own biped; the byte
travels in the gametype's AEXV block, ae_variant_ext_rules.h).

WHICH PACKS A MAP TAKES. Tag indices must agree between the host and every joined machine, so every machine adds every
biped pack it has, in slot order: the host and an offline game when the gametype names a model, a joined machine
whenever it has one (it learns the host's choice only after its map is loaded). A host whose game names a model refuses
a machine whose slots differ from its own.

FITNESS. A biped is offered only when it can do what the stock player does in the game: it holds every weapon label
the map's weapons use, in every seat that holds weapons (else the weapon is not drawn in its hands); it has every
vehicle seat the stock player has (else it cannot ride); it is built for the player's physics; and its size is within
bounds of the stock player's (vehicles, doors, bot aim and nav clearances are made for that size). The stock player's
shield and body values are written over the biped's, so a model changes the look only.
*/

#ifndef AE_BIPED_PACK_RULES_H
#define AE_BIPED_PACK_RULES_H

enum
{
	AE_BIPED_PACK_SLOTS = 4,
	AE_BIPED_MODEL_STANDARD = 0,
	AE_BIPED_PACK_NAME_SIZE = 32,
	AE_BIPED_PACK_REASON_SIZE = 384,
	/* a pack's own tags and own bytes (ae_pack_format.h's caps are wider: a biped carries its sounds too) */
	AE_BIPED_PACK_OWN_TAGS_MAXIMUM = 400,
	AE_BIPED_PACK_OWN_BYTES_MAXIMUM = 8 * 1024 * 1024
};

/* the join: the machine name's MCC mark character (AE_SILENT_JOIN_MCC_MARK | bit 0) carries the slots a machine has in
bits 4-7 (bit 3 + slot); a host of before reads bit 0 only */
#define AE_BIPED_JOIN_SHIFT 4
#define AE_BIPED_JOIN_MASK 0xF0u
/* the refusal's code, past the others (ae_silent_rules.h) */
#define AE_BIPED_REJECTION_CODE 77

/* the slot's pack id ("ae_biped_1"), or NULL for a slot out of range */
char const *ae_biped_pack_id(int slot);
/* the slot a pack id names (1 to AE_BIPED_PACK_SLOTS), or 0 */
int ae_biped_pack_slot_of_id(char const *id);
unsigned int ae_biped_pack_slot_bit(int slot);
/* the model a game names: 0 to AE_BIPED_PACK_SLOTS read as it is, anything else as STANDARD */
int ae_biped_model_sanitize(int model);

/* the slots a map takes (a mask): see WHICH PACKS A MAP TAKES; is_client: a machine joined to a host */
unsigned int ae_biped_pack_attach_mask(int is_client, int model, unsigned int loaded_mask);
/* whether a gametype may name a model: not an AE COMP gametype, a drill, TRAINING or a tournament match */
int ae_biped_pack_offered(int comp, int training, int drill, int tournament);
/* the join: a game naming model needs a machine whose slots are host_mask exactly; told_mask is what the machine said;
told_ae 0: the machine is not Arena Evolved, it says nothing */
int ae_biped_pack_join_refused(int model, unsigned int host_mask, unsigned int told_mask, int told_ae);
/* the bits a join character carries (bits 4-7) for a mask of slots, and a mask read back from a character */
unsigned int ae_biped_pack_join_bits(unsigned int loaded_mask);
unsigned int ae_biped_pack_join_told(unsigned int join_character);

/* what the fitness check knows of a biped (and of the stock player to compare with) */
struct ae_biped_facts
{
	float radius;               /* collision radius, world units */
	float standing_height;      /* collision height standing */
	float standing_eye;         /* standing camera height */
	float crouching_eye;        /* crouching camera height */
	float body_vitality;        /* the collision model's maximum body vitality */
	float shield_vitality;      /* its maximum shield vitality */
	int has_animation_graph;
	int uses_player_physics;    /* the biped's flag */
	int labels_checked;         /* the labels the map's weapons use, each looked for in every seat that holds weapons */
	int labels_missing;         /* how many of them some such seat lacks */
	char label_first[AE_BIPED_PACK_NAME_SIZE];   /* the first missing label */
	int seats_stock;            /* the stock player's graph's vehicle seats */
	int seats_missing;          /* how many of them the biped's graph lacks */
	char seat_first[AE_BIPED_PACK_NAME_SIZE];    /* the first missing seat */
};

/* the bounds of the biped's size against the stock player's: each measure's ratio from MINIMUM to MAXIMUM */
#define AE_BIPED_SIZE_RATIO_MINIMUM 0.80f
#define AE_BIPED_SIZE_RATIO_MAXIMUM 1.25f

/* TRUE when the biped is fit; else FALSE and every reason it is not (told together, each after a semicolon) written to reason */
int ae_biped_pack_fitness(struct ae_biped_facts const *stock, struct ae_biped_facts const *biped, char *reason, int size);
/* the vitalities the pack's biped has beside the stock ones */
void ae_biped_pack_vitality_text(struct ae_biped_facts const *stock, struct ae_biped_facts const *biped, char *text, int size);
/* the pack's limits: a reason (written to buffer), or NULL when it is within them */
char const *ae_biped_pack_limits_reason(int own_tags, long own_bytes, char *buffer, int size);

#endif
