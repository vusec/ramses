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

#ifndef RAMSES_BUFMAP_H
#define RAMSES_BUFMAP_H 1

#include <ramses/types.h>
#include <ramses/translate.h>
#include <ramses/msys.h>
#include <ramses/util.h>

#include <stddef.h>
#include <stdint.h>

/* Page Table Entry */
struct PTE {
	physaddr_t pa;
	uintptr_t va;
};
/* Range of contiguous DRAM area */
struct DRAMRange {
	struct DRAMAddr start;
	size_t entry_cnt;
};
/*
 * Structure maintaining a mapping between a buffer in virtual memory and the
 * addresses it maps to in DRAM address space.
 */
struct BufferMap {
	void *bufbase; /* Pointer to VM buffer */
	struct PTE *ptes; /* virt<->phys mapping of the buffer */
	size_t pte_cnt;
	size_t page_size;
	struct DRAMRange *ranges; /* DRAM ranges spanned by the buffer, in order */
	size_t range_cnt;
	size_t entry_len; /* Max memory size contiguous in both phys and DRAM address spaces */
	struct MemorySystem *msys;
};
/* virt<->DRAM address mapping for a particular entry */
struct AddrEntry {
	uintptr_t virtp;
	struct DRAMAddr dramaddr;
#ifdef ADDR_DEBUG
	physaddr_t physaddr;
#endif
};
/* Coordinate for a specific entry in a BufferMap */
struct BMPos {
	size_t ri; /* range index */
	size_t ei; /* entry index */
};

#define BUFMAP_NOCLOBBER	1 /* Do NOT use the buffer for scratch data */
#define BUFMAP_ZEROFILL 	2 /* Zero out buffer after using for scratch data */
/*
 * Set up a BufferMap structure for a buffer `buf' of size `len', using `trans'
 * for virtual->physical address translation, and `msys' to describe the memory
 * system in use.
 * Possible flags are BUFMAP_NOCLOBBER and BUFMAP_ZEROFILL.
 */
int ramses_bufmap(struct BufferMap *bm, void *buf, size_t len,
                  struct Translation *trans, struct MemorySystem *msys,
                  int flags);
/* Free BufferMap data structures allocated by ramses_bufmap */
void ramses_bufmap_free(struct BufferMap *bm);

/* Compute the DRAM address of an entry in a BufferMap */
struct DRAMAddr ramses_bufmap_addr(struct BufferMap *bm, size_t ri, size_t ei);
/* Compute the position of the next DRAM level boundary following `p' */
struct BMPos ramses_bufmap_next(struct BufferMap *bm, struct BMPos p,
                                enum DRAMLevel lvl);
/*
 * Compute the number of entries in BufferMap `bm' from `start' up to,
 * not including, `end'.
 */
size_t ramses_bufmap_entrycnt(struct BufferMap *bm,
                              struct BMPos start, struct BMPos end);

/*
 * Find DRAM address `addr' in BufferMap `bm'.
 * If successful, returns 0 and sets *pos to the position of the found item.
 * Else, returns 1 and *pos is untouched.
 * `pos', if NULL will be ignored.
 */
int ramses_bufmap_find(struct BufferMap *bm, struct DRAMAddr addr, struct BMPos *pos);
/*
 * Find the appropriate PTE index within BufferMap `bm' for phys address `pa'.
 * If successful, returns 0  and sets *ptepos to the appropriate index.
 * Else, returns 1 and *ptepos is untouched.
 * `pos', if NULL will be ignored.
 */
int ramses_bufmap_find_pte(struct BufferMap *bm, physaddr_t pa, size_t *ptepos);
/*
 * Find an entry with DRAM address on the same DRAM level `lvl' as `addr'.
 * If successful, returns 0 and sets *pos to the position of the found item.
 * Else, returns 1 and *pos is untouched.
 * `pos', if NULL will be ignored.
 */
int ramses_bufmap_find_same(struct BufferMap *bm, struct DRAMAddr addr,
                            enum DRAMLevel lvl, struct BMPos *pos);

/*
 * Write out an AddrEntry corresponding to the memory area of size
 * `bm->entry_len' at position `bp' in BufferMap `bm'.
 */
int ramses_bufmap_get_entry(struct BufferMap *bm, struct BMPos bp,
                            struct AddrEntry *entry);
/*
 * Write out into `*entries' up to `maxents' entries from BufferMap `bm',
 * starting at `start' up to (not including) `end'.
 * Returns the number of entries written.
 */
size_t ramses_bufmap_get_entries(struct BufferMap *bm,
                                 struct BMPos start, struct BMPos end,
                                 struct AddrEntry *entries, size_t maxents);

/* Row length, in bytes, of a BufferMap */
static inline size_t ramses_bufmap_rowlen(struct BufferMap *bm)
{
	return bm->msys->mapping.props.col_cnt *
	       bm->msys->mapping.props.cell_size;
}
/* Entries per row */
static inline size_t ramses_bufmap_epr(struct BufferMap *bm)
{
	return ramses_bufmap_rowlen(bm) / bm->entry_len;
}

/* Next and prev iteration functions for positions in a BufferMap */
static inline
struct BMPos ramses_bufmap_nextpos(struct BufferMap *bm, struct BMPos p)
{
	size_t nei = p.ei + 1;
	if (p.ri >= bm->range_cnt) {
		return (struct BMPos){ .ri = bm->range_cnt, .ei = 0 };
	} else if (nei >= bm->ranges[p.ri].entry_cnt) {
		return (struct BMPos){ .ri = p.ri + 1, .ei = 0 };
	} else {
		return (struct BMPos){ .ri = p.ri, .ei = nei };
	}
}
static inline
struct BMPos ramses_bufmap_prevpos(struct BufferMap *bm, struct BMPos p)
{
	if (p.ri || p.ei) {
		if (p.ei) {
			return (struct BMPos){ .ri = p.ri, .ei = p.ei - 1 };
		} else {
			return (struct BMPos){ .ri = p.ri - 1,
				                   .ei = bm->ranges[p.ri - 1].entry_cnt - 1 };
		}
	} else {
		return (struct BMPos){ .ri = 0, .ei = 0 };
	}
}

#endif /* bufmap.h */
