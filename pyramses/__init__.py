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

import os
import sys
import struct
import ctypes
import ctypes.util
import functools

LIBNAME = 'libramses.so'


_physaddr_t = ctypes.c_ulonglong

BADADDR = _physaddr_t(-1).value


class RamsesError(Exception):
    """Exception class used to encapsulate RAMSES errors"""


@functools.total_ordering
class DRAMAddr(ctypes.Structure):
    _fields_ = [('chan', ctypes.c_ubyte),
                ('dimm', ctypes.c_ubyte),
                ('rank', ctypes.c_ubyte),
                ('bank', ctypes.c_ubyte),
                ('row', ctypes.c_ushort),
                ('col', ctypes.c_ushort)]

    def __str__(self):
        return '({0.chan:1x} {0.dimm:1x} {0.rank:1x} {0.bank:1x} {0.row:4x} {0.col:3x})'.format(self)

    def __repr__(self):
        return '{0}({1.chan}, {1.dimm}, {1.rank}, {1.bank}, {1.row}, {1.col})'.format(type(self).__name__, self)

    def __eq__(self, other):
        if isinstance(other, DRAMAddr):
            return self.numeric_value == other.numeric_value
        else:
            return NotImplemented

    def __lt__(self, other):
        if isinstance(other, DRAMAddr):
            return self.numeric_value < other.numeric_value
        else:
            return NotImplemented

    def __hash__(self):
        return self.numeric_value

    def __len__(self):
        return len(self._fields_)

    def __getitem__(self, key):
        if isinstance(key, int):
            return getattr(self, self._fields_[key][0])
        elif isinstance(key, slice):
            start, stop, step = key.indices(len(self._fields_))
            return tuple(getattr(self, self._fields_[k][0]) for k in range(start, stop, step))
        else:
            raise TypeError('{} object cannot be indexed by {}'.format(type(self).__name__, type(key).__name__))

    def same_bank(self, other):
        return (self.chan == other.chan and self.dimm == other.dimm and
                self.rank == other.rank and self.bank == other.bank)

    @property
    def numeric_value(self):
        return (self.col + (self.row << 16) + (self.bank << 32) +
                (self.rank << 40) + (self.dimm << 48) + (self.chan << 52))

    def __add__(self, other):
        if isinstance(other, DRAMAddr):
            return type(self)(
                self.chan + other.chan,
                self.dimm + other.dimm,
                self.rank + other.rank,
                self.bank + other.bank,
                self.row + other.row,
                self.col + other.col
            )
        else:
            return NotImplemented

    def __sub__(self, other):
        if isinstance(other, DRAMAddr):
            return type(self)(
                self.chan - other.chan,
                self.dimm - other.dimm,
                self.rank - other.rank,
                self.bank - other.bank,
                self.row - other.row,
                self.col - other.col
            )
        else:
            return NotImplemented


def _assert_lib():
    if _lib is None:
        init_lib()


class _MappingProps(ctypes.Structure):
    _fields_ = [('granularity', _physaddr_t),
                ('bank_cnt', ctypes.c_uint),
                ('col_cnt', ctypes.c_uint),
                ('cell_size', ctypes.c_uint)]

class _Mapping(ctypes.Structure):
    _fields_ = [('map', ctypes.c_void_p),
                ('map_reverse', ctypes.c_void_p),
                ('twiddle_gran', ctypes.c_void_p),
                ('flags', ctypes.c_int),
                ('arg', ctypes.c_void_p),
                ('props', _MappingProps)]


class MemorySystem(ctypes.Structure):
    _fields_ = [('mapping', _Mapping),
                ('nremaps', ctypes.c_size_t),
                ('remaps', ctypes.c_void_p),
                ('nallocs', ctypes.c_size_t),
                ('allocs', ctypes.c_void_p)]

    def load(self, s):
        _assert_lib()
        cs = ctypes.c_char_p(s.encode('utf-8'))
        r = _lib.ramses_msys_load(cs, ctypes.byref(self), None)
        if r:
            raise RamsesError('ramses_msys_load error: ' +
                              _lib.ramses_msys_load_strerr(r).decode('ascii'))

    def load_file(self, fname):
        with open(fname, 'r') as f:
            return self.load(f.read())

    def granularity(self, pagesize):
        _assert_lib()
        return _lib.ramses_msys_granularity(ctypes.byref(self), pagesize)

    def resolve(self, phys_addr):
        _assert_lib()
        return _lib.ramses_resolve(ctypes.byref(self), phys_addr)

    def resolve_reverse(self, dram_addr):
        _assert_lib()
        return _lib.ramses_resolve_reverse(ctypes.byref(self), dram_addr)

    def __del__(self):
        try:
            if _lib is not None:
                _lib.ramses_msys_free(ctypes.byref(self))
        except NameError:
            pass


