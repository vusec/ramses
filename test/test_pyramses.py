#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# Copyright (c) 2018 Vrije Universiteit Amsterdam
#
# This program is licensed under the GPL2+.

import sys

import pyramses

from collections import namedtuple

CASE = namedtuple('CASE', ['msys', 'ranges'])
_M = 1 << 20
_G = 1 << 30
PAGESIZE = 4096

CASES = [CASE(*x) for x in [
    ('map:naive:ddr3', [(0, 4*_G)]),
    ('map:naive:ddr4', [(0, 8*_G)]),
    ('map:intel:sandy', [(0, 4*_G)]),
    ('map:intel:sandy:2rank', [(0, 8*_G)]),
    ('map:intel:sandy:2dimm', [(0, 8*_G)]),
    ('map:intel:sandy:2chan', [(0, 8*_G)]),
    ('map:intel:sandy:2dimm:2rank', [(0, 16*_G)]),
    ('map:intel:sandy:2chan:2rank', [(0, 16*_G)]),
    ('map:intel:sandy:2chan:2dimm', [(0, 16*_G)]),
    ('map:intel:sandy:2chan:2dimm:2rank', [(0, 32*_G)]),
    ('map:intel:ivyhaswell', [(0, 4*_G)]),
    ('map:intel:ivyhaswell:2rank', [(0, 8*_G)]),
    ('map:intel:ivyhaswell:2dimm', [(0, 8*_G)]),
    ('map:intel:ivyhaswell:2chan', [(0, 8*_G)]),
    ('map:intel:ivyhaswell:2dimm:2rank', [(0, 16*_G)]),
    ('map:intel:ivyhaswell:2chan:2rank', [(0, 16*_G)]),
    ('map:intel:ivyhaswell:2chan:2dimm', [(0, 16*_G)]),
    ('map:intel:ivyhaswell:2chan:2dimm:2rank', [(0, 32*_G)]),
    ('map:intel:sandy:pcibase=0x7f800000:tom=0x100000000', [(0, 0x7f8*_M), (4*_G, 4*_G + 0x808*_M)]),
    ('map:intel:sandy:2rank:pcibase=0x7f800000:tom=0x200000000', [(0, 0x7f8*_M), (4*_G, 8*_G + 0x808*_M)]),
    ('map:intel:sandy:2dimm:pcibase=0x7f800000:tom=0x200000000', [(0, 0x7f8*_M), (4*_G, 8*_G + 0x808*_M)]),
    ('map:intel:sandy:2chan:pcibase=0x7f800000:tom=0x200000000', [(0, 0x7f8*_M), (4*_G, 8*_G + 0x808*_M)]),
    ('map:intel:ivyhaswell:pcibase=0x7f800000:tom=0x100000000', [(0, 0x7f8*_M), (4*_G, 4*_G + 0x808*_M)]),
    ('map:intel:ivyhaswell:2rank:pcibase=0x7f800000:tom=0x200000000', [(0, 0x7f8*_M), (4*_G, 8*_G + 0x808*_M)]),
    ('map:intel:ivyhaswell:2dimm:pcibase=0x7f800000:tom=0x200000000', [(0, 0x7f8*_M), (4*_G, 8*_G + 0x808*_M)]),
    ('map:intel:ivyhaswell:2chan:pcibase=0x7f800000:tom=0x200000000', [(0, 0x7f8*_M), (4*_G, 8*_G + 0x808*_M)]),
    ('map:naive:ddr3;remap:rasxor:bit=3:mask=6', [(0, 4*_G)]),
    ('map:intel:sandy:2chan;remap:rasxor:bit=3:mask=6', [(0, 8*_G)]),
    ('map:intel:ivyhaswell:2chan;remap:rasxor:bit=3:mask=6', [(0, 8*_G)]),
    ('map:intel:sandy:2rank;remap:rankmirror:ddr3', [(0, 8*_G)]),
    ('map:intel:sandy:2chan:2rank;remap:rankmirror:ddr3', [(0, 16*_G)]),
    ('map:intel:ivyhaswell:2rank;remap:rankmirror:ddr3', [(0, 8*_G)]),
    ('map:intel:ivyhaswell:2chan:2rank;remap:rankmirror:ddr3', [(0, 16*_G)]),
]]


class TestFail(Exception):
    def __init__(self, addr, da, pa, *args, **kwargs):
        self.addr = addr
        self.da = da
        self.pa = pa
        super().__init__(*args, *kwargs)


def test():
    m = pyramses.MemorySystem()
    for tc in CASES:
        m.load(tc.msys)
        gran = m.granularity(PAGESIZE)
        print('@ ' + tc.msys, end=' ', flush=True)
        for start, stop in tc.ranges:
            for addr in range(start, stop, gran):
                da = m.resolve(addr)
                pa = m.resolve_reverse(da)
                if pa != addr:
                    raise TestFail(addr, da, pa)
        print('OK', flush=True)

if __name__ == '__main__':
    try:
        test()
        print('Success')
    except TestFail as e:
        print('\n'.join((
            'FAIL',
            '{:#x} != {:#x}'.format(e.addr, e.pa),
            '{:#x} -> {!s} -> {:#x}'.format(e.addr, e.da, e.pa)
        )))
        sys.exit(1)
