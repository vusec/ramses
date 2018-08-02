/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_MAP_X86_INTEL_H
#define RAMSES_MAP_X86_INTEL_H 1

#include <ramses/map.h>

#define INTEL_DUALRANK 1 /* 'Two ranks per dimm' */
#define INTEL_DUALDIMM 2 /* 'Two dimms per channel' */
#define INTEL_DUALCHAN 4 /* 'Two channels per controller' */

struct IntelCntrlOpts {
	physaddr_t pcibase;
	physaddr_t mem_top;
	int geom;
};

void ramses_map_x86_intel_sandy(struct Mapping *m, struct IntelCntrlOpts *o);
void ramses_map_x86_intel_ivyhaswell(struct Mapping *m, struct IntelCntrlOpts *o);

#endif /* intel.h */
