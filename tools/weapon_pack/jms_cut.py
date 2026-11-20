"""Cuts faces out of a JMS model file (version 8200) by position: a triangle is dropped when its three vertices are
bound to the given node and lie inside a box (in the file's units). Vertices, nodes, materials and markers are kept.
The sidearm recipe uses it to take the trigger guard off ZTeam's h2 magnum (make_pack.py, the "model_cut" step).

    jms_cut.py <in.jms> <out.jms> <node> <xmin> <xmax> <ymin> <ymax> <zmin> <zmax>
"""
import sys


class Refused(Exception):
    """not a JMS this reads, with the reason"""


def cut(text, node, box):
    """(the JMS text with the triangles inside the box dropped, how many were dropped, how many there were).
    box: xmin, xmax, ymin, ymax, zmin, zmax"""
    lines = text.split("\n")
    position = 0
    out = []

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
        return count, [[take() for _ in range(per_item)] for _ in range(count)]

    out.append(take())                  # version
    out.append(take())                  # node list checksum
    for per_item in (5, 2, 6, 1):       # nodes, materials, markers, regions
        _, items = counted(per_item)
        for item in items:
            out.extend(item)
    count, vertices = counted(6)        # each: node, position, normal, node1, node1 weight, texture coordinates
    for block in vertices:
        out.extend(block)
    nodes = [int(block[0]) for block in vertices]
    points = [[float(value) for value in block[1].split()] for block in vertices]
    count = int(take())
    triangles = [[take() for _ in range(3)] for _ in range(count)]      # each: region, shader, three vertex indices
    rest = lines[position:]

    def inside(index):
        p = points[index]
        return (nodes[index] == node and box[0] <= p[0] <= box[1] and box[2] <= p[1] <= box[3]
                and box[4] <= p[2] <= box[5])

    kept = [t for t in triangles if not all(inside(int(index)) for index in t[2].split())]
    out.append(str(len(kept)))
    for triangle in kept:
        out.extend(triangle)
    return "\n".join(out + rest), len(triangles) - len(kept), len(triangles)


def main(arguments):
    if len(arguments) != 9:
        print(__doc__)
        return 2
    with open(arguments[0]) as file:
        text = file.read()
    result, dropped, total = cut(text, int(arguments[2]), [float(value) for value in arguments[3:9]])
    with open(arguments[1], "w") as file:
        file.write(result)
    print("%s: %d of %d triangles dropped" % (arguments[1], dropped, total))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
