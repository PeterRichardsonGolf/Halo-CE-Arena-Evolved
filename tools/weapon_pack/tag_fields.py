"""Scripted edits of a tag file's fields: the recipe's lighting values, blend modes, collision model, zoom,
crosshair scales and HUD elements, each a named field and a value, through Invader's field editor.

Invader (GPL-3) is a separate program on the build machine: this runs `invader-edit`, one process an edit, and
holds none of its code. The recipe gives the edits as lists:

    ["set", <field>, <value>]                    one field
    ["sets", [[<field>, <value>], ...]]          several fields in one run of the editor
    ["erase", <fields>]                          elements of a block deleted ("extra_layers[0-2]", "vehicles[*]")
    ["insert", <block>, <count>, <position>]     new elements ("end" or an index)
    ["copy", <element>, <position>]              an element copied
    ["graft", <root>, <tag>, <from>, <to>]       one part of a tag (a struct with all below it) copied onto a
                                                 part of this one: every set field of it, blocks grown to fit
                                                 (root "self": a part of this tag itself)

Usage: tag_fields.py <invader build folder> <tags folder> <tag> <edits.json>
"""
import json
import os
import re
import subprocess
import sys


class Failed(Exception):
    """an edit that could not be made, with the reason"""


def run_program(command, library_path=None):
    """a program run to its end: its standard output; Failed with what it said if it did not succeed"""
    environment = dict(os.environ)
    if library_path:
        environment["LD_LIBRARY_PATH"] = str(library_path)
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment, check=False)
    if result.returncode != 0:
        said = (result.stderr or result.stdout).decode("utf-8", "replace").strip()[-400:]
        raise Failed("%s: exit %d: %s" % (" ".join(command[:1] + command[3:]), result.returncode, said))
    return result.stdout.decode("utf-8", "replace")


class Editor:
    """Invader's field editor on one tag of a tags folder"""

    def __init__(self, invader, tags, tag, runner=None, library_path=None):
        self.program = os.path.join(str(invader), "invader-edit")
        self.tags, self.tag = str(tags), tag
        self.runner = runner or (lambda command: run_program(command, library_path))

    def run(self, *arguments):
        return self.runner([self.program, "-t", self.tags, *[str(argument) for argument in arguments], self.tag])

    def get(self, key):
        return self.run("-G", key).strip()

    def count(self, key):
        text = self.run("-C", key).strip()
        if not re.fullmatch(r"\d+", text):
            raise Failed("no count of %s in %s: %r" % (key, self.tag, text[:80]))
        return int(text)

    def listing(self):
        return self.run("-L")

    def set(self, key, value):
        self.run("-S", key, value)

    def set_many(self, pairs):
        arguments = []
        for key, value in pairs:
            arguments += ["-S", key, value]
        self.run(*arguments)

    def erase(self, key):
        self.run("-E", key)

    def insert(self, key, count, position):
        self.run("-I", key, count, position)

    def copy(self, key, position):
        self.run("-c", key, position)


def tree_to_sets(listing, source_key, destination_key):
    """the editor's listing of a tag (every field and its value, indented four columns a level) as the edits that
    give another place the part under source_key: ("S", field, value) for each set enum value, each flag and each
    value, ("C", block, count) for each block's element count; fields renamed from source_key to
    destination_key"""
    stack = []          # (depth, name)
    found = []
    for line in listing.splitlines():
        match = re.search(r"[A-Za-z0-9_]", line)
        if not match:
            continue
        depth = match.start() // 4
        parts = re.split(r"\s{2,}", line[match.start():].strip())
        body = parts[0]
        kind = parts[1] if len(parts) > 1 else ""
        named = re.match(r"^(\S+?)(?: \((.*)\))?$", body)
        if not named:
            continue
        name, value = named.group(1), named.group(2)
        while stack and stack[-1][0] >= depth:
            stack.pop()
        path = ".".join(entry for _, entry in stack)
        if kind == "enum value":
            if "(*)" in body:
                found.append(("S", path, name))
            continue
        if kind == "bitfield":
            found.append(("S", path + "." + name, value))
            continue
        array = re.match(r"^(\w+)\[(\d+)\]$", name)
        if kind == "array" and array:
            found.append(("C", (path + "." if path else "") + array.group(1), array.group(2)))
            stack.append((depth, "__arr__" + array.group(1)))
            continue
        stack.append((depth, name))
        if value is not None and kind not in ("enum", "bitmask", "struct", "array"):
            found.append(("S", ".".join(entry for _, entry in stack), value))
    edits = []
    for operation, key, value in found:
        key = re.sub(r"__arr__\w+\.", "", key)
        if not key.startswith(source_key + ".") and key != source_key:
            continue
        edits.append((operation, destination_key + key[len(source_key):], value))
    return edits


def graft(editor, source, source_key, destination_key):
    """the part of source's tag under source_key onto editor's tag under destination_key"""
    for operation, key, value in tree_to_sets(source.listing(), source_key, destination_key):
        if operation == "C":
            have = editor.count(key)
            if have < int(value):
                editor.insert(key, int(value) - have, "end")
        else:
            editor.set(key, value)


def apply(editor, edits, editor_for=None):
    """the recipe's edits made in order; editor_for(root, tag) gives the editor of a graft's source"""
    for edit in edits:
        operation = edit[0]
        if operation == "set" and len(edit) == 3:
            editor.set(edit[1], edit[2])
        elif operation == "sets" and len(edit) == 2:
            editor.set_many(edit[1])
        elif operation == "erase" and len(edit) == 2:
            editor.erase(edit[1])
        elif operation == "insert" and len(edit) == 4:
            editor.insert(edit[1], edit[2], edit[3])
        elif operation == "copy" and len(edit) == 3:
            editor.copy(edit[1], edit[2])
        elif operation == "graft" and len(edit) == 5 and editor_for:
            graft(editor, editor_for(edit[1], edit[2]), edit[3], edit[4])
        else:
            raise Failed("not an edit: %r" % (edit,))


def main(arguments):
    if len(arguments) != 4:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    invader, tags, tag, edits = arguments
    with open(edits) as file:
        listed = json.load(file)
    try:
        apply(Editor(invader, tags, tag), listed, lambda root, name: Editor(invader, root, name))
    except Failed as failure:
        print("tag_fields: failed: %s" % failure, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
