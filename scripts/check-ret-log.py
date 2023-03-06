"""
Utility script to check CHESTER file for correct pair of "ret = xyz_func()" and LOG_ERR
message for string "Call `xyz_func` failed: %d".

Usage::

    python check-ret-log.py -f path/to/folder
"""

import argparse
from pathlib import Path
import re
import sys

EXTENSIONS = ("c", "h")

def check_file(p):
    try:
        with open(p) as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        print(f"File with invalid encoding: {p}, skipping", file=sys.stderr)
        return

    expecting = ""

    for index, line in enumerate(lines):
        m = re.match(r"^(.*)ret = ([a-z0-9_]*)\((.*)$", line)
        if (
            m
        ):
            expecting = m.group(2)
            print(f"e {expecting}, (File {p}:{index+1})")
            if(expecting == ""):
                print("Expecting")
                raise Exception(f"File {p}:{index+1}")

        m = re.match(r"^(.*)\"Call `(.*)` failed: %d\", ret\)(.*)$", line)
        if (
            m
        ):
            found = m.group(2)
            print(f"f {found}")

            if(expecting != found):
                raise Exception(f"File {p}:{index+1}")


def check_files(folder):
    for p in folder.glob("**/*"):
        if not p.is_file() or not p.suffix or p.suffix[1:] not in EXTENSIONS:
            continue

        if "/build/" in str(p):
            continue

        check_file(p)

    print("Script finnished correctly")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-f", "--folder", type=Path, required=True, help="Folder to check"
    )
    args = parser.parse_args()

    check_files(args.folder)
