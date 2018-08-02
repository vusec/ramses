/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_MSYS_H
#define RAMSES_MSYS_H 1

#include <ramses/map.h>
#include <ramses/remap.h>

struct MemorySystem {
	struct Mapping mapping;
	size_t nremaps;
	struct Remapping **remaps;
	size_t nallocs;
	void **allocs;
};

size_t ramses_msys_granularity(struct MemorySystem *m, size_t pagesz);

struct DRAMAddr ramses_resolve(struct MemorySystem *m, physaddr_t addr);

physaddr_t ramses_resolve_reverse(struct MemorySystem *m, struct DRAMAddr addr);

int ramses_msys_load(const char *str, struct MemorySystem *m, size_t *erridx);
const char *ramses_msys_load_strerr(int err);

void ramses_msys_free(struct MemorySystem *m);

#endif /* msys.h */
