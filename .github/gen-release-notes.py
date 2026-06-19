#!/usr/bin/env python3
#
# Copyright (c) 2024 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#
# Upload every built firmware variant to the HARDWARIO firmware server and emit
# GitHub release notes (a markdown table of links) on stdout / to --out.
#
# Reads the firmware from per-variant artifacts downloaded by the release job,
# laid out as <artifacts>/fw-<bundle-suffix>/build/zephyr/... . The bundle
# suffixes come from gen-matrix.py, the same list the build matrix iterates.
#
# Requires HARDWARIO_CLOUD_TOKEN in the environment (consumed by the
# `hardwario` CLI). Mirrors the upload + output parsing in scripts/deploy-v2.py.

import argparse
import os
import subprocess
import sys

import yaml

PREFIX = 'com.hardwario.chester.app.'
HERE = os.path.dirname(os.path.realpath(__file__))
APPS_DIR = os.path.join(HERE, '../applications')


def panic(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def matrix_applications():
    out = subprocess.check_output(
        ['python3', os.path.join(HERE, 'gen-matrix.py'), 'applications'], text=True)
    import json
    return json.loads(out)


def variant_for(suffix):
    """Map a bundle suffix (e.g. 'clime.iaq') to its variant dict."""
    name = suffix.split('.')[0]
    project_yml = os.path.join(APPS_DIR, name, 'project.yaml')
    project = yaml.safe_load(open(project_yml, 'r'))
    fw_bundle = f'{PREFIX}{suffix}'
    variants = project.get('variants') or [project['project']]
    for variant in variants:
        if variant['fw_bundle'] == fw_bundle:
            return variant
    panic(f'Variant not found: {fw_bundle}')


def upload(fw_name, version, cwd):
    """Run `hardwario chester app fw upload` and parse id + link."""
    p = subprocess.run(
        ['hardwario', 'chester', 'app', 'fw', 'upload',
         '--name', fw_name, '--version', version],
        cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        panic(f'Upload failed for {fw_name}:\n{p.stdout}\n{p.stderr}')

    identifier = link = None
    for line in p.stdout.splitlines():
        if line.startswith('Unique identifier:'):
            identifier = line.split(':', 1)[1].strip()
        elif line.startswith('Sharable link'):
            link = line.split(':', 1)[1].strip()
    if not (identifier and link):
        panic(f'Could not parse upload output for {fw_name}:\n{p.stdout}')
    return identifier, link


def main():
    parser = argparse.ArgumentParser(description='Upload firmware and build release notes')
    parser.add_argument('--artifacts', required=True,
                        help='Directory containing fw-<suffix>/ artifact dirs')
    parser.add_argument('--version', required=True, help='Firmware version (the git tag)')
    parser.add_argument('--ncs', required=True, help='NCS version')
    parser.add_argument('--zephyr', required=True, help='Zephyr version')
    parser.add_argument('--out', help='Write notes to this file (default: stdout)')
    args = parser.parse_args()

    rows = []
    for suffix in matrix_applications():
        artifact = os.path.join(args.artifacts, f'fw-{suffix}')
        if not os.path.isdir(artifact):
            panic(f'Missing firmware artifact for {suffix} ({artifact}); '
                  f'a build variant likely failed — refusing to publish a partial release')
        variant = variant_for(suffix)
        identifier, link = upload(variant['fw_name'], args.version, artifact)
        rows.append((variant['name'], link, identifier))
        print(f'uploaded {variant["fw_name"]} -> {link}', file=sys.stderr)

    lines = []
    lines.append(f'**NCS:** {args.ncs} &nbsp;&nbsp; **Zephyr:** {args.zephyr}')
    lines.append('')
    lines.append('| Firmware | Version | Link | ID |')
    lines.append('|----------|---------|------|----|')
    for name, link, identifier in rows:
        lines.append(f'| {name} | {args.version} | [download]({link}) | `{identifier}` |')
    notes = '\n'.join(lines) + '\n'

    if args.out:
        with open(args.out, 'w') as f:
            f.write(notes)
    else:
        sys.stdout.write(notes)


if __name__ == '__main__':
    main()
