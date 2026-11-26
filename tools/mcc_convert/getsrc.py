# getsrc.py <scenario tag> <out.hsc>: write the scenario's script source text
import struct
import sys

ENTRY = 52  # 32 byte name, u32 size, 16 bytes padding; the text follows


def find_source(b):
    """The text of the scenario's first script source file, or b''.

    The first source entry is the one whose name field is printable, whose
    size is above zero, whose padding is zero and whose text lies inside the tag. Script node data
    holds many ';*' byte pairs, so the first pair in the tag is not it.
    """
    count = struct.unpack_from('>I', b, 64 + 1216)[0] if len(b) >= 64 + 1220 else 0
    if not count:
        return b''
    j = -1
    while True:
        j = b.find(b';*', j + 1)
        if j < 0:
            return b''
        if j < ENTRY:
            continue
        name = b[j - ENTRY:j - ENTRY + 32].split(b'\x00')[0]
        size = struct.unpack_from('>I', b, j - ENTRY + 32)[0]
        padding = b[j - ENTRY + 36:j]
        if name and all(0x20 <= c < 0x7F for c in name) and size > 0 and j + size <= len(b) and not any(padding):
            return b[j:j + size].rstrip(b'\x00')


if __name__ == '__main__':
    with open(sys.argv[1], 'rb') as f:
        data = f.read()
    with open(sys.argv[2], 'wb') as f:
        f.write(find_source(data))
