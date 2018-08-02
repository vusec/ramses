/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_MAP_H
#define RAMSES_MAP_H 1

#include <ramses/types.h>

#include <stddef.h>

typedef struct DRAMAddr (*ramses_map_fn_t)(physaddr_t, int, void *);
typedef physaddr_t (*ramses_map_reverse_fn_t)(struct DRAMAddr, int, void *);
typedef size_t (*ramses_map_twiddle_gran_fn_t)(struct DRAMAddr, int, void *);

struct MappingProps {
	physaddr_t granularity;
	unsigned int bank_cnt;
	unsigned int col_cnt;
	unsigned int cell_size;
};

struct Mapping {
	ramses_map_fn_t map;
	ramses_map_reverse_fn_t map_reverse;
	ramses_map_twiddle_gran_fn_t twiddle_gran;
	int flags;
	void *arg;
	struct MappingProps props;
};


static inline struct DRAMAddr ramses_map(struct Mapping *m, physaddr_t addr)
{
	return m->map(addr, m->flags, m->arg);
}

static inline physaddr_t ramses_map_reverse(struct Mapping *m, struct DRAMAddr addr)
{
	return m->map_reverse(addr, m->flags, m->arg);
}

static inline size_t ramses_map_twiddle_gran(struct Mapping *m, struct DRAMAddr mask)
{
	return m->twiddle_gran(mask, m->flags, m->arg);
}

#endif /* map.h */
