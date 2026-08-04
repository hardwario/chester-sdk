#!/usr/bin/env python3
"""Track FLASH/RAM usage of CHESTER applications across GitLab CI history.

Downloads logs of the `applications` CI job from GitLab - one pipeline per
ISO week - into a local folder, parses the linker "Memory region" summaries
(application image only, mcuboot is skipped) and generates an interactive
HTML chart with FLASH and RAM series.

Re-running the script only downloads weeks that are not yet in the local
cache, so it is cheap to run periodically.

Token: GITLAB_TOKEN environment variable, or a `token` file in the data
directory (default: scripts/memory-trend/token). Needs `read_api` scope.

Typical usage:

    ./scripts/memory-trend/memory_trend.py                  # last 2 years
    ./scripts/memory-trend/memory_trend.py --days 3650      # go further back
    ./scripts/memory-trend/memory_trend.py --no-fetch       # just regenerate HTML
"""

import argparse
import csv
import json
import os
import re
import sys
import urllib.parse
import urllib.request
from datetime import date, datetime, timedelta

DEFAULT_URL = "https://gitlab.hardwario.com"
DEFAULT_PROJECT = "chester/sdk"
DEFAULT_JOB = "applications,build"  # job renamed from `build` in 2025
DEFAULT_REF = "main"
DEFAULT_DIR = os.path.dirname(os.path.abspath(__file__))

# GitLab timestamped log prefix: "2026-07-28T11:17:53.710543Z 01O "
TIMESTAMP_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T[0-9:.]+Z \d+[A-Z]\+? ?")
ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]|\x1b\][^\x07]*\x07|section_(?:start|end):\d+:[\w.]+")
REGION_RE = re.compile(r"^\s*(\w+):\s+(\d+(?:\.\d+)?)\s*(B|KB|MB|GB)\s+(\d+(?:\.\d+)?)\s*(B|KB|MB|GB)\s+[\d.]+%")
BUILD_RE = re.compile(r"Build: chester/(applications|samples)/([\w-]+)")
ELF_RE = re.compile(r"Generating files from \S*?applications/([\w-]+)/build(?:/([\w-]+))?/zephyr/zephyr\.elf")

UNIT = {"B": 1, "KB": 1024, "MB": 1024 ** 2, "GB": 1024 ** 3}


def get_token(data_dir):
    token = os.environ.get("GITLAB_TOKEN")
    if token:
        return token.strip()
    token_file = os.path.join(data_dir, "token")
    if os.path.exists(token_file):
        with open(token_file) as f:
            return f.read().strip()
    sys.exit(f"error: no GitLab token - set GITLAB_TOKEN or create {token_file} "
             "(access token with read_api scope)")


def api(base_url, token, path, raw=False, **params):
    query = ("?" + urllib.parse.urlencode(params)) if params else ""
    req = urllib.request.Request(
        f"{base_url}/api/v4/{path}{query}",
        headers={"PRIVATE-TOKEN": token},
    )
    with urllib.request.urlopen(req, timeout=120) as resp:
        body = resp.read().decode("utf-8", errors="replace")
        return body if raw else json.loads(body)


def api_paginated(base_url, token, path, **params):
    page = 1
    while True:
        items = api(base_url, token, path, per_page=100, page=page, **params)
        if not items:
            return
        yield from items
        if len(items) < 100:
            return
        page += 1


