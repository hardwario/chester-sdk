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


class ChesterGenerateBluetoothUuid(WestCommand):

    def __init__(self):
        super().__init__(
            'chester-gen-bt-uuid',
            'generate Bluetooth UUID',
            dedent('''
            This helper command generate Bluetooth UUID for GATT service
            or GATT characteristic, and can be copy-pasted to the object
            definition.'''))

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name,
                                         help=self.help,
                                         description=self.description)
        return parser

    def do_run(self, args, unknown_args):
        u = str(uuid.uuid4())

        log.inf('Generated 128-bit UUID:')
        log.inf(u)
        log.inf()

        u = u.split('-')
        u = f'0x{u[0]}, 0x{u[1]}, 0x{u[2]}, 0x{u[3]}, 0x{u[4]}'

        log.inf('Definition for the Zephyr RTOS:')
        log.inf('static struct bt_uuid_128 uuid = ' +
                'BT_UUID_INIT_128(BT_UUID_128_ENCODE(' + u + '));')
