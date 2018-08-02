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

#include <ramses/map/naive.h>

#include <assert.h>

#include "bitops.h"

#define MW_BITS 3
#define COL_BITS 10
#define ROW_BITS 16
#define BANK_OFF (MW_BITS + COL_BITS)


static inline int bankbits(enum DDRStandard ddr)
{
	switch(ddr) {
		case DDR3: return 3;
		case DDR4: return 4;
		default: return 0;
	}
}

static inline int rowoff(enum DDRStandard ddr)
{
	return BANK_OFF + bankbits(ddr);
}

static struct DRAMAddr map_naive(physaddr_t addr, int flags, void *arg)
{
	int bbits = bankbits((enum DDRStandard)flags);
	int row_off = rowoff((enum DDRStandard)flags);
	if (!(bbits && row_off)) {
		return RAMSES_BADDRAMADDR;
	}
	/* Sanity check that address fits in memory geometry */
	assert(!(addr >> (row_off + ROW_BITS)));
	return (struct DRAMAddr){
		.chan = 0,
		.dimm = 0,
		.rank = 0,
		.col = (addr >> MW_BITS) & LS_BITMASK(COL_BITS),
		.bank = (addr >> BANK_OFF) & LS_BITMASK(bbits),
		.row = (addr >> row_off) & LS_BITMASK(ROW_BITS),
	};
}

static physaddr_t map_reverse_naive(struct DRAMAddr addr, int flags, void *arg)
{
	int row_off = rowoff((enum DDRStandard)flags);
	if (!row_off) {
		return RAMSES_BADADDR;
	}
	return ((physaddr_t)addr.row << row_off) +
	       ((physaddr_t)addr.bank << BANK_OFF) +
	       ((physaddr_t)addr.col << MW_BITS);
}

static size_t twiddle_gran_naive(struct DRAMAddr mask, int flags, void *arg)
{
	int bbits = bankbits((enum DDRStandard)flags);
	if (bbits) {
		size_t ret = 1 << MW_BITS;
		if (mask.col) {
			return ret << leastsetbit(mask.col);
		}
		ret <<= COL_BITS;
		if (mask.bank) {
			return ret << leastsetbit(mask.bank);
		}
		ret <<= bbits;
		if (mask.row) {
			return ret << leastsetbit(mask.row);
		}
	}
	return 0;
}


void ramses_map_naive(struct Mapping *m, enum DDRStandard ddr)
{
	m->map = map_naive;
	m->map_reverse = map_reverse_naive;
	m->twiddle_gran = twiddle_gran_naive;
	m->flags = (int)ddr;
	m->arg = NULL;
	m->props = (struct MappingProps){
		.granularity = 1ULL << BANK_OFF,
		.bank_cnt = 1 << bankbits(ddr),
		.col_cnt = 1 << COL_BITS,
		.cell_size = 1 << MW_BITS,
	};
}

#include "naive_msys.h"

static const struct MSYSParam MAP_NAIVE_PARAMS[] = {
	{.name = "ddr3:ddr4", .type = 'p'},
};

static int naive_config(struct Mapping *m, union MSYSArg *args,
                        void **allocs, size_t *nallocs)
{
	ramses_map_naive(m, args[0].flag ? DDR4 : DDR3);
	*nallocs = 0;
	return 0;
}

const struct MapConfig MAP_NAIVE_CONFIG = {
	.meta = {
		.name = "naive",
		.params = MAP_NAIVE_PARAMS,
		.nparams = 1
	},
	.func = naive_config
};
