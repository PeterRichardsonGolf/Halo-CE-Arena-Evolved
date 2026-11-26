"""The auto-shotgun's first-person animations from ZTeam's (our own work, from ZTeam's shotgun_fp_spartan graph of the
ZTeam tag pack's shotgun/h3_hunter): a semi-automatic action, charged once on the draw, worked once at the end of a
reload, never on a shot.

- fire-1 (its three permutations, each from its own frames): no forend stroke. The recoil (the source's frames 0-17)
  plays 1.5 times as fast (12 frames), the loading port's cycle and the spent shell's flip (the source's frames
  24-31, which followed the stroke) are moved into the recoil's frames 1-6, the forend is held at rest, and two
  frames blend back to the rest pose: 14 frames (0.47 s), the source's 34. The fire animation's sound (the stroke's,
  0.8 s after the shot) is taken off.
- ready: ZTeam's, with its one rack and its sound: the draw charges the gun once, as a semi-automatic's.
- posing: the slot plays the source's second posing, which never moves the forend (the first is a forend check); its
  sound (a forend check's, which that posing does not show) is taken off.
- reload: the enter and the shell-by-shell loop, which never move the forend, play about a fifth faster (the enter
  19 frames to 15, 0.5 s; the loop 16 to 13, 0.43 s, resampled over its whole cycle so one loop still runs into the
  next). The engine times a shell by these animations (weapons.c: the enter's frames for the first shell, the loop's
  for each next one, the round added as the timer runs out), so a shell counts only once the hand has seated it (the
  shell node is back at rest by the loop's frame 12 of 13 and the enter's 14 of 15). The exit (both exit slots name
  it: the engine plays it once, after the last shell of any reload) keeps its single forend stroke and its sound: the
  action is worked exactly once per reload.

So the forend moves once on a draw and once per reload, never on a shot. Animations not edited keep their bytes; the
sounds no animation plays any longer leave the graph's sound list (the others renumbered). Reads the tag, writes the
tag.

Needs Reclaimer (its antr definition and animation codec): the art stage runs it (make_pack.py --derive-python).
Usage: auto_shotgun_anims.py <ZTeam's shotgun_fp_spartan.model_animations> <out model_animations>"""
import math
import sys

from reclaimer.animation.animation_compilation import compile_animation
from reclaimer.animation.animation_decompilation import extract_animation
from reclaimer.animation.jma import JmaNodeState
from reclaimer.hek.defs.antr import antr_def

SRC, OUT = sys.argv[1], sys.argv[2]
FIRE = ("first-person fire-1%0", "first-person fire-1%1", "first-person fire-1%2")
POSING_PUMP = "first-person posing%0"
POSING = "first-person posing%1"
EXIT = "first-person reload-exit-empty"
# the fp slots (the engine's order): posing, and the two reload exits
SLOT_POSING = 1
SLOT_EXIT_EMPTY = 24
SLOT_EXIT_FULL = 25
# the source's frames: the recoil, then the loading port's cycle and the shell's flip as they come after the stroke
RECOIL_LAST = 17
RECOIL_FRAMES = 12
PORT = (24, 25, 27, 30, 31)
SHELL = (26, 27, 28, 29, 30, 31)
ENTER = "first-person reload-enter"
LOOP = "first-person reload-loop"
# the reload's frames: ZTeam's 19 and 16, about a fifth faster
ENTER_FRAMES = 15
LOOP_FRAMES = 13


class Refused(Exception):
    """not the graph this edits, with the reason"""


def state(node):
    return [node.pos_x, node.pos_y, node.pos_z, node.rot_i, node.rot_j, node.rot_k, node.rot_w, node.scale]


def put(node, values):
    (node.pos_x, node.pos_y, node.pos_z, node.rot_i, node.rot_j, node.rot_k, node.rot_w, node.scale) = values


def slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b, dot = [-x for x in b], -dot
    if dot > 0.9995:
        out = [x + (y - x) * t for x, y in zip(a, b)]
    else:
        angle = math.acos(min(1.0, dot))
        sa, sb = math.sin((1 - t) * angle), math.sin(t * angle)
        out = [(x * sa + y * sb) / math.sin(angle) for x, y in zip(a, b)]
    norm = math.sqrt(sum(x * x for x in out))
    return [x / norm for x in out]


def blend(frame_a, frame_b, t):
    """two frames (lists of node states) blended: positions and scale in a straight line, rotations along the sphere"""
    out = []
    for a, b in zip(frame_a, frame_b):
        out.append([a[k] + (b[k] - a[k]) * t for k in range(3)] + slerp(a[3:7], b[3:7], t) + [a[7] + (b[7] - a[7]) * t])
    return out


def states(frame):
    return [state(node) for node in frame]


tag = antr_def.build(filepath=SRC)
d = tag.data.tagdata
names = [node.name for node in d.nodes.STEPTREE]
animations = d.animations.STEPTREE
index = {animation.name: i for i, animation in enumerate(animations)}
for name in FIRE + (ENTER, LOOP, POSING_PUMP, POSING, EXIT):
    if name not in index:
        raise Refused("no animation %r" % name)
