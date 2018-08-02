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

#include <ramses/remap.h>

#include "bitops.h"

/* DDR rank mirroring */

static struct DRAMAddr rkmirror_ddr3(struct DRAMAddr addr, union RemapArg ign)
{
	struct DRAMAddr ret = addr;
	if (addr.rank) {
		/* Switch address bits 3<->4 5<->6 7<->8 */
		ret.row &= 0xfe07;
		ret.row |= (BIT(7, addr.row) << 8) | (BIT(8, addr.row) << 7) |
		           (BIT(5, addr.row) << 6) | (BIT(6, addr.row) << 5) |
		           (BIT(3, addr.row) << 4) | (BIT(4, addr.row) << 3);
		ret.col &= 0xfe07;
		ret.col |= (BIT(7, addr.col) << 8) | (BIT(8, addr.col) << 7) |
		           (BIT(5, addr.col) << 6) | (BIT(6, addr.col) << 5) |
		           (BIT(3, addr.col) << 4) | (BIT(4, addr.col) << 3);
		/* Switch bank bits 0<->1 */
		ret.bank &= 0xfffc;
		ret.bank |= (BIT(0, addr.bank) << 1) | BIT(1, addr.bank);
	}
	return ret;
}

static struct DRAMAddr rkmirror_ddr4(struct DRAMAddr addr, union RemapArg ign)
{
	struct DRAMAddr ret = addr;
	if (addr.rank) {
	/* Switch address bits 3<->4 5<->6 7<->8 11<->13 */
		ret.row &= 0xd607;
		ret.row |= (BIT(11, addr.row) << 13) | (BIT(13, addr.row) << 11) |
		           (BIT(7, addr.row) << 8)   | (BIT(8, addr.row) << 7)   |
		           (BIT(5, addr.row) << 6)   | (BIT(6, addr.row) << 5)   |
		           (BIT(3, addr.row) << 4)   | (BIT(4, addr.row) << 3);
		ret.col &= 0xd607;
		ret.col |= (BIT(11, addr.col) << 13) | (BIT(13, addr.col) << 11) |
		           (BIT(7, addr.col) << 8)   | (BIT(8, addr.col) << 7)   |
		           (BIT(5, addr.col) << 6)   | (BIT(6, addr.col) << 5)   |
		           (BIT(3, addr.col) << 4)   | (BIT(4, addr.col) << 3);
		/* Switch BA0<->BA1 BG0<->BG1 */
		ret.bank &= 0xfff0;
		ret.bank |= (BIT(2, addr.bank) << 3) | (BIT(3, addr.bank) << 2) |
		            (BIT(0, addr.bank) << 1) | BIT(1, addr.bank);
	}
	return ret;
}

/* RAS address XOR */

static struct DRAMAddr rasxor(struct DRAMAddr addr, union RemapArg arg)
{
	if (BIT(arg.val[0], addr.row)) {
		addr.row ^= arg.val[1];
	}
	return addr;
}


struct Remapping RAMSES_REMAP_RANKMIRROR_DDR3 = {
	.remap = rkmirror_ddr3,
	.remap_reverse = rkmirror_ddr3,
	.arg = {.p = NULL},
	.gran = {0, 0, 0, 3, 0x1f8, 0x1f8}
};

struct Remapping RAMSES_REMAP_RANKMIRROR_DDR4 = {
	.remap = rkmirror_ddr4,
	.remap_reverse = rkmirror_ddr4,
	.arg = {.p = NULL},
	.gran = {0, 0, 0, 0xf, 0x29f8, 0x29f8}
};

void ramses_remap_rasxor(struct Remapping *r, int bit, int xormask)
{
	xormask &= LS_BITMASK(16);
	r->remap = rasxor;
	r->remap_reverse = rasxor;
	r->arg.val[0] = bit;
	r->arg.val[1] = xormask;
	r->gran = (struct DRAMAddr){0, 0, 0, 0, xormask, 0};
}


#include "remap_msys.h"

static const struct MSYSParam RKMIRROR_PARAMS[] = {
	{.name = "ddr3:ddr4", .type = 'p'},
};

int rkmirror_config(struct Remapping **premap, union MSYSArg *args,
                    void **allocs, size_t *nallocs)
{
	switch (args[0].flag) {
		case 0:
			*premap = &RAMSES_REMAP_RANKMIRROR_DDR3;
			break;
		case 1:
			*premap = &RAMSES_REMAP_RANKMIRROR_DDR4;
			break;
		default:
			return -1;
	}
	*nallocs = 0;
	return 0;
}

static const struct MSYSParam RASXOR_PARAMS[] = {
	{.name = "bit", .type = 'i'},
	{.name = "mask", .type = 'i'},
};

int rasxor_config(struct Remapping **premap, union MSYSArg *args,
                  void **allocs, size_t *nallocs)
{
	long long bit = args[0].num;
	long long mask = args[1].num;
	if (bit < 0 || mask < 0 || bit >= 16 || mask > 0xffff) {
		return -1;
	}
	ramses_remap_rasxor(*premap, (int)bit, (int)mask);
	*nallocs = 0;
	return 0;
}

const struct RemapConfig REMAP_RKMIRROR_CONFIG = {
	.meta = {
		.name = "rankmirror",
		.params = RKMIRROR_PARAMS,
		.nparams = 1
	},
	.func = rkmirror_config
};
const struct RemapConfig REMAP_RASXOR_CONFIG = {
	.meta = {
		.name = "rasxor",
		.params = RASXOR_PARAMS,
		.nparams = 2
	},
	.func = rasxor_config,
};