def parse_trace(trace):
    """Extract per-build memory usage blocks from a job log.

    Returns a list of dicts: section (app name from the `Build:` banner, None
    for samples), section_id, elf_app, elf_image (from the Zephyr "Generating
    files from .../zephyr.elf" line; None in logs predating it), flash_used,
    flash_size, ram_used, ram_size.
    """
    blocks = []
    section = None  # application name from the last "Build:" banner
    section_id = 0
    pending = None  # regions collected since the last "Memory region" header

    def emit(elf_app=None, elf_image=None):
        nonlocal pending
        # RAM is called SRAM in Zephyr up to ~3.2
        ram = pending and (pending.get("RAM") or pending.get("SRAM"))
        if pending and "FLASH" in pending and ram:
            blocks.append({
                "section": section, "section_id": section_id,
                "elf_app": elf_app, "elf_image": elf_image,
                "flash_used": pending["FLASH"][0],
                "flash_size": pending["FLASH"][1],
                "ram_used": ram[0],
                "ram_size": ram[1],
            })
        pending = None

    for raw in trace.splitlines():
        line = ANSI_RE.sub("", TIMESTAMP_RE.sub("", raw)).rstrip("\r")

        m = BUILD_RE.search(line)
        if m:
            emit()  # old logs have no ELF line; flush block of previous build
            section = m.group(2) if m.group(1) == "applications" else None
            section_id += 1
            continue

        if "Memory region" in line:
            emit()
            pending = {}
            continue

        if pending is not None:
            m = REGION_RE.match(line)
            if m:
                name, used, u_unit, size, s_unit = m.groups()
                pending[name] = (
                    int(float(used) * UNIT[u_unit]),
                    int(float(size) * UNIT[s_unit]),
                )
                continue

        m = ELF_RE.search(line)
        if m:
            emit(elf_app=m.group(1), elf_image=m.group(2))

    emit()
    return blocks


def app_records(trace):
    """One record per application: the application image itself, not mcuboot.

    Blocks with an ELF path are attributed precisely (sysbuild domain must be
    the app itself, or absent for child-image era builds). Blocks without an
    ELF line (old Zephyr) fall back to the `Build:` banner; there the app
    image is the block with the largest FLASH region (the mcuboot child image
    links against a small 48 kB region).
    """
    records = []
    by_section = {}
    for b in parse_trace(trace):
        if b["elf_app"]:
            if b["elf_image"] in (None, b["elf_app"]):
                records.append({"app": b["elf_app"], **{
                    k: b[k] for k in ("flash_used", "flash_size",
                                      "ram_used", "ram_size")}})
        elif b["section"]:
            by_section.setdefault((b["section_id"], b["section"]), []).append(b)
    for (_, app), blocks in by_section.items():
        b = max(blocks, key=lambda b: b["flash_size"])
        records.append({"app": app, **{
            k: b[k] for k in ("flash_used", "flash_size",
                              "ram_used", "ram_size")}})
    return records


def iso_week(iso_date):
    y, w, _ = date.fromisoformat(iso_date[:10]).isocalendar()
    return f"{y}-W{w:02d}"


def load_state(path):
    if os.path.exists(path):
        with open(path) as f:
            return json.load(f)
    return {"weeks": {}}


def save_state(path, state):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(state, f, indent=1, sort_keys=True)
    os.replace(tmp, path)


def fetch(args, state):
    token = get_token(args.dir)
    project = urllib.parse.quote(args.project, safe="")
    logs_dir = os.path.join(args.dir, "logs")
    os.makedirs(logs_dir, exist_ok=True)
    state_path = os.path.join(args.dir, "state.json")

    since = (datetime.now() - timedelta(days=args.days)).strftime("%Y-%m-%dT00:00:00Z")
    print(f"Listing pipelines on {args.project} ref={args.ref} since {since[:10]} ...")

    # newest first; group by ISO week of pipeline creation
    weeks = {}
    for p in api_paginated(args.base_url, token, f"projects/{project}/pipelines",
                           ref=args.ref, order_by="id", sort="desc",
                           updated_after=since):
        weeks.setdefault(iso_week(p["created_at"]), []).append(p)

    current_week = iso_week(datetime.now().strftime("%Y-%m-%d"))
    todo = [w for w in sorted(weeks, reverse=True)
            if w not in state["weeks"] or w == current_week]
    print(f"{len(weeks)} week(s) with pipelines, {len(todo)} to check")

    for i, week in enumerate(todo, 1):
        entry = state["weeks"].get(week)
        picked = None
        for p in weeks[week]:  # newest pipeline of the week first
            if entry and entry.get("pipeline_id") == p["id"]:
                picked = entry  # current week already up to date
                break
            try:
                jobs = api(args.base_url, token,
                           f"projects/{project}/pipelines/{p['id']}/jobs", per_page=100)
                names = args.job.split(",")
                job = next((j for j in jobs
                            if j["name"] in names and j["status"] == "success"), None)
                if not job:
                    continue
                trace = api(args.base_url, token,
                            f"projects/{project}/jobs/{job['id']}/trace", raw=True)
            except Exception as e:
                print(f"  [{i}/{len(todo)}] {week}: pipeline {p['id']}: error: {e}",
                      file=sys.stderr)
                continue
            log_path = os.path.join(logs_dir, f"{week}_pipeline{p['id']}.log")
            with open(log_path, "w") as f:
                f.write(trace)
            records = app_records(trace)
            picked = {"pipeline_id": p["id"], "sha": p["sha"][:12],
                      "created_at": p["created_at"], "log": os.path.basename(log_path),
                      "records": records}
            print(f"  [{i}/{len(todo)}] {week}: pipeline {p['id']} "
                  f"({p['created_at'][:10]}): {len(records)} app(s)")
            break

        if picked is None:
            picked = {"pipeline_id": None}  # no successful job that week
            print(f"  [{i}/{len(todo)}] {week}: no successful '{args.job}' job")
        if picked is not entry:
            state["weeks"][week] = picked
            save_state(state_path, state)

    save_state(state_path, state)


