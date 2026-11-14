"""The heavy revolver's first-person animations (ZTeam's h3 ones, its spartan arms) with the whole view model set back:
the root node ("frame bone24", the arms' and gun's common parent, which the game places at the camera) moved by SHIFT
in every base animation, in its static value or in each frame, so the arms and the gun move together and hold as they
did. ZTeam's root sits 0.033 forward and 0.073 down; at AE's default field of view that puts the gun big and near the
middle of the screen, where the other packs' pistols sit lower right and further off; set back as below, it sits where
the HJ magnum does (box captures side by side, the stock pistol and the HJ magnum at the same field of view). Overlay animations (aim,
moving) carry offsets from their own frames and are left as they are.

Our own work, from ZTeam's tag; reads the tag, writes the tag (frame data is big-endian in a tag file: rotations four
int16, translations three float32, scales one float32, a node's in that order and only for what its flags animate;
the default data the same for what they do not).
Needs Reclaimer (its antr definition): the art stage runs it (make_pack.py --derive-python).
Usage: revolver_fp.py <ZTeam's mauler_fp_spartan.model_animations> <out.model_animations>"""
import os
import struct
import sys

from reclaimer.hek.defs.antr import antr_def

ROOT = "frame bone24"
# the set back in the root's own space (world units): x forward (away from the eye), y left, z up
SHIFT = (0.045, -0.025, -0.035)


def flags(low, high, count):
    return [((int(low) | (int(high) << 32)) >> node) & 1 for node in range(count)]


def offset_of(node, rotations, translations, scales, animated):
    """where node's translation sits in one frame (animated: the flagged values) or in the default data (not)"""
    position = 0
    for index in range(node):
        position += 8 * (rotations[index] == animated) + 12 * (translations[index] == animated) + \
            4 * (scales[index] == animated)
    return position + 8 * (rotations[node] == animated)


def shift(data, position):
    x, y, z = struct.unpack_from(">3f", data, position)
    struct.pack_into(">3f", data, position, x + SHIFT[0], y + SHIFT[1], z + SHIFT[2])


def main(source, out):
    tag = antr_def.build(filepath=source)
    d = tag.data.tagdata
    names = [node.name for node in d.nodes.STEPTREE]
    if ROOT not in names:
        sys.exit("no node %s" % ROOT)
    root = names.index(ROOT)
    moved = 0
    for animation in d.animations.STEPTREE:
        if animation.type.enum_name != "base":
            continue
        count = animation.node_count
        rotations = flags(animation.rot_flags0, animation.rot_flags1, count)
        translations = flags(animation.trans_flags0, animation.trans_flags1, count)
        scales = flags(animation.scale_flags0, animation.scale_flags1, count)
        frame_size = 8 * sum(rotations) + 12 * sum(translations) + 4 * sum(scales)
        if frame_size != animation.frame_size:
            sys.exit("%s: frame size %d, not %d" % (animation.name, animation.frame_size, frame_size))
        if translations[root]:
            data = bytearray(animation.frame_data.data)
            if len(data) != frame_size * animation.frame_count:
                sys.exit("%s: frame data of %d bytes" % (animation.name, len(data)))
            at = offset_of(root, rotations, translations, scales, 1)
            for frame in range(animation.frame_count):
                shift(data, frame * frame_size + at)
            animation.frame_data.data = data
        else:
            data = bytearray(animation.default_data.data)
            if len(data) != 24 * count - frame_size:
                sys.exit("%s: default data of %d bytes" % (animation.name, len(data)))
            shift(data, offset_of(root, rotations, translations, scales, 0))
            animation.default_data.data = data
        moved += 1
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tag.filepath = out
    tag.serialize(temp=False, backup=False, calc_pointers=True, int_test=False)
    print("revolver_fp: %d base animations set back by %s" % (moved, SHIFT))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
