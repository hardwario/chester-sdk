#!/usr/bin/env python3

#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

#
# Usage: ./gen-msg-key.py <input.yaml> <output.h>
#

import yaml
import sys

def main():
    inp = sys.stdin
    out = sys.stdout
    if len(sys.argv) == 3:
        inp = open(sys.argv[1])
        out = open(sys.argv[2], 'w')

    decoder = yaml.load(inp, Loader=yaml.FullLoader)

    out.write('#ifndef MSG_KEY_H_\n')
    out.write('#define MSG_KEY_H_\n\n')
    out.write('/* This file has been generated using the script gen-msg-key.py */\n\n')
    out.write('#ifdef __cplusplus\n')
    out.write('extern "C" {\n')
    out.write('#endif\n\n')
    out.write('enum msg_key {\n')

    for i, item in enumerate(decoder):
        key, _ = item.popitem()
        out.write(f'\tMSG_KEY_{key.upper()} = {i},\n')

    out.write('};\n\n')
    out.write('#ifdef __cplusplus\n')
    out.write('}\n')
    out.write('#endif\n\n')

    out.write('#endif /* MSG_KEY_H_ */\n')

    if len(sys.argv) == 3:
        inp.close()
        out.close()


if __name__ == '__main__':
    main()
