# Props

tag_props.bin (5.2 MB) is Halo Content Evolved's file (Watanoge, AGPL-3.0-or-later), taken as it ships there, byte for byte.

What it holds: Tag Injector bundles of two props, a drop capsule and a weapon rack, taken out of the community Custom Edition map
bigass_open_v1.1 together with everything those two props depend on in that map. That is 118 packed blocks and about 1,100 tags:
the props' own models, shaders and bitmaps, and the effects, sounds, particles and other tags they reference. The dependencies are
the map makers' work as their map ships it; their tag folders carry the names their makers gave them, among them folders of
community authors (altis, bourrin, cmt) and folders named after other games' looks.

Forge places the two props, and Firefight's ordnance pods and resupply racks use them. The game unpacks the file into its tag
injector folder at the first use (port/linux/src/port_tag_profiles.c, port_tag_profiles_props_open).

Credit: Halo Content Evolved (Watanoge) for the bundle, and the makers of bigass_open_v1.1 for the content. Credited in
port/assets/credits.txt.

Removal: if a maker of this content asks, the file is taken out of the project and its releases; the two props are then read
from the player's own copy of the map.
