# Arena Evolved, OpenCE and ChupathingyCE

Back to the [README](../README.md).

- [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) is where the port is
  made. Arena Evolved merges its releases.
- [ChupathingyCE](https://github.com/ChupathingyCE/chupathingyce), a community
  build of OpenCE, is the base Arena Evolved grew from: its online games,
  Custom Edition and HaloMD map support and fixes come with it, and their
  documentation in this repository is theirs.
- Fixes to the shared game code are offered back upstream.
- Online, Arena Evolved is network version 29 (from 0.3.0-beta): it plays
  with OpenCE builds on network 29 (build-206 onward, until OpenCE changes
  its network version) and ChupathingyCE 0.7.5b, both ways, and joins only
  games hosted on version 29 (an older host is refused: the host needs to
  update). Builds that join only hosts of 28 or older (Arena Evolved
  0.2.3-beta to 0.2.5-beta, OpenCE build-205 and earlier, DamnationCE,
  ChupathingyCE 0.7.4 and earlier) don't join its games: their players are
  told the host is newer. 0.2.3-beta to 0.2.5-beta (28) play with OpenCE
  build-201 to build-205. A game on a Custom Edition map is named by the
  map's own name, as OpenCE names it, its version the map's hash, so OpenCE
  and ChupathingyCE players with the map join it too; players without it
  download it from the game's players when the host shares maps (OpenCE's
  map sharing, Server Setup > MAP SHARING; port/linux/README.md, "Map
  sharing"), or, if they say yes, from the community map repository
  (ChupathingyCE's map downloads, Settings > Network > MAP DOWNLOADS).

