/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "pcihole.h"

#define _4G (1ULL << 32)

physaddr_t pcihole_remap(physaddr_t addr, physaddr_t pcibase, physaddr_t tom)
{
	if (addr < tom) {
		if (addr >= pcibase && addr < _4G) {
			return RAMSES_BADADDR;
		} else {
			return addr;
		}
	} else {
		return pcibase + (addr - tom);
	}
}

physaddr_t pcihole_remap_reverse(physaddr_t addr, physaddr_t pcibase, physaddr_t tom)
{
	return ((addr >= pcibase && addr < _4G) ? (addr - pcibase + tom) : addr);
}

physaddr_t pcihole_offset(physaddr_t addr, physaddr_t holebase, uint32_t holeoffset)
{
	if (addr >= holebase) {
		if (addr < _4G) {
			return RAMSES_BADADDR;
		} else {
			return addr - holeoffset;
		}
	} else {
		return addr;
	}
}

physaddr_t pcihole_offset_reverse(physaddr_t addr, physaddr_t holebase, uint32_t holeoffset)
{
	return (addr >= holebase) ? addr + holeoffset : addr;
}
