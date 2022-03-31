#!/usr/bin/env python3

import uuid

if __name__ == '__main__':
    u = str(uuid.uuid4())

    print('Generated 128-bit UUID:')
    print(u)
    print()

    u = u.split('-')
    u = f'0x{u[0]}, 0x{u[1]}, 0x{u[2]}, 0x{u[3]}, 0x{u[4]}'

    print('Definition for the Zephyr RTOS:')
    print('static struct bt_uuid_128 uuid = ' +
          'BT_UUID_INIT_128(BT_UUID_128_ENCODE(' + u + '));')