def collect_series(state):
    """{app: [[date, flash_used, flash_size, ram_used, ram_size], ...]} sorted by date."""
    series = {}
    for week in sorted(state["weeks"]):
        entry = state["weeks"][week]
        if not entry.get("pipeline_id"):
            continue
        day = entry["created_at"][:10]
        for r in entry.get("records", []):
            series.setdefault(r["app"], []).append(
                [day, r["flash_used"], r["flash_size"], r["ram_used"], r["ram_size"]])
    return series


def write_csv(path, series):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["date", "app", "flash_used", "flash_size", "ram_used", "ram_size"])
        for app in sorted(series):
            for row in series[app]:
                w.writerow([row[0], app, *row[1:]])


def write_html(path, series):
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "memory_trend.html.tmpl")) as f:
        template = f.read()
    html = template.replace("__DATA__", json.dumps(series, separators=(",", ":")))
    html = html.replace("__GENERATED__", datetime.now().strftime("%Y-%m-%d %H:%M"))
    with open(path, "w") as f:
        f.write(html)
    print(f"Chart written to {path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base-url", default=DEFAULT_URL)
    ap.add_argument("--project", default=DEFAULT_PROJECT)
    ap.add_argument("--ref", default=DEFAULT_REF)
    ap.add_argument("--job", default=DEFAULT_JOB,
                    help="CI job name(s) to scrape, comma-separated, first match wins")
    ap.add_argument("--days", type=int, default=730,
                    help="how far back to look (default: 730 = 2 years)")
    ap.add_argument("--dir", default=DEFAULT_DIR,
                    help="data directory (logs, state, token, output)")
    ap.add_argument("--csv", metavar="FILE", help="also export parsed data as CSV")
    ap.add_argument("--output", help="output HTML file "
                    "(default: <dir>/memory-trend.html)")
    ap.add_argument("--no-fetch", action="store_true",
                    help="skip GitLab API, regenerate HTML from cached state only")
    ap.add_argument("--reparse", action="store_true",
                    help="re-run the parser on all cached logs (after parser changes)")
    args = ap.parse_args()

    os.makedirs(args.dir, exist_ok=True)
    state_path = os.path.join(args.dir, "state.json")
    state = load_state(state_path)
    if args.reparse:
        for week, entry in state["weeks"].items():
            if not entry.get("log"):
                continue
            with open(os.path.join(args.dir, "logs", entry["log"])) as f:
                entry["records"] = app_records(f.read())
        save_state(state_path, state)
        print("Cached logs re-parsed")
    if not args.no_fetch:
        fetch(args, state)

    series = collect_series(state)
    if not series:
        sys.exit("error: no data collected")
    if args.csv:
        write_csv(args.csv, series)
        print(f"CSV written to {args.csv}")
    write_html(args.output or os.path.join(args.dir, "memory-trend.html"), series)


if __name__ == "__main__":
    main()
