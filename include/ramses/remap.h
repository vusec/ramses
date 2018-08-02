/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_REMAP_H
#define RAMSES_REMAP_H 1

#include <ramses/types.h>

#include <stddef.h>

union RemapArg {
	void *p;
	int val[2];
};

typedef struct DRAMAddr (*ramses_remap_fn_t)(struct DRAMAddr, union RemapArg);

struct Remapping {
	ramses_remap_fn_t remap;
	ramses_remap_fn_t remap_reverse;
	union RemapArg arg;
	struct DRAMAddr gran;
};

static inline struct DRAMAddr
ramses_remap(struct Remapping *r, struct DRAMAddr addr)
{
	return r->remap(addr, r->arg);
}

static inline struct DRAMAddr
ramses_remap_reverse(struct Remapping *r, struct DRAMAddr addr)
{
	return r->remap_reverse(addr, r->arg);
}

static inline struct DRAMAddr
ramses_remap_chain(struct Remapping *r[], size_t nremaps, struct DRAMAddr addr)
{
	for (size_t i = 0; i < nremaps; i++) {
		addr = r[i]->remap(addr, r[i]->arg);
	}
	return addr;
}

static inline struct DRAMAddr
ramses_remap_chain_reverse(struct Remapping *r[], size_t nremaps, struct DRAMAddr addr)
{
	for (size_t i = nremaps; i --> 0;) {
		addr = r[i]->remap_reverse(addr, r[i]->arg);
	}
	return addr;
}


extern struct Remapping RAMSES_REMAP_RANKMIRROR_DDR3;
extern struct Remapping RAMSES_REMAP_RANKMIRROR_DDR4;

void ramses_remap_rasxor(struct Remapping *r, int bit, int xormask);

#endif /* remap.h */
