# Network extensions

What a machine can do beyond the shared protocol is told as a list of capabilities named by strings. Builds need not
agree on the list: a capability a machine does not know is ignored. A game that needs a capability keeps out the
machines that do not have it, with a notice saying why. A build that does not speak extensions plays every game that
needs none, as before.

Nothing a stock machine sends or expects changes: the join request, the shared messages and the network version stay as
they are.

Arena Evolved tells one capability, `ae.arena_evolved` (version 1), and requires none. The machine-name marks and
rejection codes of the join (`ae_silent_rules.h`) stay the gates of a game; a client that tells the capability also
carries the AE mark in its name, and the host logs once per machine when the two disagree.

## What a stock machine does with the packet

A game packet of a type it does not know is logged and dropped, the connection kept: the client's packet switch
(`network_client_message_handler.c`, "unknown packet type received") and the host's (`network_server_message_handler.c`,
"bad or inappropriate packet type received from a client"). Stock types are 0 to 34. A stock client never answers an
announcement, so a game that requires a capability waits for an answer that does not come and removes the machine
after 5 seconds, with the capability's own text as a notice.

## The message

One game packet type, **255** (`AE_NETEXT_PACKET_TYPE`), in both directions over a machine's stream only. Its bytes, before
the type byte that ends every packet:

| Bytes | Field |
|---|---|
| 4 | magic `SLXT` |
| 1 | format, 1 |
| 1 | kind: 0 the host's announcement, 1 a client's answer |
| 1 | count of entries (at most 32) |
| ... | the entries |

Each entry:

| Bytes | Field |
|---|---|
| 1 | key length, 1 to 63 |
| n | key: ASCII letters, digits, `_` and `.` |
| 2 | version (little-endian) |
| 1 | flags: bit 0 *required* (the host's announcement: the game needs it) |
| 1 | text length, 0 to 127 |
| n | text: printable ASCII, what a machine without the capability is told (empty in an answer) |

A message is dropped whole, and logged once per machine, when a length passes its end, a key is bad or appears twice,
the count passes 32, or the message passes 1024 bytes. A key the reader does not have is skipped, but its text is kept
when the game requires it.

## The handshake

1. A client joins as before.
2. The host sends each client machine that has joined its announcement: every capability it has, each marked *required*
   if the current game needs it; again when what the game requires changes.
3. A client that speaks extensions answers with every capability it has. If the announcement requires a capability it
   does not have, it leaves the game with the host's text in an error box.
4. The host removes a machine that, when the game requires a capability, answered without it or did not answer in 5
   seconds: it is sent the capability's text as the removal notice, and removed 3 seconds later.
5. A game that requires nothing removes no one.

## The code

- `port/linux/game/ae_net_extensions_rules.c`: the bytes, built and read, with every length and count bounded;
  `port/linux/tests/ae_net_extensions_rules_test.c` feeds it truncated, oversized, duplicated and garbage packets.
- `port/linux/game/ae_net_extensions.c`: the table of this build's capabilities, each machine's record, the sends.
- DEV MODE: `netext status` prints this machine's names and, per connected machine, the names it told.
