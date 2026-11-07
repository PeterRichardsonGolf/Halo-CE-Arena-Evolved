/* Which machine a datagram is from when machines share an address (port/linux/game/ae_net_flow_rules.c). */

#include <stdio.h>
#include <string.h>

#include "../game/ae_net_flow_rules.h"

static int failures = 0;

static void expect(long got, long want, char const *what)
{
	if (got != want)
	{
		printf("FAIL %s: got %ld, want %ld\n", what, got, want);
		failures++;
	}
}

#define IP 0xC0A80B2CUL

int main(void)
{
	struct ae_flow_slot slots[4];
	unsigned long own[2] = { 0xC0A80B2CUL, 0x0A000001UL };

	memset(slots, 0, sizeof(slots));
	/* one machine of an address: any port is its, and the last port is kept */
	slots[0].address = IP; slots[0].joined = 1; slots[0].token = 11;
	expect(ae_flow_pick(slots, 4, IP, 40000, 0), 0, "lone machine takes any port");
	expect(slots[0].port, 40000, "lone machine port kept");
	expect(ae_flow_pick(slots, 4, IP, 40001, 0), 0, "lone machine, a new port");
	expect(slots[0].port, 40001, "lone machine port follows");
	expect(ae_flow_pick(slots, 4, IP + 1, 40001, 0), AE_FLOW_NONE, "other address: none");
	/* a second machine of the address, its port unknown: it takes the first new port */
	slots[1].address = IP; slots[1].joined = 1; slots[1].token = 22;
	expect(ae_flow_pick(slots, 4, IP, 40001, 0), 0, "first machine keeps its port");
	expect(ae_flow_pick(slots, 4, IP, 40002, 0), 1, "second machine takes the new port");
	expect(ae_flow_pick(slots, 4, IP, 40002, 0), 1, "and keeps it");
	expect(ae_flow_pick(slots, 4, IP, 40001, 0), 0, "while the first is the first's");
	/* a third and fourth both unknown: a new port is not guessed */
	slots[2].address = IP; slots[2].joined = 1; slots[2].token = 33;
	slots[3].address = IP; slots[3].joined = 1; slots[3].token = 44;
	expect(ae_flow_pick(slots, 4, IP, 40003, 0), AE_FLOW_NONE, "two unknown: not guessed");
	/* two not known, one of them loaded: it takes the datagram */
	slots[2].loaded = 1;
	expect(ae_flow_pick(slots, 4, IP, 40003, 0), 2, "the one that has loaded takes it");
	slots[2].port = 0;
	slots[2].loaded = 0;
	/* the token says which */
	expect(ae_flow_bind(slots, 4, IP, 40004, 44), 3, "bind by token");
	expect(ae_flow_bind(slots, 4, IP, 40003, 33), 2, "bind by token, the other");
	expect(ae_flow_pick(slots, 4, IP, 40004, 0), 3, "the bound port is its");
	expect(ae_flow_pick(slots, 4, IP, 40003, 0), 2, "the other's");
	expect(ae_flow_bind(slots, 4, IP, 40005, 99), AE_FLOW_NONE, "a token no machine has");
	expect(ae_flow_bind(slots, 4, IP + 1, 40005, 44), AE_FLOW_NONE, "a token from another address");
	expect(ae_flow_bind(slots, 4, IP, 40005, 0), AE_FLOW_NONE, "no token");
	/* a port that moves to another machine leaves the first */
	expect(ae_flow_bind(slots, 4, IP, 40002, 44), 3, "machine 3 takes machine 1's port");
	expect(slots[1].port, 0, "which machine 1 loses");
	/* a port a bind said never moves but by a bind: alone at its address, a datagram from another port is nobody's */
	memset(slots, 0, sizeof(slots));
	slots[0].address = IP; slots[0].joined = 1; slots[0].token = 11;
	expect(ae_flow_bind(slots, 4, IP, 40010, 11), 0, "bound by token");
	expect(ae_flow_pick(slots, 4, IP, 40011, 0), AE_FLOW_NONE, "a lone bound machine: another port is nobody's");
	expect(slots[0].port, 40010, "and its port stays");
	expect(ae_flow_pick(slots, 4, IP, 40010, 0), 0, "its own port is its");
	expect(ae_flow_bind(slots, 4, IP, 40011, 11), 0, "a bind moves it");
	expect(ae_flow_pick(slots, 4, IP, 40011, 0), 0, "to the new port");
	/* a second machine, never bound, at the address: elimination gives it a new port, the bound one keeps its own */
	slots[1].address = IP; slots[1].joined = 1; slots[1].token = 22;
	expect(ae_flow_pick(slots, 4, IP, 40012, 0), 1, "the unbound machine takes the new port");
	expect(ae_flow_pick(slots, 4, IP, 40013, 0), AE_FLOW_NONE, "a third port: nobody's");
	expect(slots[0].port, 40011, "the bound port untouched");
	/* a bind that takes a port from another machine unbinds that one */
	expect(ae_flow_bind(slots, 4, IP, 40012, 11), 0, "machine 0 binds to machine 1's port");
	expect(slots[1].port, 0, "machine 1 loses it");
	expect(slots[1].bound, 0, "and is not bound");
	/* the host's own machine at the address: left out for datagrams of a remote one */
	memset(slots, 0, sizeof(slots));
	slots[0].address = IP; slots[0].joined = 1; slots[0].local = 1;
	slots[1].address = IP; slots[1].joined = 1;
	expect(ae_flow_pick(slots, 4, IP, 5151, 1), 1, "remote first");
	expect(ae_flow_pick(slots, 4, IP, 5151, 0), 1, "(its port now known)");
	/* machines not in the game are not found */
	slots[1].joined = 0;
	expect(ae_flow_pick(slots, 4, IP, 5151, 1), 0, "only the local one left");
	/* a rejoin */
	expect(ae_flow_same_machine("abc", "abc"), 1, "same hardware id: same machine");
	expect(ae_flow_same_machine("abc", "abd"), 0, "other hardware id: another machine");
	expect(ae_flow_same_machine("", "abd"), 0, "none told: not the other machine");
	expect(ae_flow_same_machine("abc", NULL), 0, "none known: not the other machine");
	expect(ae_flow_same_machine("", ""), 0, "neither told: not the same machine");
	expect(ae_flow_same_machine(NULL, NULL), 0, "neither known: not the same machine");
	/* the host's rule for a machine that joins from an address another is at: the same hardware id, and not a tunnel
	agent's address (every player of a tunnel shares it) */
	expect(!ae_flow_address_is_tunnel("auto", 1, IP, own, 2) && ae_flow_same_machine("abc", "abc"), 0,
		"a tunnel's address: never the old machine");
	expect(!ae_flow_address_is_tunnel("auto", 1, IP, own, 2) && ae_flow_same_machine("", "abc"), 0,
		"a tunnel's address and no id: never");
	expect(!ae_flow_address_is_tunnel("", 1, IP, own, 2) && ae_flow_same_machine("abc", "abc"), 1,
		"a home router's address, the same id: the old machine come back");
	expect(!ae_flow_address_is_tunnel("", 1, IP, own, 2) && ae_flow_same_machine("", "abc"), 0,
		"a home router's address, no id: another machine");
	/* tunnel addresses */
	expect(ae_flow_address_is_tunnel("auto", 1, IP, own, 2), 1, "auto: an own address");
	expect(ae_flow_address_is_tunnel("auto", 1, 0x7F000001UL, own, 2), 1, "auto: loopback");
	expect(ae_flow_address_is_tunnel("auto", 1, 0x7F0000E6UL, own, 2), 0, "auto: another loopback address is not");
	expect(ae_flow_address_is_tunnel("auto", 1, 0x08080808UL, own, 2), 0, "auto: another address");
	expect(ae_flow_address_is_tunnel("auto", 0, IP, own, 2), 0, "auto: not on a listen host");
	expect(ae_flow_address_is_tunnel("", 1, IP, own, 2), 0, "empty: none");
	expect(ae_flow_address_is_tunnel("10.1.2.3, 192.168.11.44", 0, IP, own, 2), 1, "listed");
	expect(ae_flow_address_is_tunnel("10.1.2.3 junk auto", 1, 0x0A000001UL, own, 2), 1, "list with auto and junk");
	expect(ae_flow_address_is_tunnel("10.1.2", 0, 0x0A010203UL, own, 2), 0, "half an address is none");
	expect(ae_flow_address_is_tunnel("1.2.3.400", 0, 0x01020390UL, own, 2), 0, "out of range is none");
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
