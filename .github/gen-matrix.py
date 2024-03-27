#!/usr/bin/env python3
import yaml
import sys
import json
import os

PREFIX = 'com.hardwario.chester.app.'

if len(sys.argv) != 2:
    print("Usage: gen-matrix.py <type>")
    sys.exit(1)

path = os.path.dirname(os.path.realpath(__file__))


def rmprefix(s):
    return s[len(PREFIX):] if s.startswith(PREFIX) else s


if sys.argv[1] == 'applications':
    filepath = os.path.join(path, '../applications/applications.yaml')
    applications = yaml.safe_load(open(filepath))
    applications = [rmprefix(a['fw_bundle'])
                    for a in applications if 'fw_bundle' in a]
    print(json.dumps(applications, sort_keys=True))

elif sys.argv[1] == 'samples':
    samples = os.listdir(os.path.join(path, '../samples'))
    print(json.dumps([s for s in samples if os.path.isdir(
        os.path.join(path, '../samples', s))], sort_keys=True))

else:
    print("Unknown type")
    sys.exit(1)
