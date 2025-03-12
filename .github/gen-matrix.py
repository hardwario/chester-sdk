#!/usr/bin/env python3
import yaml
import sys
import json
import os

PREFIX = 'com.hardwario.chester.app.'
path = os.path.dirname(os.path.realpath(__file__))


def panic(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


if len(sys.argv) != 2:
    print("Usage: gen-matrix.py <type>")
    sys.exit(1)


if sys.argv[1] == 'applications':
    applications = []
    for name in os.listdir(os.path.join(path, '../applications')):
        if name.startswith('_') or '.' in name:
            continue
        project_yml_path = os.path.join(path, '../applications', name , 'project.yaml')
        if os.path.exists(project_yml_path):
            project = yaml.safe_load(open(project_yml_path, 'r'))
            if not 'project' in project:
                panic(f"Project {name} has no project key")

            if not project.get('variants', None):
                project['variants'] = [project['project']]

            for variant in project['variants']:
                fw_bundle = variant.get('fw_bundle', None)
                if name not in fw_bundle:
                    panic(f"Invalid fw_bundle name: name: {name} variant {variant['name']}")
                if not fw_bundle.startswith(PREFIX):
                    panic(f"Invalid fw_bundle prefix: name: {name} variant {variant['name']}")
                applications.append(fw_bundle[len(PREFIX):])

    print(json.dumps(applications, sort_keys=True))

elif sys.argv[1] == 'samples':
    samples = os.listdir(os.path.join(path, '../samples'))
    print(json.dumps([s for s in samples if os.path.isdir(
        os.path.join(path, '../samples', s))], sort_keys=True))

else:
    print("Unknown type")
    sys.exit(1)
