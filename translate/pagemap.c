/*
 * Copyright (c) 2017-2018 Vrije Universiteit Amsterdam
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
#define _XOPEN_SOURCE 500

#include <ramses/translate/pagemap.h>

#include "bitops.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


static int size2shift(size_t size)
{
	int r = 0;
	size >>= 1;
	while (size) { r++; size >>= 1; }
	return r;
}

static physaddr_t pagemap_trans(uintptr_t addr, int page_shift, union TranslateArg arg)
{
	uint64_t pagemap_entry;
	off_t pagemap_off = (addr >> page_shift) * sizeof(pagemap_entry);
	if (pread(arg.val, &pagemap_entry, sizeof(pagemap_entry), pagemap_off) != sizeof(pagemap_entry)) {
		return RAMSES_BADADDR;
	}
	/* Sanity check that page is in memory */
	if (!BIT(63, pagemap_entry)) {
		errno = ENODATA;
		return RAMSES_BADADDR;
	}
	return ((pagemap_entry & LS_BITMASK(55)) << page_shift) +
			(addr & LS_BITMASK(page_shift));
}

static size_t pagemap_range(uintptr_t addr, size_t npages, physaddr_t *out,
                            int page_shift, union TranslateArg arg)
{
	uint64_t *pagemap_buf = malloc(npages * sizeof(*pagemap_buf));
	off_t pagemap_off = (addr >> page_shift) * sizeof(*pagemap_buf);
	if (pagemap_buf != NULL) {
		size_t read_ptes = pread(arg.val,
		                         pagemap_buf,
		                         npages * sizeof(*pagemap_buf),
		                         pagemap_off)
		                   / sizeof(*pagemap_buf);
		for (size_t i = 0; i < read_ptes; i++) {
			if (BIT(63, pagemap_buf[i])) {
				out[i] = ((pagemap_buf[i] & LS_BITMASK(55)) << page_shift);
			} else {
				out[i] = RAMSES_BADADDR;
			}
		}
		free(pagemap_buf);
		return read_ptes;
	} else {
		return 0;
	}
}


void ramses_translate_pagemap(struct Translation *t, int pagemap_fd)
{
	t->translate = pagemap_trans;
	t->translate_range = pagemap_range;
	t->page_shift = size2shift(sysconf(_SC_PAGESIZE));
	t->arg.val = pagemap_fd;
}
