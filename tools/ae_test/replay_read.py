#!/usr/bin/env python3
"""Reads a replay file (port/linux/game/ae_replay_rules.h's format) without the game, with every length bounded as the
game's reader bounds it: the header's fields, then the records through the zlib stream. One line says what it is:

    python3 tools/ae_test/replay_read.py <file.ocreplay> [--network 44]

    replay: <file>: <map> (<game type>), <seconds> s, <records> records (<kind> <count> <KB> ...), stream <KB> KB
    replay: <file>: refused: <why>

A file a crash or a full disk cut short reads to its last sync, and says so ("cut after <seconds> s"). Exit 0 when it
reads, 1 when it is refused."""
import argparse
import struct
import sys
import zlib

MAGIC = b"OCREPLAY"
FORMAT = 5
HEADER_SIZE = 384
RECORD_HEADER_SIZE = 8
TICKS_PER_SECOND = 30
LONGEST_TICKS = 24 * 60 * 60 * TICKS_PER_SECOND
KINDS = {1: "datagram", 2: "reliable", 3: "to-recorder", 4: "game-message", 5: "game", 6: "own-actions", 7: "setup"}
TEXT_FIELDS = [("map", 40, 128, True), ("variant", 168, 16, True), ("version", 184, 96, True),
               ("server", 288, 64, False), ("player", 352, 32, False)]
# (the stream inflated a piece at a time: a file's records are never held whole)
CHUNK = 1 << 16


class Refused(Exception):
    pass


def text_field(raw, ascii_only):
    """the field's text: its end within its bytes, nothing but zeros after it, no control bytes"""
    end = raw.find(b"\0")
    if end < 0 or raw[end:].strip(b"\0"):
        raise Refused("its header is damaged")
    text = raw[:end]
    if any(b < 0x20 or b == 0x7F or (ascii_only and b >= 0x7F) for b in text):
        raise Refused("its header is damaged")
    return text.decode("utf-8", "replace")


def read_header(head, network):
    if len(head) < HEADER_SIZE or head[:8] != MAGIC:
        raise Refused("not a replay")
    fmt, net, build, tps, start, size = struct.unpack_from("<6I", head, 8)
    length, flags = struct.unpack_from("<2I", head, 280)
    if fmt != FORMAT:
        raise Refused("made by a later build" if fmt > FORMAT else "made by an earlier build")
    if net != network:
        raise Refused("recorded by a build of another network version")
    if size != HEADER_SIZE or tps != TICKS_PER_SECOND or length > LONGEST_TICKS or flags & ~1:
        raise Refused("its header is damaged")
    header = {"build": build, "start": start, "length": length, "flags": flags}
    for name, offset, width, ascii_only in TEXT_FIELDS:
        header[name] = text_field(head[offset:offset + width], ascii_only)
    if not header["map"]:
        raise Refused("its header is damaged")
    return header


def read_records(stream, header):
    """each record's header checked against what is left, the kind known, the ticks in order and within the game"""
    inflater = zlib.decompressobj()
    pending = b""
    counts, sizes = {}, {}
    last_tick = header["start"]
    most_tick = header["start"] + LONGEST_TICKS
    total = 0
    while True:
        piece = stream.read(CHUNK)
        if not piece:
            break
        total += len(piece)
        try:
            pending += inflater.decompress(piece, CHUNK * 4)
            while inflater.unconsumed_tail:
                pending += inflater.decompress(inflater.unconsumed_tail, CHUNK * 4)
                pending = take(pending, counts, sizes, last_tick, most_tick)
                last_tick = take.last_tick
        except zlib.error:
            raise Refused(f"its stream is damaged after {last_tick - header['start']} ticks")
        pending = take(pending, counts, sizes, last_tick, most_tick)
        last_tick = take.last_tick
    cut = not inflater.eof
    if pending and not cut:
        raise Refused("its last record is cut")
    return counts, sizes, total, cut, last_tick


def take(pending, counts, sizes, last_tick, most_tick):
    """the whole records at the start of pending counted; what is left of it"""
    at = 0
    take.last_tick = last_tick
    while len(pending) - at >= RECORD_HEADER_SIZE:
        kind, zero, size, tick = struct.unpack_from("<BBHI", pending, at)
        if kind not in KINDS or zero or tick < take.last_tick or tick > most_tick:
            raise Refused(f"a record of kind {kind} at tick {tick} is not one")
        if size > len(pending) - at - RECORD_HEADER_SIZE:
            break
        counts[kind] = counts.get(kind, 0) + 1
        sizes[kind] = sizes.get(kind, 0) + size
        take.last_tick = tick
        at += RECORD_HEADER_SIZE + size
    return pending[at:]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("file")
    parser.add_argument("--network", type=int, default=44)
    args = parser.parse_args(argv)
    try:
        with open(args.file, "rb") as stream:
            header = read_header(stream.read(HEADER_SIZE), args.network)
            counts, sizes, total, cut, last_tick = read_records(stream, header)
    except Refused as why:
        print(f"replay: {args.file}: refused: {why}")
        return 1
    except OSError as why:
        print(f"replay: {args.file}: refused: {why.strerror}")
        return 1
    if not counts.get(5) or not counts.get(1):
        print(f"replay: {args.file}: refused: no game record or no datagrams")
        return 1
    kinds = " ".join(f"{KINDS[k]} {counts[k]} {sizes[k] // 1024}KB" for k in sorted(counts))
    seconds = (last_tick - header["start"]) // TICKS_PER_SECOND
    print(f"replay: {args.file}: {header['map']} ({header['variant']}), {seconds} s, {sum(counts.values())} records "
          f"({kinds}), stream {total // 1024} KB" + (f", cut after {seconds} s" if cut else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
