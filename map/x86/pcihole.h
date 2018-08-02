/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_MAP_X86_PCIHOLE_H
#define RAMSES_MAP_X86_PCIHOLE_H 1

#include <ramses/types.h>

#include <stdint.h>

physaddr_t pcihole_remap(physaddr_t addr, physaddr_t pcibase, physaddr_t tom);
physaddr_t pcihole_remap_reverse(physaddr_t addr, physaddr_t pcibase, physaddr_t tom);

physaddr_t pcihole_offset(physaddr_t addr, physaddr_t holebase, uint32_t holeoffset);
physaddr_t pcihole_offset_reverse(physaddr_t addr, physaddr_t holebase, uint32_t holeoffset);

#endif /* pcihole.h */