class _TranslateArg(ctypes.Union):
    _fields_ = [('p', ctypes.c_void_p),
                ('pa', _physaddr_t),
                ('val', ctypes.c_int)]

_TranslateFunc = ctypes.CFUNCTYPE(
    _physaddr_t, ctypes.c_void_p, ctypes.c_int, _TranslateArg
)

_TranslateRangeFunc = ctypes.CFUNCTYPE(
    ctypes.c_size_t,
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_int, _TranslateArg
)

class _Translation(ctypes.Structure):
    _fields_ = [('translate', _TranslateFunc),
                ('translate_range', _TranslateRangeFunc),
                ('page_shift', ctypes.c_int),
                ('arg', _TranslateArg)]

_nulltrans = lambda: _Translation(_TranslateFunc(0), _TranslateRangeFunc(0),
                                  0, _TranslateArg(0))

class _VMMap:
    def __enter__(self):
        return self

    def __exit__(self, e_type, e_val, traceb):
        return False

    def translate(self, addr):
        return self.trans.translate(addr, self.trans.page_shift, self.trans.arg)

    def translate_range(self, addr, page_count):
        obuf = ctypes.create_string_buffer(page_count * ctypes.sizeof(ctypes.c_ulonglong))
        cnt = self.trans.translate_range(
            addr, page_count, obuf, self.trans.page_shift, self.trans.arg
        )
        unpacker = struct.iter_unpack('Q', obuf.raw)
        return [next(unpacker)[0] for _ in range(cnt)]


class Pagemap(_VMMap):
    def __init__(self, pid=None):
        pidstr = str(pid) if pid is not None else 'self'
        self.pagemap_path = os.path.join('/proc', pidstr, 'pagemap')
        self.trans = None
        self.fd = -1

    def __enter__(self):
        _assert_lib()
        self.fd = os.open(self.pagemap_path, os.O_RDONLY)
        self.trans = _nulltrans()
        _lib.ramses_translate_pagemap(ctypes.byref(self.trans), self.fd)
        return self

    def __exit__(self, e_type, e_val, traceb):
        self.trans = None
        os.close(self.fd)
        self.fd = -1
        return False


class Heurmap(_VMMap):
    def __init__(self, cont_bits, base):
        _assert_lib()
        self.trans = _nulltrans()
        _lib.ramses_translate_heuristic(ctypes.byref(self.trans), cont_bits, base)


# Module init code

try:
    _MODULE_DIR = os.path.abspath(os.path.dirname(sys.modules[__name__].__file__))
except AttributeError:
    _MODULE_DIR = os.getcwd()


_SEARCH_PATHS = [os.path.realpath(os.path.join(_MODULE_DIR, x)) for x in
    ['.', '..']
]

_lib = None


def init_lib(extra_paths=None):
    global _lib

    if extra_paths is not None:
        search_paths = list(extra_paths) + _SEARCH_PATHS
    else:
        search_paths = _SEARCH_PATHS
    for p in search_paths:
        try:
            _lib = ctypes.cdll.LoadLibrary(os.path.join(p, LIBNAME))
            break
        except OSError as e:
            pass
    else:
        _lib = ctypes.cdll.LoadLibrary(LIBNAME)

    _lib.ramses_msys_load.restype = ctypes.c_int
    _lib.ramses_msys_load.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]
    _lib.ramses_msys_load_strerr.restype = ctypes.c_char_p
    _lib.ramses_msys_load_strerr.argtypes = [ctypes.c_int]

    _lib.ramses_resolve.restype = DRAMAddr
    _lib.ramses_resolve.argtypes = [ctypes.c_void_p, _physaddr_t]
    _lib.ramses_resolve_reverse.restype = _physaddr_t
    _lib.ramses_resolve_reverse.argtypes = [ctypes.c_void_p, DRAMAddr]

    _lib.ramses_msys_granularity.restype = ctypes.c_size_t
    _lib.ramses_msys_granularity.argtypes = [ctypes.c_void_p, ctypes.c_size_t]

    _lib.ramses_translate_heuristic.restype = None
    _lib.ramses_translate_heuristic.argtypes = [ctypes.c_void_p, ctypes.c_int, _physaddr_t]
    _lib.ramses_translate_pagemap.restype = None
    _lib.ramses_translate_pagemap.argtypes = [ctypes.c_void_p, ctypes.c_int]

# End module init code
