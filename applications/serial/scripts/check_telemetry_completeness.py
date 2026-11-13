#!/usr/bin/env python3
# Copyright (c) 2026 HARDWARIO a.s.
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
"""Guard against a device driver that has no telemetry encoding path.

A driver can decode its device perfectly and still ship nothing, because the CBOR
(LTE/cloud) and LRW (LoRaWAN) encoders dispatch on device type via a switch with a
default case: a type with no `case` falls through and is silently dropped. The
compiler does not catch it (default swallows the -Wswitch warning), and unit tests of
the frame codecs cannot see it - it is the gap between the driver and the app encoders.

This check closes that gap statically: every device type that has a driver must have a
`case APP_DEVICE_TYPE_X` in BOTH app_cbor.c and app_lrw.c. A driver with an item table
(`.items`) has no case: both encoders send it through one generic item path, so it counts as
covered where that path is called. A driver that is intentionally shell-only can be listed in
NO_TELEMETRY below, which documents the decision.

Run from anywhere; paths are resolved relative to this file. Exits non-zero on a gap.
"""

import re
import sys
from pathlib import Path

# Device types that deliberately have no uplink telemetry (shell-only). Keep empty unless
# a driver is genuinely not meant to report - and say why here.
NO_TELEMETRY = set()

SRC = Path(__file__).resolve().parents[1] / "src"
DRIVERS_DIR = SRC / "drivers"
CBOR = SRC / "app_cbor.c"
LRW = SRC / "app_lrw.c"

TYPE_RE = re.compile(r"\.type\s*=\s*(APP_DEVICE_TYPE_\w+)")
CASE_RE = re.compile(r"case\s+(APP_DEVICE_TYPE_\w+)\s*:")
# One driver registration table; a file can hold several (drv_sensecap.c)
DRIVER_RE = re.compile(r"struct\s+app_device_driver\s+\w+\s*=\s*\{(.*?)^\};", re.S | re.M)
ITEMS_RE = re.compile(r"^\s*\.items\s*=", re.M)
# Call of the generic item path (indented, unlike the definition)
ITEMS_CALL_RE = {
    "app_cbor.c": re.compile(r"^\s+encode_device_items\(", re.M),
    "app_lrw.c": re.compile(r"^\s+(?:\w+\s*\|?=\s*)?encode_items\(", re.M),
}


def types_with_driver():
    """Every APP_DEVICE_TYPE_* that a driver registration table declares, and the subset
    whose driver has an item table."""
    found, items = set(), set()
    for c in sorted(DRIVERS_DIR.glob("drv_*.c")):
        text = c.read_text()
        for m in TYPE_RE.finditer(text):
            found.add(m.group(1))
        for body in DRIVER_RE.findall(text):
            if ITEMS_RE.search(body):
                items.update(TYPE_RE.findall(body))
    return found, items


def types_with_case(path, items):
    """Types with a dispatch case in path; item-table types if path calls the generic path."""
    text = path.read_text()
    found = set(CASE_RE.findall(text))
    if ITEMS_CALL_RE[path.name].search(text):
        found |= items
    return found


def main():
    for p in (CBOR, LRW, DRIVERS_DIR):
        if not p.exists():
            print(f"error: expected path not found: {p}", file=sys.stderr)
            return 2

    drivers, items = types_with_driver()
    drivers -= NO_TELEMETRY
    cbor = types_with_case(CBOR, items)
    lrw = types_with_case(LRW, items)

    missing_cbor = sorted(drivers - cbor)
    missing_lrw = sorted(drivers - lrw)

    if not missing_cbor and not missing_lrw:
        print(f"OK: all {len(drivers)} driver device types encode to CBOR and LRW.")
        return 0

    print("Telemetry completeness check FAILED - driver device types with no encoding path:")
    for t in missing_cbor:
        print(f"  {t}: missing 'case {t}' in {CBOR.name} (LTE/cloud data would be dropped)")
    for t in missing_lrw:
        print(f"  {t}: missing 'case {t}' in {LRW.name} (LoRaWAN uplink would be -ENOTSUP)")
    print(
        "\nAdd the encode_device_* path (and dispatch case) in the file(s) above, or list the "
        "type in NO_TELEMETRY in this script if it is intentionally shell-only.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
