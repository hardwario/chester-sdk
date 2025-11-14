#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

'''chester.py

West extension for HARDWARIO CHESTER platform.'''

import uuid
from textwrap import dedent

from west import log
from west.commands import WestCommand
import os
import subprocess
import sys


class ChesterLRWDecode(WestCommand):

    def __init__(self):
        super().__init__(
            'chester-lrw-decode',
            'decode LRW payload',
            dedent('''
            This helper command tests LRW Decode'''))

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name,
                                         help=self.help,
                                         description=self.description)

        parser.error = lambda msg: None  # Prevents error from being raised

        return parser

    def do_run(self, args, unknown_args):
        # Get the directory where this script resides
        script_dir = os.path.dirname(os.path.abspath(__file__))

        # Get the current working directory (where west was called from)
        cwd = os.getcwd()

        # Change to the original working directory if not already there
        if os.path.abspath(cwd) != os.path.abspath(os.getcwd()):
            os.chdir(cwd)

        # Build the path to decode.js
        decode_js_path = os.path.join(script_dir, 'decode.js')

        print(f"script_dir {script_dir}, cwd: {cwd}, unknown_args: {unknown_args}")

        # Prepare the command: node decode.js [args...]
        cmd = ['node', decode_js_path] + unknown_args

        # Run the node script and forward stdout/stderr
        try:
            result = subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError as e:
            log.err(f"decode.js failed with exit code {e.returncode}")
            sys.exit(e.returncode)
