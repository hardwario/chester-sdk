#!/usr/bin/env python3
import yaml
import sys
import json
import os

PREFIX = 'com.hardwario.chester.app.'


def panic(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


if len(sys.argv) != 3:
    panic("Usage: gen-applications.py <app> <out>")

app = sys.argv[1]
out = sys.argv[2]

if out not in ['path', 'cmd']:
    panic("Unknown out type supported: path, cmd")

fw_bundle = f'{PREFIX}{app}'
path = os.path.dirname(os.path.realpath(__file__))

name = app.split('.')[0]

if out == 'path':
    print(f'chester-sdk/chester/applications/{name}')
    sys.exit(0)

project_yml_path = os.path.join(path, '../applications', name , 'project.yaml')
if os.path.exists(project_yml_path):
    project = yaml.safe_load(open(project_yml_path, 'r'))
    if not project.get('variants', None):
        if out == 'cmd':
            print(f'rm -rf build && west chester-update && west build')
            sys.exit(0)

    for variant in project['variants']:
        if variant['fw_bundle'] == fw_bundle:
            # print(json.dumps(variant, sort_keys=True))
            # sys.exit(0)
            if out == 'cmd':
                print(f'rm -rf build && west chester-update --variant "{variant["name"]}" && west build')
                sys.exit(0)

    panic(f'Variant not found: {fw_bundle}')
else:
    panic(f'Project not found: {name}')
