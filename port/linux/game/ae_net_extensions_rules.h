/*
AE_NET_EXTENSIONS_RULES.H

What a machine can do beyond the shared protocol, told as a list of capabilities named by strings, as pure rules
(no engine or config calls: port/linux/tests/ae_net_extensions_rules_test.c runs them). The engine side is
ae_net_extensions.c's.

The message is game packet type 255 (AE_NETEXT_PACKET_TYPE), in both directions over a machine's stream. Its bytes,
before the type byte that ends every game packet:

  4  magic "SLXT"
  1  format, 1
  1  kind: 0 the host's announcement, 1 a client's answer
  1  count of entries
  .. the entries: 1 key length (1 to 63), the key (ASCII letters, digits, '_' and '.'), 2 version (little-endian),
     1 flags (bit 0: the game requires it), 1 text length (0 to 127), the text (printable ASCII: what a machine
     without the capability is told; empty in an answer)

A message that does not read whole is dropped whole: a length past its end, a bad key, a key twice, more entries than
AE_NETEXT_MAXIMUM_ENTRIES, a message past AE_NETEXT_MAXIMUM_BYTES. A key the reader does not have is kept in the peer's
list of names (for the status command) and skipped for any other use.
*/

#ifndef __AE_NET_EXTENSIONS_RULES_H
#define __AE_NET_EXTENSIONS_RULES_H

#define AE_NETEXT_PACKET_TYPE 255
#define AE_NETEXT_FORMAT 1
#define AE_NETEXT_KIND_ANNOUNCEMENT 0
#define AE_NETEXT_KIND_ANSWER 1
#define AE_NETEXT_FLAG_REQUIRED 1
#define AE_NETEXT_MAGIC "SLXT"
#define AE_NETEXT_MAGIC_LENGTH 4
#define AE_NETEXT_MAXIMUM_KEY 63
#define AE_NETEXT_MAXIMUM_TEXT 127
/* the most bytes a message of ours (type byte excluded) holds, and the most entries a reader keeps */
#define AE_NETEXT_MAXIMUM_BYTES 1024
#define AE_NETEXT_MAXIMUM_ENTRIES 32

/* what a build has: a key, its version, what a machine without it is told */
struct ae_netext_capability
{
	char const *key;
	unsigned short version;
	char const *missing_notice;
};

/* what one message told */
struct ae_netext_peer
{
	int count;
	char names[AE_NETEXT_MAXIMUM_ENTRIES][AE_NETEXT_MAXIMUM_KEY + 1];
	unsigned short versions[AE_NETEXT_MAXIMUM_ENTRIES];
	unsigned char flags[AE_NETEXT_MAXIMUM_ENTRIES];
	/* an announcement's first entry the game requires that the reader has not, or has an older version of */
	int missing_required;
	char missing_key[AE_NETEXT_MAXIMUM_KEY + 1];
	char missing_text[AE_NETEXT_MAXIMUM_TEXT + 1];
};

enum
{
	AE_NETEXT_PARSE_OK = 0,
	/* not a message of this kind (another magic, format or kind): ignored without a word */
	AE_NETEXT_PARSE_OTHER,
	/* a message that does not read whole */
	AE_NETEXT_PARSE_MALFORMED
};

/* a message of `kind` of the table's capabilities, the ones of `required_mask` (a bit per table entry) flagged, into
out[capacity]: its size, or -1 if it does not fit or the table is bad. The type byte is written last. */
long ae_netext_build(unsigned char kind, struct ae_netext_capability const *table, long table_count,
	unsigned long required_mask, unsigned char *out, long capacity);

/* reads `data`[size] (the message and the type byte that ends the packet) into `peer`, against the reader's table */
int ae_netext_parse(void const *data, long size, unsigned char kind, struct ae_netext_capability const *table,
	long table_count, struct ae_netext_peer *peer);

/* the version of `key` the peer told (0 for none) */
unsigned short ae_netext_peer_version(struct ae_netext_peer const *peer, char const *key);

/* the first table entry of `required_mask` the peer did not tell at the table's version or later, or -1 */
long ae_netext_first_missing(struct ae_netext_peer const *peer, struct ae_netext_capability const *table,
	long table_count, unsigned long required_mask);

/* the key's characters only */
int ae_netext_key_valid(char const *key, long length);

#endif
