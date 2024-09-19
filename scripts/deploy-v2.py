#!/usr/bin/env python3

import yaml
import subprocess
import os
from datetime import datetime

# From 'chester-sdk/chester/applications' call '../scripts/deploy-v2.py'


def build_app_variant(variant, version, docs_link, cwd):
    print(f'cwd {cwd}')

    name = variant['name']
    fw_name = variant['fw_name']
    remark = variant.get('remark', '')
    print(f'{name}')

    if True:
        p = subprocess.Popen(['rm', '-rf', 'build/'], cwd=cwd)
        p.wait()

        p = subprocess.Popen(
            ['west', 'chester-update', '--variant', name, '--version', version], cwd=cwd)
        p.wait()

        p = subprocess.Popen(['west', 'build'], cwd=cwd)
        p.wait()

        p = subprocess.Popen(['hardwario', 'chester', 'app', 'fw', 'upload',
                              '--name', fw_name, '--version', version], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = p.communicate()

        # Parse the output
        unique_identifier = None
        sharable_link = None

        for line in stdout.splitlines():
            if line.startswith("Unique identifier:"):
                unique_identifier = line.split(":", 1)[1].strip()
            elif line.startswith("Sharable link"):
                sharable_link = line.split(":", 1)[1].strip()

    docs_link_combined = docs_link + '#' + name.lower().replace(" ", "-")

    date_published = datetime.now().strftime("%Y-%m-%d")

    with open("docs.md", "a") as doc_file:
        if "scale" in name:
            # CHESTER Scale don't have catalog page
            doc_file.write(f'| **{name}** ')
        else:
            doc_file.write(f'| [**{name}**]({docs_link_combined}) ')

        doc_file.write(f'| [**{version}**]({sharable_link}) ')
        doc_file.write(f'| <small>`{unique_identifier}`</small> ')
        doc_file.write(f'| {date_published} | {remark} |\n')


def build_app(name, version):
    cwd = os.getcwd()

    # Load the YAML file
    with open(name + '/project.yaml', 'r') as file:
        data = yaml.safe_load(file)

    docs_link = data['project']['fw_name'].lower().replace(" ", "-") + '.md'

    # Iterate over the variants
    for variant in data['variants']:
        build_app_variant(variant, version, docs_link, f'{cwd}/{name}')


def main():
    # build_app('clime', 'v3.0.1')
    # build_app('push', 'v3.0.1')
    # build_app('control', 'v3.0.1')
    # build_app('current', 'v3.0.1')
    # build_app('scale', 'v3.0.1')
    # build_app('meteo', 'v3.0.1')
    build_app('range', 'v3.0.1')


if __name__ == '__main__':
    main()
