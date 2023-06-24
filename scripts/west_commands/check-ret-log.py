#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

'''check-ret-log.py

Utility script to check CHESTER file for correct pair of "ret = xyz_func()"
and LOG_ERR message for string "Call `xyz_func` failed: %d".
'''

from textwrap import dedent

from west import log
from west.commands import WestCommand

import argparse
from pathlib import Path
import re
import sys
import os

EXTENSIONS = ('c', 'h')


class CheckRetLog(WestCommand):

    def __init__(self):
        super().__init__(
            'check-ret-log',
            'check LOG messages consistency',
            dedent('''
            Utility script to check CHESTER file for correct
            pair of "ret = xyz_func()" and LOG_ERR message'''))

    def check_file(self, p):
        try:
            with open(p) as f:
                lines = f.readlines()
        except UnicodeDecodeError:
            print(f'File with invalid encoding: {p}', file=sys.stderr)
            return

        expecting = ''

        for index, line in enumerate(lines):
            m = re.match(r'^(.*)ret = ([a-z0-9_]*)\((.*)$', line)
            if m:
                expecting = m.group(2)
                print(f'e {expecting}, (File {p}:{index+1})')
                if expecting == '':
                    print('Expecting')
                    raise Exception(f'File {p}:{index+1}')

            m = re.match(r'^(.*)\"Call `(.*)` failed: %d\", ret\)(.*)$', line)
            if m:
                found = m.group(2)
                print(f'f {found}')

                if expecting != found:
                    raise Exception(f'File {p}:{index+1}')

    def check_files(self, folder):
        print(folder)
        for p in folder.glob('**/*'):
            if not p.is_file() or not p.suffix or p.suffix[1:] \
               not in EXTENSIONS:
                continue

            if '/build/' in str(p):
                continue

            self.check_file(p)

        print('Script finished correctly')

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name,
                                         help=self.help,
                                         description=self.description)

        parser.add_argument('-f', '--folder', type=Path,
                            default=Path(os.getcwd()),
                            help='Folder to check')

        return parser

    def do_run(self, args, unknown_args):
        self.check_files(args.folder)
