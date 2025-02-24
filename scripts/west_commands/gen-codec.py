#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

'''gen-codec.py

West extension for HARDWARIO CHESTER platform.'''

import uuid
from textwrap import dedent

from west import log
from west.commands import WestCommand
import os
import cbor2
import yaml
import re
import hashlib
import struct

key_separator = '__'
key_words_decoder = ('$key', '$div', '$mul', '$add',
                     '$sub', '$fpp', '$tso', '$tsp', '$enum', '$mbus')
key_words_encoder = ('$div', '$mul', '$add', '$sub', '$fpp', '$enum', '$type')
type_words = ('int', 'float', 'bool', 'string')


def key_normalize(key):
    return re.sub('[^A-Z\d]', '_', key.upper())


def iter_items(items, prefix, key_words):
    # print('items', items)
    for item in items:
        if isinstance(item, dict):
            if len(item) != 1:
                raise Exception(f'Invalid item {item}')
            key = list(item.keys())[0]
            values = item[key]
            if key.startswith('$'):
                if key in key_words:
                    if key == '$type':
                        if values not in type_words:
                            raise Exception(f'Invalid type {values}')
                    continue
                raise Exception(f'Invalid key {key}')
            key = key_normalize(key)
            yield f'{prefix}{key}', values
            if isinstance(values, list):
                yield from iter_items(values, f'{prefix}{key}{key_separator}', key_words)


def load_yaml(filename):
    with open(filename, 'r') as f:
        codec = yaml.safe_load(f)

        key_words = None

        if 'version' not in codec:
            raise Exception(f'Invalid codec file {filename} missing version')
        if codec['version'] != 2:
            raise Exception(f'Invalid codec version {codec["version"]}')

        if 'type' not in codec:
            raise Exception(f'Invalid codec file {filename} missing type')

        if codec['type'] == 'decoder':
            key_words = key_words_decoder
        elif codec['type'] == 'encoder':
            key_words = key_words_encoder
        else:
            raise Exception(f'Invalid codec file {filename} invalid type')

        if 'name' not in codec:
            raise Exception(f'Invalid codec file {filename} missing name')

        if 'schema' not in codec:
            raise Exception(f'Invalid codec file {filename} missing schema')

        if not isinstance(codec['schema'], list):
            raise Exception(f'Invalid codec file {filename} invalid schema')

        key_list = []
        key_dict = {}

        for key, values in iter_items(codec['schema'], '', key_words):
            key_list.append(key)
            key_dict[key] = values

        buffer = cbor2.dumps(codec)

        h = hashlib.sha256(buffer)
        a, b, c, d = struct.unpack('QQQQ', h.digest())
        hashInt = a ^ b ^ c ^ d

        return {
            'name': codec['name'],
            'type': codec['type'],
            'key_list': key_list,
            'key_dict': key_dict,
            'hash': hashInt,
            'buffer': buffer,
        }


def write_h(filename: str, codecs: dict):

    with open(filename, 'w') as f:
        f.write('#ifndef CODEC_H_\n')
        f.write('#define CODEC_H_\n\n')
        f.write('/* This file has been generated using the script gen-codec.py */\n\n')
        f.write('#ifdef __cplusplus\n')
        f.write('extern "C" {\n')
        f.write('#endif\n\n')

        f.write('#define CODEC_CLOUD_DECODER_HASH ((uint64_t)0x%016x)\n' %
                (codecs['decoder']['hash'] if codecs['decoder'] else 0))
        f.write('#define CODEC_CLOUD_ENCODER_HASH ((uint64_t)0x%016x)\n' %
                (codecs['encoder']['hash'] if codecs['encoder'] else 0))

        f.write('\n')

        for codec in codecs.values():
            if not codec:
                continue
            if codec['type'] == 'decoder':
                # decoder in cloud is encoder in device
                f.write('enum codec_key_e {\n')
                prefix = 'CODEC_KEY_E_'
            elif codec['type'] == 'encoder':
                # encoder in cloud is decoder in device
                f.write('enum codec_key_d {\n')
                prefix = 'CODEC_KEY_D_'
            for i, item in enumerate(codec['key_list']):
                f.write(f'\t{prefix}{item} = {i},\n')
            f.write('};\n\n')

        f.write('#define CODEC_CLOUD_OPTIONS_STATIC(_name) \\\n')
        if codecs['decoder']:
            f.write(
                '\tstatic const uint8_t _name##_cloud_decoder[] = CLOUD_DECODER_BUFFER; \\\n')
        if codecs['encoder']:
            f.write(
                '\tstatic const uint8_t _name##_cloud_encoder[] = CLOUD_ENCODER_BUFFER; \\\n')
        f.write('\tstatic struct ctr_cloud_options _name = { \\\n')
        f.write('\t\t.decoder_hash = CODEC_CLOUD_DECODER_HASH, \\\n')
        f.write('\t\t.encoder_hash = CODEC_CLOUD_ENCODER_HASH, \\\n')
        f.write('\t\t.decoder_buf = %s, \\\n' %
                ('_name##_cloud_decoder' if codecs['decoder'] else 'NULL'))
        f.write('\t\t.decoder_len = %d, \\\n' %
                (len(codecs['decoder']['buffer']) if codecs['decoder'] else 0))
        f.write('\t\t.encoder_buf = %s, \\\n' %
                ('_name##_cloud_encoder' if codecs['encoder'] else 'NULL'))
        f.write('\t\t.encoder_len = %d, \\\n' %
                (len(codecs['encoder']['buffer']) if codecs['encoder'] else 0))
        f.write('}\n\n')

        for codec in codecs.values():
            if not codec:
                continue
            f.write('#define CLOUD_%s_BUFFER { \\\n' % codec['type'].upper())
            for i in range(0, len(codec['buffer']), 8):
                f.write('\t')
                for j in range(8):
                    if i + j < len(codec['buffer']):
                        f.write('0x%02x, ' % codec['buffer'][i + j])
                f.write('\\\n')
            f.write('}\n\n')

        f.write('#ifdef __cplusplus\n')
        f.write('}\n')
        f.write('#endif\n\n')

        f.write('#endif /* CODEC_H_ */\n')


class GenerateCodec(WestCommand):

    def __init__(self):
        super().__init__(
            'gen-codec',
            'generate app codec',
            dedent('''
            This helper command generate application codec.'''))

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name,
                                         help=self.help,
                                         description=self.description)

        parser.add_argument('-d', '--decoder', type=str, default='codec/cbor-decoder.yaml',
                            help='Decoder YAML file')

        parser.add_argument('-e', '--encoder', type=str, default='codec/cbor-encoder.yaml',
                            help='Encoder YAML file')

        parser.add_argument('-o', '--output', type=str, default='src/app_codec.h',
                            help='Output file')

        return parser

    def do_run(self, args, unknown_args):
        print('Generating codec...')

        codecs = {
            'decoder': None,
            'encoder': None,
        }

        if os.path.isfile(args.decoder):
            print('Loaded decoder from %s' % args.decoder)
            codec = load_yaml(args.decoder)
            if codec['type'] != 'decoder':
                raise Exception('Invalid codec file, expecting decoder')
            codecs['decoder'] = codec

        if os.path.isfile(args.encoder):
            print('Loaded encoder from %s' % args.encoder)
            codec = load_yaml(args.encoder)
            if codec['type'] != 'encoder':
                raise Exception('Invalid codec file, expecting encoder')
            codecs['encoder'] = codec

        write_h(args.output, codecs)

        print('Saved to %s' % args.output)
