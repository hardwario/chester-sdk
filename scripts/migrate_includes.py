#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

"""
Utility script to migrate CHESTER-based projects to the new <chester/...> include
prefix.

Usage::

    python migrate_includes.py -p path/to/chester-based-project
"""

import argparse
from pathlib import Path
import re
import sys


ZEPHYR_BASE = Path(__file__).parents[1]

print(ZEPHYR_BASE)

EXTENSIONS = ("c", "cpp", "h", "dts", "dtsi", "rst", "S", "overlay", "ld")


def update_includes(project, dry_run):
    for p in project.glob("**/*"):
        if not p.is_file() or not p.suffix or p.suffix[1:] not in EXTENSIONS:
            continue

        try:
            with open(p) as f:
                lines = f.readlines()
        except UnicodeDecodeError:
            print(f"File with invalid encoding: {p}, skipping", file=sys.stderr)
            continue

        content = ""
        migrate = False
        for line in lines:
            m = re.match(r"^(.*)#include <(.*\.h)>(.*)$", line)
            if (
                m
                and not m.group(2).startswith("chester/")
                and (ZEPHYR_BASE / "include" / "chester" / m.group(2)).exists()
            ):
                content += (
                    m.group(1)
                    + "#include <chester/"
                    + m.group(2)
                    + ">"
                    + m.group(3)
                    + "\n"
                )
                migrate = True
            else:
                content += line

        if migrate:
            print(f"Updating {p}{' (dry run)' if dry_run else ''}")
            if not dry_run:
                with open(p, "w") as f:
                    f.write(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--project", type=Path, required=True, help="CHESTER-based project path"
    )
    parser.add_argument("--dry-run", action="store_true", help="Dry run")
    args = parser.parse_args()

    update_includes(args.project, args.dry_run)
