"""Re-rigs a JMS model file (version 8200) onto another model's node frames: every vertex and marker position is moved
by p' = scale * (p - origin) + target (a uniform scale about a point, then to another), and the named nodes are given the
given positions (relative to their parents, as the file holds them), so the model's shape is where it was in the file's
space while its nodes sit where the other model's do. The energy sword recipe uses it to put CMT's sword on the node
frames ("frame handle", "frame blades") of the first-person animations it plays (make_pack.py, the "model_rig" step).

    jms_rig.py <in.jms> <out.jms> <scale> <origin x y z> <target x y z> [<node> <x> <y> <z>]...
"""
import sys


class Refused(Exception):
    """not a JMS this reads, with the reason"""


def number(value):
    text = "%.10f" % value
    return text.rstrip("0").rstrip(".") if "." in text else text


def move(line, scale, origin, target):
    values = [float(item) for item in line.split()]
    if len(values) != 3:
        raise Refused("a position is not three numbers: %r" % line)
    return "\t".join(number(scale * (value - start) + end) for value, start, end in zip(values, origin, target))


def rig(text, scale, origin, target, nodes):
    """(the JMS text moved, how many vertices and markers were moved). nodes: {node name: (x, y, z)}"""
    lines = text.split("\n")
    position = 0
    out = []
    moved = 0
    unused = set(nodes)

    def take():
        nonlocal position
        while position < len(lines) and lines[position].strip() == "":
            out.append(lines[position])
            position += 1
        if position >= len(lines):
            raise Refused("the file ends early")
        position += 1
        return lines[position - 1]

    def counted(per_item):
        count = int(take())
        out.append(str(count))
        return [[take() for _ in range(per_item)] for _ in range(count)]

    out.append(take())                  # version
    out.append(take())                  # node list checksum
    for block in counted(5):            # name, first child, next sibling, rotation, position
        if block[0] in nodes:
            block[4] = "\t".join(number(value) for value in nodes[block[0]])
            unused.discard(block[0])
        out.extend(block)
    for block in counted(2):            # materials
        out.extend(block)
    for block in counted(6):            # markers: name, region, node, rotation, position, radius
        block[4] = move(block[4], scale, origin, target)
        moved += 1
        out.extend(block)
    for block in counted(1):            # regions
        out.extend(block)
    for block in counted(6):            # vertices: node, position, normal, node1, node1 weight, texture coordinates
        block[1] = move(block[1], scale, origin, target)
        moved += 1
        out.extend(block)
    if unused:
        raise Refused("no such node: %s" % ", ".join(sorted(unused)))
    return "\n".join(out + lines[position:]), moved


def main(arguments):
    if len(arguments) < 9 or (len(arguments) - 9) % 4:
        raise SystemExit(__doc__)
    nodes = {arguments[index]: tuple(float(value) for value in arguments[index + 1:index + 4])
             for index in range(9, len(arguments), 4)}
    with open(arguments[0]) as source:
        text, moved = rig(source.read(), float(arguments[2]), [float(v) for v in arguments[3:6]],
                          [float(v) for v in arguments[6:9]], nodes)
    with open(arguments[1], "w") as out:
        out.write(text)
    print("%d points moved" % moved)


if __name__ == "__main__":
    main(sys.argv[1:])
