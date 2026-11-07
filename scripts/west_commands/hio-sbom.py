#
# Copyright (c) 2025 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

'''hio-sbom.py

West extension for HARDWARIO CHESTER platform.

Post-processes the output of `west spdx` into a single SPDX JSON document
suitable for CRA compliance, see
https://stack-canary.com/blog/zephyr-sbom-cra-compliance/

Scan the result with grype (`grype sbom:<file>`), which matches by CPE. The
CPE is product-wide, so expect Zephyr CVEs for code that is not built or is
already fixed in the fork; they are triaged with VEX by tools/hio-vex in
gitlab.hardwario.com/security/psirt. osv-scanner cannot be used: it has no
way to match C/GIT components from SPDX PURLs.'''

import os
import re
from datetime import datetime

import yaml
from license_expression import get_spdx_licensing
from spdx_tools.spdx.model import (Actor, ActorType, CreationInfo, Document,
                                   ExternalPackageRef,
                                   ExternalPackageRefCategory, Package,
                                   Relationship, RelationshipType,
                                   SpdxNoAssertion)
from spdx_tools.spdx.parser.parse_anything import parse_file
from spdx_tools.spdx.writer.write_anything import write_file
from west import log
from west.commands import WestCommand

# module name -> (supplier, CPE template, PURL template); `{v}` is the version
COMPONENTS = {
    'zephyr': ('Zephyr Project',
               'cpe:2.3:o:zephyrproject:zephyr:{v}:*:*:*:*:*:*:*',
               'pkg:github/zephyrproject-rtos/zephyr@v{v}'),
    'nrf': ('Nordic Semiconductor', None, 'pkg:github/nrfconnect/sdk-nrf@v{v}'),
    'chester': ('HARDWARIO', None, 'pkg:github/hardwario/chester-sdk@{v}'),
    'mcuboot': ('MCUboot', None, 'pkg:github/nrfconnect/sdk-mcuboot@{v}'),
    'hal_nordic': ('Nordic Semiconductor', None, 'pkg:github/nrfconnect/sdk-hal_nordic@{v}'),
    'nrfxlib': ('Nordic Semiconductor', None, 'pkg:github/nrfconnect/sdk-nrfxlib@{v}'),
    'littlefs': ('littlefs project', None, 'pkg:github/littlefs-project/littlefs@{v}'),
    'zcbor': ('Nordic Semiconductor', None, 'pkg:github/NordicSemiconductor/zcbor@{v}'),
    'cjson': ('Dave Gamble', None, 'pkg:github/DaveGamble/cJSON@{v}'),
}

# Kconfig symbol -> Zephyr subsystem compiled into the kernel package
SUBSYSTEMS = {
    'CONFIG_BT': 'bluetooth',
    'CONFIG_NETWORKING': 'net',
    'CONFIG_MQTT_LIB': 'net/lib/mqtt',
    'CONFIG_USB_DEVICE_STACK': 'usb',
    'CONFIG_FILE_SYSTEM': 'fs',
}


def zephyr_version(project_dir):
    '''Return "major.minor.patch" from a Zephyr-style VERSION file.'''
    with open(os.path.join(project_dir, 'VERSION')) as f:
        v = dict(re.findall(r'^(\w+)\s*=\s*(\S*)', f.read(), re.M))
    return f"{v['VERSION_MAJOR']}.{v['VERSION_MINOR']}.{v['PATCHLEVEL']}"


def merge(spdx_dirs, name):
    '''Parse all *.spdx files in the given directories and merge them into one Document.

    spdx_dirs maps a prefix (the sysbuild domain) to a `west spdx` output directory.'''
    docs = {}
    for domain, spdx_dir in spdx_dirs.items():
        files = []
        if os.path.isdir(spdx_dir):
            files = sorted(f for f in os.listdir(spdx_dir) if f.endswith('.spdx'))
        if not files:
            raise SystemExit(f'no *.spdx files in {spdx_dir}, run `west spdx` first')
        for file in files:
            # SPDX IDs allow only letters, digits, '.' and '-'
            docs[(domain.replace('_', '-'), file[:-5])] = parse_file(os.path.join(spdx_dir, file))

    prefix_of_ns = {d.creation_info.document_namespace: f'{domain}-{kind}'
                    for (domain, kind), d in docs.items()}

    now = datetime.now()
    out = Document(CreationInfo(
        spdx_version='SPDX-2.3',
        spdx_id='SPDXRef-DOCUMENT',
        name=name,
        document_namespace=f'https://hardwario.com/spdxdocs/{name}-{now:%Y%m%d%H%M%S}',
        creators=[Actor(ActorType.TOOL, 'hio-sbom'),
                  Actor(ActorType.ORGANIZATION, 'HARDWARIO a.s.')],
        created=now))

    licenses = {}
    for (domain, kind), doc in docs.items():
        prefix = f'{domain}-{kind}'
        ns_of_ref = {r.document_ref_id: r.document_uri
                     for r in doc.creation_info.external_document_refs}

        def rename(spdx_id):
            # SPDXRef-X -> SPDXRef-<doc>-X; DocumentRef-D:SPDXRef-X -> SPDXRef-<D's doc>-X
            if not isinstance(spdx_id, str) or spdx_id == 'SPDXRef-DOCUMENT':
                return spdx_id
            ref, _, local = spdx_id.rpartition(':')
            target = prefix_of_ns[ns_of_ref[ref]] if ref else prefix
            return f"SPDXRef-{target}-{local.removeprefix('SPDXRef-')}"

        for item in doc.packages + doc.files:
            item.spdx_id = rename(item.spdx_id)
        if kind == 'build':
            # CMake targets like "debug" or "kernel" are not third-party components, but
            # scanners match bare package names (e.g. npm "debug"), so make them distinct
            for pkg in doc.packages:
                pkg.name = f'cmake-target-{pkg.name}'
        for rel in doc.relationships:
            rel.spdx_element_id = rename(rel.spdx_element_id)
            rel.related_spdx_element_id = rename(rel.related_spdx_element_id)
        out.packages += doc.packages
        out.files += doc.files
        out.relationships += doc.relationships
        for lic in doc.extracted_licensing_info:
            licenses[lic.license_id] = lic
    out.extracted_licensing_info = list(licenses.values())
    return out


