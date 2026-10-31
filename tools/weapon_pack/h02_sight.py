"""The EVO rifle's holo sight from CuiGames' H02 Holo (A5 rail kit, used under its licence: in games, never handed out
as files, so the recipe reads it from the kit's zip at build time and nothing of it is kept in the repo): its glb read
with numpy alone, made fit for Halo CE and put on the rifle's top rail.

- Left out: the kit's text marks (CGAMES, H02), the small amber marker and the glass pane (the rifle's own glass quad,
  whose shader is ours (evo_holo), goes where the pane was).
- Made smaller: the vertices clustered on a grid (GRID of the sight's length), each triangle whose corners fall in
  three different cells kept once; normals smoothed between faces of one cell and material that bend by less than
  SMOOTH degrees.
- Each kit material is drawn with a flat patch of the rifle's own texture of about its shade (TEXELS).
- Placed: the kit's forward (+z) along the rifle's x, its up (+y) up, scaled so it stands HEIGHT tall, its rail foot on
  the rail at FOOT, its middle at MIDDLE along x.

sight(path) gives the vertices (each as evo_model.py's: position, normal, binormal, tangent, u, v, 0), the triangles and
the glass quad's corners (x, y0, y1, z0, z1). Run alone it writes them as JSON: h02_sight.py <glb> <out json>"""
import json
import struct
import sys

GRID = 1.0 / 44.0                 # the cluster cell, a part of the sight's length
SMOOTH = 50.0                     # degrees
HEIGHT = 0.0215                   # wu, as the boxes the sight replaces
FOOT = 0.0495                     # the top rail's height, wu
MIDDLE = 0.0395                   # along x, wu
LEFT_OUT = ("06 ",)               # node name starts: the text marks and the amber marker
PANE = "03 transparent visual pane"
# the kit's materials -> a patch of the rifle's texture (u, v) of about their shade
DARKEST, DARK, STEEL = (0.3887, 0.6348), (0.1697, 0.0690), (0.3799, 0.0605)
TEXELS = {"H02 FRAME": DARKEST, "H02 RUBBER": DARKEST, "H02 STEEL": STEEL, "H02 SHELL": DARK, "H02 BODY": DARK,
          "H02 ADAPTER": DARK, "H02 MARK": DARK, "H02 AMBER": DARK}


def read_glb(path):
    import numpy as np
    data = open(path, "rb").read()
    length = struct.unpack("<I", data[12:16])[0]
    gltf = json.loads(data[20:20 + length])
    binary = data[20 + length + 8:]
    sizes = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    kinds = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}

    def accessor(index):
        a = gltf["accessors"][index]
        view = gltf["bufferViews"][a["bufferView"]]
        dtype = np.dtype(kinds[a["componentType"]])
        width = sizes[a["type"]]
        start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = view.get("byteStride", width * dtype.itemsize)
        raw = np.frombuffer(binary, np.uint8, count=stride * (a["count"] - 1) + width * dtype.itemsize, offset=start)
        rows = np.lib.stride_tricks.as_strided(raw, (a["count"], width * dtype.itemsize), (stride, 1))
        return np.ascontiguousarray(rows).view(dtype).reshape(a["count"], width).astype(float)
    return gltf, accessor


def node_matrix(node):
    import numpy as np
    m = np.eye(4)
    if "matrix" in node:
        return np.array(node["matrix"], float).reshape(4, 4).T
    x, y, z, w = node.get("rotation", (0, 0, 0, 1))
    rotation = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m[:3, :3] = rotation * np.array(node.get("scale", (1, 1, 1)), float)
    m[:3, 3] = node.get("translation", (0, 0, 0))
    return m


def kit_triangles(path):
    """the kit's triangles in its own frame (the root's children carry the parts): corners (n, 3, 3), each triangle's
    material name; and the pane's corners"""
    import numpy as np
    gltf, accessor = read_glb(path)
    root = next(n for n in gltf["nodes"] if n.get("name") == "H02_ROOT")
    corners, materials, pane = [], [], None
    for index in root["children"]:
        node = gltf["nodes"][index]
        name = node.get("name", "")
        if "mesh" not in node or name.startswith(LEFT_OUT):
            continue
        m = node_matrix(node)
        for primitive in gltf["meshes"][node["mesh"]]["primitives"]:
            points = accessor(primitive["attributes"]["POSITION"])
            points = points @ m[:3, :3].T + m[:3, 3]
            indices = accessor(primitive["indices"]).astype(int).reshape(-1, 3)
            if name == PANE:
                pane = points
                continue
            corners.append(points[indices])
            materials += [gltf["materials"][primitive["material"]]["name"]] * len(indices)
    return np.concatenate(corners), materials, pane


