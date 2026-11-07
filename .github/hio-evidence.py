#!/usr/bin/env python3
#
# Copyright (c) 2025 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

'''Write the evidence of a psirt leaf, or of the workspace shared by all leaves of a release.

The layout is defined in the README of gitlab.hardwario.com/security/psirt.
The SBOM of a leaf is written by `west hio-sbom -o <leaf>/sbom.spdx.json`.

Run from anywhere inside the west workspace. After a sysbuild build, for a leaf:

    python3 .github/hio-evidence.py --build-dir build --out <leaf> \\
        --project chester --application clime --variant clime --version v3.1.0

Once per release, for <project>/_workspace/<version> (modules.json and the git history
of all modules, needs no build):

    python3 .github/hio-evidence.py --workspace-out <dir>
'''

import argparse
import glob
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone

import yaml

# module -> upstream repository whose `main` defines the fork-local commits
UPSTREAM = {
    'zephyr': 'https://github.com/zephyrproject-rtos/zephyr',
    'mcuboot': 'https://github.com/mcu-tools/mcuboot',
    'nrf': 'https://github.com/nrfconnect/sdk-nrf',
}


def git(repo, *args, stdin=None):
    # no credential prompts, and no user diff config that would change patch-ids
    return subprocess.run(['git', '-C', repo, '-c', 'diff.noprefix=false', *args],
                          input=stdin, capture_output=True, text=True, check=True,
                          env={**os.environ, 'GIT_TERMINAL_PROMPT': '0'}).stdout


def version(project_dir):
    '''Return "major.minor.patch" from a Zephyr-style VERSION file.'''
    with open(os.path.join(project_dir, 'VERSION')) as f:
        v = dict(re.findall(r'^(\w+)\s*=\s*(\S*)', f.read(), re.M))
    return f"{v['VERSION_MAJOR']}.{v['VERSION_MINOR']}.{v['PATCHLEVEL']}"


def domains(build_dir):
    '''Return sysbuild domain name -> build directory.'''
    with open(os.path.join(build_dir, 'domains.yaml')) as f:
        return {d['name']: d['build_dir'] for d in yaml.safe_load(f)['domains']}


def built_sources(build_dirs):
    '''Return the real paths of all files compiled in any of the build directories.'''
    built = set()
    for build_dir in build_dirs:
        with open(os.path.join(build_dir, 'compile_commands.json')) as f:
            built.update(os.path.realpath(os.path.join(c['directory'], c['file']))
                         for c in json.load(f))
    return built


def module_roots(modules_files, zephyr_dir):
    '''Return module name -> real path from zephyr_modules.txt files, plus zephyr itself.'''
    roots = {'zephyr': os.path.realpath(zephyr_dir)}
    for modules_file in modules_files:
        with open(modules_file) as f:
            roots.update({n: os.path.realpath(p)
                          for n, p, _ in re.findall(r'"(.*?)":"(.*?)":"(.*?)"', f.read())})
    return roots


def module_path(path, roots):
    '''Return "<module>/<relpath>" for a file inside a module, or None.'''
    # the deepest root wins, in case a module lies inside another one
    for name, root in sorted(roots.items(), key=lambda r: -len(r[1])):
        if path.startswith(root + os.sep):
            return f'{name}/{os.path.relpath(path, root)}'
    return None


def history(name, repo):
    '''Return all ancestors of HEAD and the commits that are not in upstream.'''
    if git(repo, 'rev-parse', '--is-shallow-repository').strip() == 'true':
        raise SystemExit(f'{repo} is a shallow clone, its history would be incomplete')
    ancestors = git(repo, 'rev-list', 'HEAD').split()
    local = []
    if name in UPSTREAM:
        git(repo, 'fetch', '--quiet', '--no-tags', '--filter=blob:none', UPSTREAM[name], 'main')
        base = git(repo, 'merge-base', 'HEAD', 'FETCH_HEAD').strip()
        log = git(repo, 'log', '--format=%H %s', f'{base}..HEAD')
        patches = git(repo, 'log', '-p', '--no-ext-diff', '--no-textconv', '--no-color',
                      '--format=%H', f'{base}..HEAD')
        ids = {c: p for p, c in (line.split() for line in
                                 git(repo, 'patch-id', '--stable', stdin=patches).splitlines())}
        local = [{'sha': sha, 'subject': subject, 'patch_id': ids.get(sha)}
                 for sha, _, subject in (line.partition(' ') for line in log.splitlines())]
    return {'ancestors': ancestors, 'local': local}