slots = d.fp_animations.STEPTREE[0].animations.STEPTREE
if (slots[SLOT_POSING].animation, slots[SLOT_EXIT_EMPTY].animation, slots[SLOT_EXIT_FULL].animation) != (
        index[POSING_PUMP], index[EXIT], index[EXIT]):
    raise Refused("the posing and exit slots are not the source's")
PUMP, PORT_NODE, SHELL_NODE = (names.index(n) for n in ("frame pump", "frame loading port", "frame shell"))


def decoded(name):
    """(the animation's frames as node states, the decoded animation); the decoder's last frame is its loop copy
    of the first, which the encoder takes off again"""
    jma = extract_animation(index[name], d, write_jma=False)
    if jma is None or len(jma.frames) != animations[index[name]].frame_count + 1:
        raise Refused("%s does not decode" % name)
    return [states(frame) for frame in jma.frames[:-1]], jma


def encode(name, frames, jma):
    """the frames (node states) written back into the animation, with its other fields (next permutation, sound,
    key frames) kept"""
    animation = animations[index[name]]
    keep = (animation.next_animation, animation.sound, animation.sound_frame_index, animation.loop_frame_index,
            animation.key_frame_index, animation.second_key_frame_index, animation.weight)
    jma.frames = [[node_state(values) for values in frame] for frame in frames + [frames[0]]]
    jma.calculate_animation_flags()
    errors = compile_animation(animation, jma)
    if errors:
        raise Refused("%s: %s" % (name, "; ".join(errors)))
    (animation.next_animation, animation.sound, animation.sound_frame_index, animation.loop_frame_index,
     animation.key_frame_index, animation.second_key_frame_index, animation.weight) = keep
    animation.key_frame_index = min(animation.key_frame_index, len(frames) - 1)


def node_state(values):
    node = JmaNodeState()
    put(node, values)
    return node


def semi_auto_fire(name):
    src, jma = decoded(name)
    rest = src[0]
    out = []
    for k in range(RECOIL_FRAMES):
        t = k * RECOIL_LAST / (RECOIL_FRAMES - 1)
        i = int(t)
        frame = blend(src[i], src[min(i + 1, RECOIL_LAST)], t - i)
        frame[PUMP] = list(rest[PUMP])
        frame[PORT_NODE] = list(src[PORT[k - 1]][PORT_NODE]) if 1 <= k <= len(PORT) else list(rest[PORT_NODE])
        frame[SHELL_NODE] = list(src[SHELL[k - 1]][SHELL_NODE]) if 1 <= k <= len(SHELL) else list(rest[SHELL_NODE])
        out.append(frame)
    out.append(blend(out[-1], rest, 0.5))
    out.append([list(v) for v in rest])
    encode(name, out, jma)
    animations[index[name]].sound = -1


def faster(name, frames, cycle):
    """the animation resampled to the given number of frames (each blended between the source's two nearest); a cycle
    is resampled over its loop back to its first frame, so that one play runs into the next"""
    src, jma = decoded(name)
    span = len(src) if cycle else len(src) - 1
    ring = src + [src[0]]
    out = []
    for k in range(frames):
        t = k * span / (frames if cycle else frames - 1)
        i = int(t)
        out.append(blend(ring[i], ring[min(i + 1, len(ring) - 1)], t - i))
    encode(name, out, jma)


for name in FIRE:
    semi_auto_fire(name)
faster(ENTER, ENTER_FRAMES, cycle=False)
faster(LOOP, LOOP_FRAMES, cycle=True)
slots[SLOT_POSING].animation = index[POSING]
animations[index[POSING]].sound = -1
animations[index[POSING]].next_animation = -1

# the sounds the slots' animations play: the others leave the list (none of the remaining animations names them)
played = set()
for slot in slots:
    i = slot.animation
    while 0 <= i < len(animations):
        if animations[i].sound >= 0:
            played.add(animations[i].sound)
        i = animations[i].next_animation
renumber = {old: new for new, old in enumerate(sorted(played))}
for animation in animations:
    animation.sound = renumber.get(animation.sound, -1)
references = d.sound_references.STEPTREE
kept = [references[old].sound.filepath for old in sorted(played)]
del references[:]
references.extend(len(kept))
for reference, path in zip(references, kept):
    reference.sound.tag_class.set_to("sound")
    reference.sound.filepath = path
if animations[index[EXIT]].sound < 0:
    raise Refused("the exit lost its sound")
tag.serialize(filepath=OUT, temp=False, backup=False)
print("auto_shotgun_anims: fire %d frames, reload enter %d, loop %d, exit %d frames; sounds kept: %s" % (
    animations[index[FIRE[0]]].frame_count, animations[index[ENTER]].frame_count, animations[index[LOOP]].frame_count,
    animations[index[EXIT]].frame_count, ", ".join(kept)))
