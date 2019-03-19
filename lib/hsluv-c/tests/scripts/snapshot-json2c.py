#!/usr/bin/env python3

import sys
import json

snapshot_file_path = 'snapshot-rev4.json'

with open(snapshot_file_path) as data_file:
    data = json.load(data_file)



sys.stdout.write("""/* Do not touch this file manually.
 * It is generated from '{0}' using 'snapshot-json2c.py`.
 */

""".format(snapshot_file_path));

sys.stdout.write("""
typedef struct TestVector_tag TestVector;
struct TestVector_tag {
    const char hex_str[8];
    double rgb_r;
    double rgb_g;
    double rgb_b;
    double xyz_x;
    double xyz_y;
    double xyz_z;
    double luv_l;
    double luv_u;
    double luv_v;
    double lch_l;
    double lch_c;
    double lch_h;
    double hsluv_h;
    double hsluv_s;
    double hsluv_l;
    double hpluv_h;
    double hpluv_s;
    double hpluv_l;
};

static const TestVector snapshot[] = {
""")

for hex_str in sorted(data):
    record = data[hex_str]

    sys.stdout.write('    {\n')
    sys.stdout.write('        "{0}",\n'.format(hex_str))
    for space in [ "rgb", "xyz", "luv", "lch", "hsluv", "hpluv" ]:
        a = str(record[space][0])
        b = str(record[space][1])
        c = str(record[space][2])

        if "." not in a:
            a += ".0"
        if "." not in b:
            b += ".0"
        if "." not in c:
            c += ".0"

        sys.stdout.write('        {1}, {2}, {3},  /* {0} */\n'.format(space, a, b, c))
    sys.stdout.write('    },\n')

sys.stdout.write("""};

static const int snapshot_n = (int)(sizeof(snapshot) / sizeof(TestVector));
""")