def west_projects(workspace):
    '''Return real path -> (remote URL, workspace-relative path) of all west projects.'''
    out = subprocess.run(['west', 'list', '-f', '{abspath}|{url}|{path}'], cwd=workspace,
                         capture_output=True, text=True, check=True).stdout
    return {os.path.realpath(a): (u if u != 'N/A' else None, p)
            for a, u, p in (line.split('|') for line in out.splitlines())}


def sha256(path):
    with open(path, 'rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def workspace_modules(zephyr_dir):
    '''Return the zephyr_modules.txt of the west workspace, as the build system finds it.'''
    out = os.path.join(tempfile.mkdtemp(), 'zephyr_modules.txt')
    subprocess.run(['python3', os.path.join(zephyr_dir, 'scripts', 'zephyr_module.py'),
                    '--zephyr-base', zephyr_dir, '--cmake-out', out], check=True)
    return out


def write_workspace(out, roots, projects, workspace):
    '''Write modules.json and history/ of all modules.'''
    os.makedirs(os.path.join(out, 'history'), exist_ok=True)
    modules = []
    for name, root in sorted(roots.items()):
        remote, path = projects.get(root, (None, os.path.relpath(root, workspace)))
        modules.append({'name': name, 'remote': remote,
                        'revision': git(root, 'rev-parse', 'HEAD').strip(), 'path': path})
        # no mtime or file name in the gzip header, so that equal history gives equal bytes
        with open(os.path.join(out, 'history', f'{name}.json.gz'), 'wb') as raw, \
                gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as gz:
            gz.write(json.dumps(history(name, root)).encode())
    with open(os.path.join(out, 'modules.json'), 'w') as f:
        json.dump(modules, f, indent=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--build-dir', default='build', help='sysbuild build directory')
    parser.add_argument('--workspace', help='west workspace (default: west topdir)')
    parser.add_argument('--out', help='leaf directory')
    parser.add_argument('--workspace-out', help='write the release workspace here instead')
    parser.add_argument('--project', default='chester')
    parser.add_argument('--application')
    parser.add_argument('--variant')
    parser.add_argument('--version')
    args = parser.parse_args()
    if bool(args.out) == bool(args.workspace_out):
        parser.error('exactly one of --out and --workspace-out is required')
    if args.out and not (args.application and args.variant and args.version):
        parser.error('--out requires --application, --variant and --version')

    workspace = args.workspace or subprocess.run(
        ['west', 'topdir'], capture_output=True, text=True, check=True).stdout.strip()
    projects = west_projects(workspace)
    path_of = {p: a for a, (_, p) in projects.items()}
    zephyr_dir, nrf_dir = path_of['zephyr'], path_of['nrf']
    if args.workspace_out:
        write_workspace(args.workspace_out, module_roots([workspace_modules(zephyr_dir)], zephyr_dir),
                        projects, workspace)
        return
    sdk_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

    dirs = domains(args.build_dir)
    roots = module_roots([os.path.join(d, 'zephyr_modules.txt') for d in dirs.values()],
                         zephyr_dir)
    # generated files in the build directory are not module sources, even if it is in one
    build = os.path.realpath(args.build_dir) + os.sep
    compiled = sorted({module_path(s, roots) for s in built_sources(dirs.values())
                       if not s.startswith(build)} - {None})

    evidence = os.path.join(args.out, 'evidence')
    os.makedirs(os.path.join(evidence, 'config'), exist_ok=True)

    with open(os.path.join(args.out, 'meta.json'), 'w') as f:
        json.dump({'project': args.project, 'application': args.application,
                   'variant': args.variant, 'version': args.version,
                   'workspace': f'{args.project}/_workspace/{args.version}',
                   'commit': git(sdk_dir, 'rev-parse', 'HEAD').strip(),
                   'ncs': version(nrf_dir), 'zephyr': version(zephyr_dir),
                   'generated': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')},
                  f, indent=2)

    with open(os.path.join(evidence, 'compiled.txt'), 'w') as f:
        f.writelines(p + '\n' for p in compiled)

    for name, build_dir in dirs.items():
        shutil.copy(os.path.join(build_dir, 'zephyr', '.config'),
                    os.path.join(evidence, 'config', f'{name}.config'))

    patterns = ['merged_*.hex', 'dfu_application.zip', '*/zephyr/zephyr.hex',
                '*/zephyr/zephyr.signed.bin', '*/zephyr/zephyr.signed.hex']
    images = {os.path.relpath(p, args.build_dir): sha256(p) for pattern in patterns
              for p in sorted(glob.glob(os.path.join(args.build_dir, pattern)))}
    with open(os.path.join(evidence, 'images.json'), 'w') as f:
        json.dump(images, f, indent=2)


if __name__ == '__main__':
    main()
