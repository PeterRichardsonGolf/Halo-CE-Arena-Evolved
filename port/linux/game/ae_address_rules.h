/*
AE_ADDRESS_RULES.H

JOIN BY ADDRESS (ae_screen_join_address.c): what a player types, an IP address, a computer's name or a private
network's name, with a port after a colon when the host uses another one, and the RECENT list of the hosts joined this
way. Pure, no engine includes (unit test: port/linux/tests/ae_address_rules_test.c).
*/

#ifndef __AE_ADDRESS_RULES_H
#define __AE_ADDRESS_RULES_H

#include <stddef.h>
#include <time.h>

enum
{
	AE_ADDRESS_LIMIT = 64,        /* the typed text's characters */
	AE_ADDRESS_HOST_LIMIT = 64,
	AE_ADDRESS_RECENT_LIMIT = 10,
	AE_ADDRESS_NAME_LIMIT = 24
};

/* why a text is no address */
enum
{
	AE_ADDRESS_OK,
	AE_ADDRESS_EMPTY,
	AE_ADDRESS_TOO_LONG,
	AE_ADDRESS_BAD_CHARACTER,
	AE_ADDRESS_NO_HOST,
	AE_ADDRESS_BAD_PORT
};

struct ae_address
{
	char host[AE_ADDRESS_HOST_LIMIT + 1];   /* the host as typed, trimmed */
	unsigned short port;                    /* the port typed, else the default given */
	int port_typed;
	char text[AE_ADDRESS_LIMIT + 1];        /* "host" or "host:port" as it is shown and kept */
};

/* text (spaces around it ignored) as host or host:port: letters, digits, '.', '-' and '_' in the host, a port of
1 to 65535 after one colon. The default port when none is typed. Returns AE_ADDRESS_OK, else why not (address
untouched) */
int ae_address_parse(const char *text, unsigned short default_port, struct ae_address *address);
/* the sentence under the field for a reason (AE_ADDRESS_OK: ""), in plain words */
const char *ae_address_reason(int reason);

struct ae_address_recent_entry
{
	char name[AE_ADDRESS_NAME_LIMIT + 1];   /* the host's name (its player's), UTF-8 */
	char text[AE_ADDRESS_LIMIT + 1];
	long when;                              /* seconds since 1970 */
};

struct ae_address_recent
{
	short count;
	struct ae_address_recent_entry entries[AE_ADDRESS_RECENT_LIMIT];
};

void ae_address_recent_init(struct ae_address_recent *recent);
/* an address joined: first in the list (an older entry of the same address, any case, replaced; the newest 10 kept);
the name empty: the address shows. 1 when kept */
int ae_address_recent_add(struct ae_address_recent *recent, const char *name, const char *text, long when);
/* forgets the entry at index; 0 when none */
int ae_address_recent_forget(struct ae_address_recent *recent, int index);
/* the list as text lines "when<TAB>address<TAB>name" (newest first) into buffer; its length, -1 when it does not fit */
long ae_address_recent_write(struct ae_address_recent const *recent, char *buffer, long size);
/* the list from such text; lines that are no entry are dropped; 1 when the text was read whole, 0 when a line was
dropped (what was good is kept) */
int ae_address_recent_read(struct ae_address_recent *recent, const char *text);

/* when as the list shows it for the local times then and now: "TODAY 20:41", "YESTERDAY", "3 DAYS AGO" (to 6), else
"OCTOBER 12" (the month's name in capitals) */
void ae_address_when_label(struct tm const *then, struct tm const *now, char *label, size_t size);

#endif
