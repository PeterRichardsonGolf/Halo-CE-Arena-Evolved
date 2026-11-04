"""A tag file a recipe takes whole from an extra source (a zip's member, given as an art input): copied as it is,
byte for byte, so a recipe whose own source is another kind (a map's tags) can carry one tag of another release.
The recipe pins the copy by its SHA-256 (a "derived" file).

    copy_tag.py <in> <out>
"""
import shutil
import sys


def main(arguments):
    if len(arguments) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    shutil.copyfile(arguments[1], arguments[2])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
