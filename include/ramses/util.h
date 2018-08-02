/*
 * Copyright (c) 2016 Andrei Tatar
 * Copyright (c) 2017-2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

/* Miscellaneous utility functions for DRAM addresses */

#ifndef RAMSES_UTIL_H
#define RAMSES_UTIL_H 1

#include <ramses/types.h>

#include <stdbool.h>

/* For use in printf()-like functions */
#define DRAMADDR_HEX_FMTSTR "(%1x %1x %1x %1x %4x %3x)"

/* DRAM organization level, from fine to coarse */
enum DRAMLevel {
	DRAM_ROW,
	DRAM_BANK,
	DRAM_RANK,
	DRAM_DIMM,
	DRAM_CHAN
};
/* Check whether two DRAM addresses are on the same DRAM level */
static inline bool ramses_dramaddr_same(enum DRAMLevel lvl,
                                        struct DRAMAddr a, struct DRAMAddr b)
{
	bool ret = true;
	switch (lvl) {
		case DRAM_ROW:  ret = ret && (a.row == b.row);
		case DRAM_BANK: ret = ret && (a.bank == b.bank);
		case DRAM_RANK: ret = ret && (a.rank == b.rank);
		case DRAM_DIMM: ret = ret && (a.dimm == b.dimm);
		case DRAM_CHAN: ret = ret && (a.chan == b.chan);
	}
	return ret;
}
/* qsort()-like comparison function for DRAM addresses */
static inline int ramses_dramaddr_cmp(struct DRAMAddr a, struct DRAMAddr b)
{
	int ch = a.chan - b.chan;
	int di = a.dimm - b.dimm;
	int ra = a.rank - b.rank;
	int ba = a.bank - b.bank;
	int rw = a.row - b.row;
	int cl = a.col - b.col;
	return !ch ? !di ? !ra ? !ba ? !rw ? cl : rw : ba : ra : di : ch;
}

#endif /* util.h */
