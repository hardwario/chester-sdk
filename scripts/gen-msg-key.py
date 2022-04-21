#!/usr/bin/env python3
#
# Usage: ./gen-msg-key.py <input.yaml> <output.h>
#

import yaml
import sys

def main():
    decoder = yaml.load(open(sys.argv[1]), Loader=yaml.FullLoader)

    with open(sys.argv[2], 'w') as f:
        f.write('#ifndef MSG_KEY_H_\n')
        f.write('#define MSG_KEY_H_\n\n')
        f.write('/* This file has been generated using the script gen-msg-key.py */\n\n')
        f.write('#ifdef __cplusplus\n')
        f.write('extern "C" {\n')
        f.write('#endif\n\n')
        f.write('enum msg_key {\n')

        for i, item in enumerate(decoder):
            key, _ = item.popitem()
            f.write(f'\tMSG_KEY_{key.upper()} = {i},\n')

        f.write('};\n\n')
        f.write('#ifdef __cplusplus\n')
        f.write('}\n')
        f.write('#endif\n\n')

        f.write('#endif /* MSG_KEY_H_ */\n')

if __name__ == '__main__':
    main()
