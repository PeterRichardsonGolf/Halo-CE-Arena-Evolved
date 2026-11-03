"""Production layouts shared by the CPU-only transparent rendering tests."""
from harness import enum_with, read, structure


def thirty_two_bit(text):
    """Arena Evolved: the source as its 32-bit builds compile it, each "#ifdef HALO_64BIT" (or "#ifndef") block
    resolved for a build without HALO_64BIT, other conditionals kept. Our sources give some structures a 64-bit
    form (rasterizer_xbox_transparent_geometry.c's transparent_geometry_group); this 32-bit harness takes the
    32-bit one."""
    out, stack = [], []  # stack: for each open conditional, None (kept) or (keeping now, ours)
    for line in text.splitlines(keepends=True):
        word = line.strip().split()
        directive = word[0] if word and word[0].startswith("#") else ""
        if directive in ("#ifdef", "#ifndef", "#if"):
            ours = directive in ("#ifdef", "#ifndef") and len(word) > 1 and word[1] == "HALO_64BIT"
            outer = all(entry is None or entry[0] for entry in stack)
            if ours:
                stack.append((directive == "#ifndef", True))
                continue
            stack.append(None)
            if outer:
                out.append(line)
            continue
        if directive in ("#else", "#elif", "#endif") and stack:
            top = stack[-1]
            if top is not None:
                if directive == "#else":
                    stack[-1] = (not top[0], True)
                elif directive == "#endif":
                    stack.pop()
                continue
            if directive == "#endif":
                stack.pop()
            if all(entry is None or entry[0] for entry in stack):
                out.append(line)
            continue
        if all(entry is None or entry[0] for entry in stack):
            out.append(line)
    return "".join(out)


def transparent_types():
    declarations = []
    for path, names in [
        ("source/tag_files/tag_groups.h", ["tag_block", "tag_reference"]),
        ("source/shaders/shader_definitions.h", ["shader_radiosity_properties", "shader_physics_properties", "shader_base", "shader"]),
        ("source/rasterizer/rasterizer_geometry.h", ["vertex_buffer", "triangle_buffer"]),
        ("source/rasterizer/xbox/rasterizer_xbox_transparent_geometry.c", ["shader_transparent_generic", "shader_transparent_generic_definition", "shader_transparent_glass_definition", "transparent_geometry_group"]),
    ]:
        text = thirty_two_bit(read(path))
        declarations += [structure(text, name) for name in names]
    text = thirty_two_bit(read("source/rasterizer/xbox/rasterizer_xbox_transparent_geometry.c"))
    for member in ["_shader_type_screen", "_shader_transparent_flag_alpha_tested_bit", "_shader_transparent_glass_flag_alpha_tested_bit", "_shader_transparent_glass_reflection_type_bumped_cube_map", "_shader_radiosity_FILTHY_transparent_lit_bit", "_framebuffer_fade_mode_none", "_framebuffer_blend_function_alpha_blend", "_rasterizer_geometry_no_sort_bit"]:
        declarations.append(enum_with(text, member))
    text = read("source/rasterizer/rasterizer_geometry.h")
    declarations += [enum_with(text, "_rasterizer_vertex_type_environment_uncompressed"), enum_with(text, "_triangle_buffer_type_triangles")]
    return "\n".join(declarations)
