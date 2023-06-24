#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

__version__ = '0.1.0'

import csv
import datetime
import re

import click
import serial

COLS = [
    'date',
    'time',
    'cycle',
    'temperature',
    'a1_minimum',
    'a1_maximum',
    'a1_average',
    'a1_median',
    'a1_filtered',
    'a2_minimum',
    'a2_maximum',
    'a2_average',
    'a2_median',
    'a2_filtered',
    'a1_sample_0',
    'a1_sample_1',
    'a1_sample_2',
    'a1_sample_3',
    'a1_sample_4',
    'a1_sample_5',
    'a1_sample_6',
    'a1_sample_7',
    'a1_sample_8',
    'a1_sample_9',
    'a1_sample_10',
    'a1_sample_11',
    'a1_sample_12',
    'a1_sample_13',
    'a1_sample_14',
    'a1_sample_15',
    'a1_sample_16',
    'a1_sample_17',
    'a1_sample_18',
    'a1_sample_19',
    'a1_sample_20',
    'a1_sample_21',
    'a1_sample_22',
    'a1_sample_23',
    'a1_sample_24',
    'a1_sample_25',
    'a1_sample_26',
    'a1_sample_27',
    'a1_sample_28',
    'a1_sample_29',
    'a1_sample_30',
    'a1_sample_31',
    'a2_sample_0',
    'a2_sample_1',
    'a2_sample_2',
    'a2_sample_3',
    'a2_sample_4',
    'a2_sample_5',
    'a2_sample_6',
    'a2_sample_7',
    'a2_sample_8',
    'a2_sample_9',
    'a2_sample_10',
    'a2_sample_11',
    'a2_sample_12',
    'a2_sample_13',
    'a2_sample_14',
    'a2_sample_15',
    'a2_sample_16',
    'a2_sample_17',
    'a2_sample_18',
    'a2_sample_19',
    'a2_sample_20',
    'a2_sample_21',
    'a2_sample_22',
    'a2_sample_23',
    'a2_sample_24',
    'a2_sample_25',
    'a2_sample_26',
    'a2_sample_27',
    'a2_sample_28',
    'a2_sample_29',
    'a2_sample_30',
    'a2_sample_31',
]


def loop(ser, cf, writer):
    row = None
    while True:
        try:
            line = ser.readline().decode('ascii').rstrip()
        except UnicodeDecodeError:
            continue

        click.echo(line)

        m = re.match(
            r'^\[\d+:\d+:\d+\.\d+,\d+\] <(dbg|inf|wrn|err)> main: ', line)
        if not m:
            continue

        msg = line[len(m.group(0)):]

        m = re.match(r'^main: Cycle start: (\d+)$', msg)
        if m:
            row = {}
            now = datetime.datetime.now()
            row['date'] = now.strftime('%Y/%m/%d')
            row['time'] = now.strftime('%H:%M:%S')
            row['cycle'] = m.group(1)

        if row is None:
            continue

        m = re.match(r'^main: Cycle stop$', msg)
        if m:
            if all(col in row for col in COLS):
                writer.writerow(row)
                cf.flush()
            row = None

        m = re.match(r'^main: Temperature: (-?[0-9]\d+(\.\d+)?) C$', msg)
        if m:
            row['temperature'] = m.group(1)

        m = re.match(r'^read_weight: Channel (A1|A2): Minimum: (-?\d+)$', msg)
        if m:
            row[f'{m.group(1).lower()}_minimum'] = m.group(2)

        m = re.match(r'^read_weight: Channel (A1|A2): Maximum: (-?\d+)$', msg)
        if m:
            row[f'{m.group(1).lower()}_maximum'] = m.group(2)

        m = re.match(r'^read_weight: Channel (A1|A2): Average: (-?\d+)$', msg)
        if m:
            row[f'{m.group(1).lower()}_average'] = m.group(2)

        m = re.match(r'^read_weight: Channel (A1|A2): Median: (-?\d+)$', msg)
        if m:
            row[f'{m.group(1).lower()}_median'] = m.group(2)

        m = re.match(r'^read_weight: Channel (A1|A2): Filtered: (-?\d+)$', msg)
        if m:
            row[f'{m.group(1).lower()}_filtered'] = m.group(2)

        m = re.match(
            r'^read_weight: Channel (A1|A2): Sample (\d+): (-?\d+)$', msg)
        if m:
            row[f'{m.group(1).lower()}_sample_{m.group(2)}'] = m.group(3)


@click.command()
@click.option('--device', required=True, help='Device path.')
@click.option('--csv-output', required=True, help='Output CSV file.')
def main(device, csv_output):
    ser = serial.Serial(device, baudrate=115200)

    with open(csv_output, 'w', newline='') as cf:
        writer = csv.DictWriter(cf, fieldnames=COLS)
        writer.writeheader()
        cf.flush()
        loop(ser, cf, writer)


if __name__ == '__main__':
    main()
