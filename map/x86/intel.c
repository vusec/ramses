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

#include <ramses/map/x86/intel.h>

#include <assert.h>

#include "bitops.h"
#include "pcihole.h"

#define MW_BITS 3
#define COL_BITS 10

static struct DRAMAddr drammap_sandy(physaddr_t addr, int geom_flags)
{
	struct DRAMAddr retval = {0,0,0,0,0,0};
	/* Idx: 0 */
	if (geom_flags & INTEL_DUALCHAN) {
		retval.chan = BIT(6, addr);
		addr = POP_BIT(6, addr);
	}
	/* Discard index into memory word */
	addr >>= MW_BITS;
	/* Idx: 3 */
	retval.col = addr & LS_BITMASK(COL_BITS);
	addr >>= COL_BITS;
	/* Idx: 13/14 */
	/* HACK: DIMM selection rule assumed (and you know what they say about when you assume) */
	if (geom_flags & INTEL_DUALDIMM) {
		retval.dimm = BIT(3,addr);
		addr = POP_BIT(3,addr);
	}
	/* Idx: 13/14, Possible Holes: 16/17 */
	if (geom_flags & INTEL_DUALRANK) {
		retval.rank = BIT(3,addr);
		addr = POP_BIT(3,addr);
	}
	/* Idx: 13/14, Possible Holes: 16/17, 17/18 */
	for (int i = 0; i < 3; i++) {
		retval.bank |= (BIT(0,addr) ^ BIT(3,addr)) << i;
		addr >>= 1;
	}
	retval.row = addr & LS_BITMASK(16);
	addr >>= 16;
	/* Sanity check that address fits in memory geometry */
	assert(addr == 0);
	return retval;
}

static physaddr_t drammap_reverse_sandy(struct DRAMAddr addr, int geom_flags)
{
	physaddr_t retval = addr.row & LS_BITMASK(16);
	if (geom_flags & INTEL_DUALRANK) {
		retval <<= 1;
		retval |= addr.rank & 1;
	}
	if (geom_flags & INTEL_DUALDIMM) {
		retval <<= 1;
		retval |= addr.dimm & 1;
	}
	for (int i = 2; i >= 0; i--) {
		retval <<= 1;
		retval |= BIT(i, addr.bank) ^ BIT(i, addr.row);
	}
	if (geom_flags & INTEL_DUALCHAN) {
		retval <<= 7;
		retval |= (addr.col >> 3) & LS_BITMASK(7);
		retval <<= 1;
		retval |= addr.chan & 1;
		retval <<= 3;
		retval |= addr.col & LS_BITMASK(3);
	} else {
		retval <<= COL_BITS;
		retval |= addr.col & LS_BITMASK(COL_BITS);
	}
	retval <<= MW_BITS;

	return retval;
}

static struct DRAMAddr drammap_ivyhaswell(physaddr_t addr, int geom_flags)
{
	struct DRAMAddr retval = {0,0,0,0,0,0};
	/* Idx: 0 */
	if (geom_flags & INTEL_DUALCHAN) {
		retval.chan = BIT(7,addr) ^ BIT(8,addr) ^ BIT(9,addr) ^ BIT(12,addr) ^
					  BIT(13,addr) ^ BIT(18,addr) ^ BIT(19,addr);
		addr = POP_BIT(7,addr);
	}
	/* Discard index into memory word */
	addr >>= MW_BITS;
	/* Idx: 3 */
	retval.col = addr & LS_BITMASK(COL_BITS);
	addr >>= COL_BITS;
	/* Idx: 13/14 */
	if (geom_flags & INTEL_DUALDIMM) {
		retval.dimm = BIT(2,addr);
		addr = POP_BIT(2,addr);
	}
	/* Idx: 13/14, Possible Holes: 15/16 */
	if (geom_flags & INTEL_DUALRANK) {
		retval.rank = BIT(2,addr) ^ BIT(6,addr);
		addr = POP_BIT(2,addr);
	}
	/* Idx: 13/14, Possible Holes: 15/16, 16/17 */
	for (int i = 0; i < 2; i++) {
		retval.bank |= (BIT(0,addr) ^ BIT(3,addr)) << i;
		addr >>= 1;
	}
	retval.bank |= (BIT(0,addr) ^ BIT((geom_flags & INTEL_DUALRANK) ? 4 : 3, addr)) << 2;
	addr >>= 1;

	retval.row = addr & LS_BITMASK(16);
	addr >>= 16;
	/* Sanity check that address fits in memory geometry */
	assert(addr == 0);
	return retval;
}

static physaddr_t drammap_reverse_ivyhaswell(struct DRAMAddr addr, int geom_flags)
{
	physaddr_t retval = addr.row & LS_BITMASK(16);
	if (geom_flags & INTEL_DUALRANK) {
		retval <<= 1;
		retval |= BIT(2, addr.bank) ^ BIT(3, addr.row);
		retval <<= 1;
		retval |= (addr.rank & 1) ^ BIT(2, addr.row);
	} else {
		retval <<= 1;
		retval |= BIT(2, addr.bank) ^ BIT(2, addr.row);
	}
	if (geom_flags & INTEL_DUALDIMM) {
		retval <<= 1;
		retval |= addr.dimm & 1;
	}
	for (int i = 1; i >= 0; i--) {
		retval <<= 1;
		retval |= BIT(i, addr.bank) ^ BIT(i, addr.row);
	}
	if (geom_flags & INTEL_DUALCHAN) {
		retval <<= 6;
		retval |= (addr.col >> 4) & LS_BITMASK(6);
		retval <<= 1;
		retval |= (addr.chan & 1) ^ BIT(1,retval) ^ BIT(2,retval) ^
		          BIT(5,retval) ^ BIT(6,retval) ^ BIT(11,retval) ^ BIT(12,retval);
		retval <<= 4;
		retval |= addr.col & LS_BITMASK(4);
	} else {
		retval <<= COL_BITS;
		retval |= addr.col & LS_BITMASK(COL_BITS);
	}
	retval <<= MW_BITS;

	return retval;
}


