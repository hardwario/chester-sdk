#!/usr/bin/env python3
import yaml
import sys
import json
import os

PREFIX = 'com.hardwario.chester.app.'

if len(sys.argv) != 3:
    print("Usage: gen-applications.py <bundle> <type>")
    sys.exit(1)

bundle = sys.argv[1]
if not bundle.startswith('com.'):
    bundle = f'{PREFIX}{bundle}'

path = os.path.dirname(os.path.realpath(__file__))
filepath = os.path.join(path, '../applications/applications.yaml')
applications = yaml.safe_load(open(filepath))


for a in applications:
    if 'fw_bundle' in a and a['fw_bundle'] == bundle:
        if sys.argv[2] == 'cmd':

            cmd = f'FW_BUNDLE={a["fw_bundle"]}'

            if 'fw_name' in a:
                cmd += f' FW_NAME="{a["fw_name"]}"'

            if 'fw_version' in a:
                cmd += f' FW_VERSION={a["fw_version"]}'

            cmd += ' west build'

            if 'shield' in a or 'extra_args' in a:
                cmd += ' --'

            if 'shield' in a:
                cmd += f' -DSHIELD="{a["shield"]}"'

            if 'extra_args' in a:
                cmd += f' {a["extra_args"]}'

            print(cmd)

            sys.exit(0)

        elif sys.argv[2] == 'path':

            if not 'path' in a:
                d = a['fw_bundle'].split('.')[4]
            else:
                d = a['path']

            print(f'chester-sdk/chester/applications/{d}')

            sys.exit(0)

        elif sys.argv[2] == 'info':
            print(json.dumps(a, sort_keys=True))
            sys.exit(0)

        elif sys.argv[2] == 'fw_bundle':
            print(a['fw_bundle'])
            sys.exit(0)

        else:
            print("Unknown type")
            sys.exit(1)
else:
    print("Bundle not found")
    sys.exit(1)
