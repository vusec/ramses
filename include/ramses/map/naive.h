/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_MAP_NAIVE_H
#define RAMSES_MAP_NAIVE_H 1

#include <ramses/map.h>

enum DDRStandard {
	DDR3,
	DDR4
};

void ramses_map_naive(struct Mapping *m, enum DDRStandard ddr);

#endif /* naive.h */