static inline size_t contiguous_twiddle(long long mask, size_t base, int maxbits)
{
	int lsb = leastsetbit(mask);
	if (lsb >= 0 && (!maxbits || lsb < maxbits)) {
		return base << lsb;
	} else {
		return 0;
	}
}

static inline int has_pcihole(const struct IntelCntrlOpts *o)
{ return o->pcibase && o->mem_top; }


static struct DRAMAddr map_sandy(physaddr_t addr, int flags, void *opts)
{
	const struct IntelCntrlOpts *o = (struct IntelCntrlOpts *)opts;
	if (has_pcihole(o)) {
		addr = pcihole_remap(addr, o->pcibase, o->mem_top);
	}
	return drammap_sandy(addr, o->geom);
}

static physaddr_t map_reverse_sandy(struct DRAMAddr addr, int flags, void *opts)
{
	const struct IntelCntrlOpts *o = (struct IntelCntrlOpts *)opts;
	physaddr_t ret = drammap_reverse_sandy(addr, o->geom);
	if (has_pcihole(o)) {
		ret = pcihole_remap_reverse(ret, o->pcibase, o->mem_top);
	}
	return ret;
}

static size_t twiddle_gran_sandy(struct DRAMAddr mask, int flags, void *opts)
{
	const struct IntelCntrlOpts *o = (struct IntelCntrlOpts *)opts;
	const int dchan = !!(o->geom & INTEL_DUALCHAN);
	const int ddimm = !!(o->geom & INTEL_DUALDIMM);
	const int drank = !!(o->geom & INTEL_DUALRANK);
	size_t base = 1 << MW_BITS;
	size_t ret;
	if ((ret = contiguous_twiddle(mask.col, base, 3))) return ret;
	if (dchan && mask.chan) return base << 3;
	if ((ret = contiguous_twiddle(mask.col, base + dchan, 0))) return ret;
	base <<= COL_BITS + dchan;
	if ((ret = contiguous_twiddle(mask.bank, base, 0))) return ret;
	base <<= 3;
	if (ddimm && mask.dimm) return ret;
	if (drank && mask.rank) return ret << ddimm;
	base <<= ddimm + drank;
	return contiguous_twiddle(mask.row, base, 0);
}

static struct DRAMAddr map_ivyhaswell(physaddr_t addr, int flags, void *opts)
{
	const struct IntelCntrlOpts *o = (struct IntelCntrlOpts *)opts;
	if (has_pcihole(o)) {
		addr = pcihole_remap(addr, o->pcibase, o->mem_top);
	}
	return drammap_ivyhaswell(addr, o->geom);
}

static physaddr_t map_reverse_ivyhaswell(struct DRAMAddr addr, int flags, void *opts)
{
	const struct IntelCntrlOpts *o = (struct IntelCntrlOpts *)opts;
	physaddr_t ret = drammap_reverse_ivyhaswell(addr, o->geom);
	if (has_pcihole(o)) {
		ret = pcihole_remap_reverse(ret, o->pcibase, o->mem_top);
	}
	return ret;
}

static size_t twiddle_gran_ivyhaswell(struct DRAMAddr mask, int flags, void *opts)
{
	const struct IntelCntrlOpts *o = (struct IntelCntrlOpts *)opts;
	const int dchan = !!(o->geom & INTEL_DUALCHAN);
	const int ddimm = !!(o->geom & INTEL_DUALDIMM);
	const int drank = !!(o->geom & INTEL_DUALRANK);
	size_t base = 1 << MW_BITS;
	size_t ret;
	if ((ret = contiguous_twiddle(mask.col, base, 4))) return ret;
	if (dchan && mask.chan) return base << 4;
	if ((ret = contiguous_twiddle(mask.col, base + dchan, 0))) return ret;
	base <<= COL_BITS + dchan;
	if ((ret = contiguous_twiddle(mask.bank, base, 2))) return ret;
	if (ddimm && mask.dimm) return base << 2;
	if (drank && mask.rank) return base << (2 + ddimm);
	if (BIT(2, mask.bank)) return base << (2 + ddimm + drank);
	base <<= 3 + ddimm + drank;
	return contiguous_twiddle(mask.row, base, 0);
}

void ramses_map_x86_intel_sandy(struct Mapping *m, struct IntelCntrlOpts *o)
{
	m->map = map_sandy;
	m->map_reverse = map_reverse_sandy;
	m->twiddle_gran = twiddle_gran_sandy;
	m->flags = 0;
	m->arg = o;
	m->props = (struct MappingProps){
		.granularity = (o->geom & INTEL_DUALCHAN) ? (1 << 6) : (1 << 13),
		.bank_cnt = 8,
		.col_cnt = 1 << COL_BITS,
		.cell_size = 1 << MW_BITS
	};
}

void ramses_map_x86_intel_ivyhaswell(struct Mapping *m, struct IntelCntrlOpts *o)
{
	m->map = map_ivyhaswell;
	m->map_reverse = map_reverse_ivyhaswell;
	m->twiddle_gran = twiddle_gran_ivyhaswell;
	m->flags = 0;
	m->arg = o;
	m->props = (struct MappingProps){
		.granularity = (o->geom & INTEL_DUALCHAN) ? (1 << 7) : (1 << 13),
		.bank_cnt = 8,
		.col_cnt = 1 << COL_BITS,
		.cell_size = 1 << MW_BITS
	};
}
