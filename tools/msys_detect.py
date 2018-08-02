#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# Copyright (c) 2016 Andrei Tatar
# Copyright (c) 2017-2018 Vrije Universiteit Amsterdam
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Tool to autodetect running memory system and produce msys files."""

import re
import os
import math
import argparse
import subprocess

from collections import namedtuple
from collections import OrderedDict

_CTRL = OrderedDict([
    ('naive', (('ddr3', 'ddr3'), ('ddr4', 'ddr4'))),
    ('intel', (('sandy', 'ddr3'), ('ivyhaswell', 'ddr3'))),
])

CONTROLLERS = {':'.join((x, y[0])) : y[1] for x in _CTRL for y in _CTRL[x]}

REMAPS = [
    None,
    'rasxor:bit=3:mask=6',
    'custom rasxor',
]


def _ask(name, lst, default=None):
    chmsg = 'Choice: ' if default is None else 'Choice [{}]: '.format(default)
    print('Select {}:'.format(name))
    while True:
        for i, s in enumerate(lst):
            print('\t{}. {}'.format(i, s))
        inp = input(chmsg)
        try:
            return lst[int(inp) if inp != '' or default is None else default]
        except (ValueError, IndexError):
            pass


def _ask_yn(question, default=True):
    while True:
        ans = input('{} {}: '.format(question, '[Y/n]' if default else '[y/N]'))
        if ans == '':
            return default
        elif ans.lower() == 'y':
            return True
        elif ans.lower() == 'n':
            return False


def _anyint(s):
    if s.startswith('0x'):
        return int(s, 16)
    elif s.startswith('0o'):
        return int(s, 8)
    elif s.startswith('0b'):
        return int(s, 2)
    else:
        return int(s)


def _ask_int(name, default=None):
    if default is not None:
        msg = '{} [{}]: '.format(name, default)
    else:
        msg = '{}: '.format(name)
    while True:
        inp = input(msg)
        try:
            return _anyint(inp if inp != '' or default is None else default)
        except ValueError:
            pass

_INTEL_SMM = namedtuple('_INTEL_SMM', ['remap', 'pci_start', 'topmem'])
_INTEL_GEOM = namedtuple('_INTEL_GEOM', ['chans', 'dimms', 'ranks'])


def _intel_smm_detect():
    MEM_GRAN = 1 << 30 # Assume system memory is rounded to gigabytes
    iomem_rex = re.compile(r'(\w+)-(\w+)\s:\s(.+)')
    with open('/proc/iomem') as f:
        lines = [x.groups() for ln in f for x in iomem_rex.finditer(ln)]
    try:
        pci_start = int(next(x[0] for x in lines if int(x[0], 16) > 0x100000 and 'PCI' in x[-1]), 16)
    except StopIteration:
        # No PCI start address found; either insufficient permissions or not x86
        return None
    topmem = (int([x for x in lines if 'PCI' not in x[-1]][-1][1], 16) + 1) - 0x100000000 + pci_start
    topmem = math.ceil(topmem / MEM_GRAN) * MEM_GRAN
    remap = len([x for x in lines if int(x[0], 16) >= 0x100000000]) > 0
    return _INTEL_SMM(remap, pci_start, topmem)


def _intel_smm_ask():
    remap = _ask_yn('PCI hole remapping active?')
    pci_start = _ask_int('PCI start address')
    topmem = _ask_int('Total memory size')
    return _INTEL_SMM(remap, pci_start, topmem)


def _intel_geom_guess():
    r = subprocess.check_output(['dmidecode', '-t', 'memory'])
    dmi = r.decode('ascii').split('\n')
    sizes = [x.strip().split(': ') for x in dmi if 'Size' in x]
    used = [x[1] != 'No Module Installed' for x in sizes]
    nused = sum(used)
    ranks = [x.strip().split(': ') for x in dmi if 'Rank' in x]
    rank_guess = 1
    for i, rk in enumerate(ranks):
        if used[i]:
            try:
                rank_guess = int(rk[1])
                break
            except ValueError:
                pass
    if len(sizes) == 1:
        return _INTEL_GEOM(1, 1, rank_guess)
    elif len(sizes) == 2:
        if nused == 2:
            return _INTEL_GEOM(1, 2, rank_guess)
        else:
            return _INTEL_GEOM(1, 1, rank_guess)
    elif len(sizes) == 4:
        if nused == 1:
            return _INTEL_GEOM(1, 1, rank_guess)
        elif nused == 4:
            # Typical dual-channel dual-dimm setup
            return _INTEL_GEOM(2, 2, rank_guess)
        elif nused == 2:
            if (used[0] and used[1]) or (used[2] and used[3]):
                # Typical single-channel dual-dimm setup
                return _INTEL_GEOM(1, 2, rank_guess)
            elif (used[0] and used[2]) or (used[1] and used[3]):
                # Typical dual-channel single-dimm setup
                return _INTEL_GEOM(2, 1, rank_guess)
            else:
                # No idea; ask user
                return None
        else:
            # No idea; ask user
            return None
    else:
        # No idea; ask user
        return None


def _intel_geom_ask():
    ch = _ask_int('Number of active channels')
    di = _ask_int('Number of DIMMs per channel')
    ra = _ask_int('Number of ranks per DIMM')
    return _INTEL_GEOM(ch, di, ra)


def _intel_opts(cntrl, interactive):
    if not interactive:
        geom = _intel_geom_guess()
        smm = _intel_smm_detect()
    else:
        geom = None
        smm = None

    print('Autodetected memory geometry')
    while True:
        if geom is not None:
            print('\t{} active channels\n\t{} DIMMs per channel\n\t{} ranks per DIMM\n'.format(*geom))
            ans = _ask_yn('Is this correct?')
        else:
            print('Unknown')
            ans = False

        if ans is True:
            break
        elif ans is False:
            geom = _intel_geom_ask()

    print('Autodetected routing options')
    while True:
        if smm is not None:
            print('PCI IOMEM start: {}; Total installed RAM: {}'.format(hex(smm.pci_start), hex(smm.topmem)))
            print('PCI memory hole remapping is [{}]'.format('enabled' if smm.remap else 'disabled'))
            ans = _ask_yn('Is this correct?')
        else:
            print('Unknown')
            ans = False
        if ans is True:
            break
        elif ans is False:
            smm = _intel_smm_ask()
    opts = []
    if geom.chans > 1:
        opts.append('2chan')
    if geom.dimms > 1:
        opts.append('2dimm')
    if geom.ranks > 1:
        opts.append('2rank')
    if smm.remap:
        opts.append('='.join(('pcibase', hex(smm.pci_start))))
        opts.append('='.join(('tom', hex(smm.topmem))))
    return opts


def _handle_rasxor(ans):
    if ans.startswith('custom'):
        bit = _ask_int('RAS XOR bit')
        mask = _ask_int('RAS XOR mask')
        return ':'.join(('rasxor', 'bit={:d}'.format(bit), 'mask={:d}'.format(mask)))
    else:
        return ans


def _writeout(cntrl, ctrlo, remaps, prettiness=1):
    csep = ';\n' if prettiness else ';'
    if prettiness > 1:
        osep = '\n\t: '
    elif prettiness:
        osep = ' : '
    else:
        osep = ':'
    return csep.join((
        osep.join(('map', cntrl, *ctrlo)),
        *(osep.join(('remap', x)) for x in remaps)
    ))


def _main():
    parser = argparse.ArgumentParser(description='Detect memory system configuration')
    parser.add_argument('-i', '--interactive-only', action='store_true',
                        help='Do not attempt to autodetect anything; configure everything interactively')
    parser.add_argument('-o', '--output', action='store', help='Output file')
    parser.add_argument('-c', '--stdout', action='store_true',
                        help='Output config to stdout')
    parser.add_argument('-p', '--pretty', action='store_const', const=2, dest='pretty', default=1,
                        help='Prettify output to make it more human-readable')
    parser.add_argument('-u', '--ugly', action='store_const', const=0, dest='pretty', default=1,
                        help='Make output more compact')
    args = parser.parse_args()

    if os.geteuid() != 0 and not args.interactive_only:
        print(80 * '-')
        print("*For best autodetection results it's recommended you run this tool as superuser*")
        print(80 * '-')
    outpath = args.output if args.output is not None else './mem.msys'

    cntrl = _ask('memory controller', list(CONTROLLERS))
    ctrlo = []

    if cntrl.startswith('intel:'):
        ctrlo = _intel_opts(cntrl, args.interactive_only)

    remaps = []
    ans = _ask_yn('Enable address pin mirroring for second rank?', False)
    if ans:
        remaps.append(':'.join(('rankmirror', CONTROLLERS[cntrl])))
    ans = _ask("additional on-DIMM remap (if unsure, select 'none')", REMAPS, 0)
    if ans:
        remaps.append(_handle_rasxor(ans))

    cfgstr = _writeout(cntrl, ctrlo, remaps, prettiness=args.pretty)
    if not args.stdout:
        if args.output is None:
            ans = input('Path to write output to [{}]: '.format(outpath))
            if ans:
                outpath = ans
        with open(outpath, 'w') as f:
            f.write(cfgstr)
    else:
        print(cfgstr)


if __name__ == '__main__':
    try:
        _main()
    except KeyboardInterrupt:
        print('\nInterrupted. Exiting...')