def sight(path):
    import numpy as np
    corners, materials, pane = kit_triangles(path)
    # the kit's frame -> the rifle's: forward z -> x, its x -> y, up y -> z (a turn: no mirror)
    def turn(p):
        return np.stack([p[..., 2], p[..., 0], p[..., 1]], -1)
    corners = turn(corners)
    pane = turn(pane)
    low, high = corners.reshape(-1, 3).min(0), corners.reshape(-1, 3).max(0)
    scale = HEIGHT / (high[2] - low[2])
    shift = np.array([MIDDLE - (low[0] + high[0]) / 2 * scale, -(low[1] + high[1]) / 2 * scale, FOOT - low[2] * scale])
    corners = corners * scale + shift
    pane = pane * scale + shift
    # cluster
    cell = (high[0] - low[0]) * scale * GRID
    keys = np.floor((corners - corners.reshape(-1, 3).min(0)) / cell).astype(np.int64)
    flat = keys.reshape(-1, 3)
    cells, number = np.unique(flat, axis=0, return_inverse=True)
    number = number.reshape(-1, 3)
    sums = np.zeros((len(cells), 3))
    counts = np.zeros(len(cells))
    np.add.at(sums, number.reshape(-1), corners.reshape(-1, 3))
    np.add.at(counts, number.reshape(-1), 1)
    middle = sums / counts[:, None]
    keep = (number[:, 0] != number[:, 1]) & (number[:, 1] != number[:, 2]) & (number[:, 0] != number[:, 2])
    seen, faces = set(), []
    for t in np.nonzero(keep)[0]:
        key = (tuple(sorted(number[t])), materials[t])
        if key not in seen:
            seen.add(key)
            faces.append((tuple(number[t]), materials[t]))
    # smoothing: a corner's vertex is shared by the faces of its cell and material whose normals bend < SMOOTH
    limit = np.cos(np.radians(SMOOTH))
    groups = {}                   # (cell, material) -> [[normal sum, vertex index]]
    vertices, triangles = [], []
    normals = []
    for (a, b, c), material in faces:
        n = np.cross(middle[b] - middle[a], middle[c] - middle[a])
        size = np.linalg.norm(n)
        if size < 1e-12:
            continue
        normals.append(n / size)
    normal_of = iter(normals)
    corner_vertex = []
    for (a, b, c), material in faces:
        n = np.cross(middle[b] - middle[a], middle[c] - middle[a])
        if np.linalg.norm(n) < 1e-12:
            continue
        n = next(normal_of)
        tri = []
        for k in (a, b, c):
            slots = groups.setdefault((k, material), [])
            for slot in slots:
                if np.dot(slot[0] / np.linalg.norm(slot[0]), n) > limit:
                    slot[0] = slot[0] + n
                    tri.append(slot[1])
                    break
            else:
                slots.append([n.copy(), len(corner_vertex)])
                corner_vertex.append((k, material))
                tri.append(len(corner_vertex) - 1)
        triangles.append(tuple(tri))
    sums_of = {}
    for (k, material), slots in groups.items():
        for total, index in slots:
            sums_of[index] = total / np.linalg.norm(total)
    for index, (k, material) in enumerate(corner_vertex):
        n = sums_of[index]
        tangent = np.cross([0.0, 0.0, 1.0] if abs(n[2]) < 0.9 else [1.0, 0.0, 0.0], n)
        tangent /= np.linalg.norm(tangent)
        binormal = np.cross(n, tangent)
        u, v = TEXELS[material]
        p = middle[k]
        vertices.append([float(x) for x in (p[0], p[1], p[2], n[0], n[1], n[2], binormal[0], binormal[1], binormal[2],
                                            tangent[0], tangent[1], tangent[2], u, v, 0)])
    glass = (float(pane[:, 0].mean()), float(pane[:, 1].min()), float(pane[:, 1].max()), float(pane[:, 2].min()),
             float(pane[:, 2].max()))
    return vertices, [tuple(map(int, t)) for t in triangles], glass


if __name__ == "__main__":
    vertices, triangles, glass = sight(sys.argv[1])
    json.dump({"vertices": vertices, "triangles": triangles, "glass": glass}, open(sys.argv[2], "w"))
    print("%d vertices, %d triangles, glass %s" % (len(vertices), len(triangles), glass))
