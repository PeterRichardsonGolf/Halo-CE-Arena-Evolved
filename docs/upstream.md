# Arena Evolved, OpenCE and ChupathingyCE

Back to the [README](../README.md).

- [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) is where the port is
  made. Arena Evolved merges its releases: it is on OpenCE build-244 and
  ChupathingyCE 0.8.0b. Arena Evolved is AGPL-3.0-or-later from its merge of
  OpenCE build-237, as OpenCE is; ChupathingyCE's code from its 0.8.0 is
  GPL-3.0-or-later, which the GPL lets it combine with an AGPL work (README,
  "Licence").
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), a community
  build of OpenCE, is the base Arena Evolved grew from: its online games,
  Custom Edition and HaloMD map support and fixes come with it, and their
  documentation in this repository is theirs.
- Fixes to the shared game code are offered back upstream.
- Online, Arena Evolved is network version 32 (from 0.3.5): it plays with
  OpenCE builds on network 32 (build-244 onward, until OpenCE changes its
  network version), both ways, and joins only games hosted on version 32
  (an older host is refused: the host needs to update). Arena Evolved
  0.3.0-beta to 0.3.4-beta, OpenCE build-206 to build-237 and ChupathingyCE
  0.7.5 (29), and OpenCE build-242 (31), don't play with 0.3.5: their
  players are told which side needs to update. From 0.3.6 Arena Evolved is
  on ChupathingyCE 0.8's Delta wire, `chupa-32a`, with a range of that wire
  alone (its 30 to 32 are OpenCE's: `port/linux/src/ae_delta_wire.h`), so it
  plays with ChupathingyCE 0.8 both ways, and with Arena Evolved 0.3.5 (its
  own wire, `ae-32a`); ChupathingyCE 0.7.5's clients (`chupa-29a`) are
  refused, as its hosts are. 0.3.0-beta to 0.3.4-beta
  (29) play with OpenCE build-206 to build-237; 0.2.3-beta to 0.2.5-beta
  (28) with OpenCE build-201 to build-205. A game on a Custom Edition map is named by the
  map's own name, as OpenCE names it, its version the map's hash, so OpenCE
  and ChupathingyCE players with the map join it too; players without it
  download it from the game's players when the host shares maps (OpenCE's
  map sharing, Server Setup > MAP SHARING; port/linux/README.md, "Map
  sharing"), or, if they say yes, from the community map repository
  (ChupathingyCE's map downloads, Settings > Network > MAP DOWNLOADS).