def enrich(doc, zephyr_v, nrf_v):
    '''Fill supplier, version, CPE and PURL of known components.'''
    for pkg in doc.packages:
        module = pkg.name.removesuffix('-sources').replace('-', '_')
        if module not in COMPONENTS or not pkg.name.endswith('-sources') or pkg.supplier:
            continue
        supplier, cpe, purl = COMPONENTS[module]
        if module == 'zephyr':
            pkg.version = zephyr_v
        elif module == 'nrf':
            pkg.version = nrf_v
        v = re.sub(r'(-(dirty|off))+$', '', pkg.version or '')
        pkg.supplier = Actor(ActorType.ORGANIZATION, supplier)
        refs = [(ExternalPackageRefCategory.SECURITY, 'cpe23Type', cpe),
                (ExternalPackageRefCategory.PACKAGE_MANAGER, 'purl', purl)]
        for category, ref_type, template in refs:
            if template:
                pkg.external_references.append(
                    ExternalPackageRef(category, ref_type, template.format(v=v)))


def add_subsystems(doc, config_path, zephyr_v):
    '''Add a package for every Zephyr subsystem enabled in .config.'''
    with open(config_path) as f:
        config = f.read().splitlines()
    # the first zephyr-sources package is the application domain's, see merge()
    kernel = next((p for p in doc.packages if p.name == 'zephyr-sources'), None)
    if kernel is None:
        raise SystemExit('zephyr-sources package not found in the merged SBOM')
    apache = get_spdx_licensing().parse('Apache-2.0')
    for symbol, subsys in SUBSYSTEMS.items():
        if f'{symbol}=y' not in config:
            continue
        pkg = Package(
            spdx_id=f"SPDXRef-zephyr-subsys-{subsys.replace('/', '-')}",
            name=f'zephyr-{subsys}',
            download_location='https://github.com/zephyrproject-rtos/zephyr',
            version=zephyr_v,
            supplier=Actor(ActorType.ORGANIZATION, 'Zephyr Project'),
            files_analyzed=False,
            license_concluded=apache,
            license_declared=apache,
            copyright_text=SpdxNoAssertion(),
            external_references=[ExternalPackageRef(
                ExternalPackageRefCategory.PACKAGE_MANAGER, 'purl',
                f'pkg:github/zephyrproject-rtos/zephyr@v{zephyr_v}#subsys/{subsys}')])
        doc.packages.append(pkg)
        doc.relationships.append(
            Relationship(kernel.spdx_id, RelationshipType.CONTAINS, pkg.spdx_id))


class HioSbom(WestCommand):

    def __init__(self):
        super().__init__(
            'hio-sbom',
            'post-process west spdx output into a single SPDX JSON',
            'Merge the documents written by `west spdx` into one SPDX JSON '
            'document, fill in supplier/PURL/CPE and add Zephyr subsystems.')

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name, help=self.help,
                                         description=self.description)
        parser.add_argument('-d', '--build-dir', default='build',
                            help='build directory (default: build)')
        parser.add_argument('-o', '--output',
                            help='output file (default: <app domain>/spdx/sbom.spdx.json)')
        return parser

    def do_run(self, args, unknown_args):
        # domain name -> build directory, the default (application) domain first
        build_dir = args.build_dir
        domains = {os.path.basename(os.path.abspath(build_dir)): build_dir}
        if os.path.exists(os.path.join(build_dir, 'domains.yaml')):  # sysbuild
            with open(os.path.join(build_dir, 'domains.yaml')) as f:
                sysbuild = yaml.safe_load(f)
            default = sysbuild['default']
            names = [default] + [d['name'] for d in sysbuild['domains'] if d['name'] != default]
            domains = {n: os.path.join(build_dir, n) for n in names}
        app_dir = next(iter(domains.values()))

        output = args.output or os.path.join(app_dir, 'spdx', 'sbom.spdx.json')
        zephyr_dir, nrf_dir = (p.abspath for p in self.manifest.get_projects(['zephyr', 'nrf']))
        zephyr_v, nrf_v = zephyr_version(zephyr_dir), zephyr_version(nrf_dir)

        doc = merge({n: os.path.join(d, 'spdx') for n, d in domains.items()},
                    next(iter(domains)) + '-sbom')
        enrich(doc, zephyr_v, nrf_v)
        add_subsystems(doc, os.path.join(app_dir, 'zephyr', '.config'), zephyr_v)

        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        write_file(doc, output)
        log.inf(f'{output}: {len(doc.packages)} packages, {len(doc.files)} files, '
                f'{len(doc.relationships)} relationships')
