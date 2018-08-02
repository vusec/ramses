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

#include <ramses/bufmap.h>

#include <ramses/binsearch.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define align_down(a,n) (((a) / (n)) * (n))


static int pte_pa_cmp(const void *a, const void *b)
{
	physaddr_t aa = ((struct PTE *)a)->pa;
	physaddr_t ba = ((struct PTE *)b)->pa;
	return (aa == ba) ? 0 : (aa < ba) ? -1 : 1;
}

static int bmsetup_ptes(struct PTE *ptes, size_t ptelen,
                        struct Translation *trans,
                        uintptr_t buf, void *tmpbuf)
{
	const bool fast_translate = tmpbuf != NULL;
	const size_t pagesz = ramses_translate_granularity(trans);
	physaddr_t *tb = (physaddr_t *)tmpbuf;

	assert(buf % pagesz == 0);
	if (fast_translate) {
		if (ramses_translate_range(trans, buf, ptelen, tb) != ptelen) {
			return 1;
		}
	}
	for (size_t i = 0; i < ptelen; i++) {
		uintptr_t va = buf + i * pagesz;
		ptes[i].va = va;
		if (fast_translate) {
			ptes[i].pa = tb[i];
		} else {
			ptes[i].pa = ramses_translate(trans, va);
		}
	}
	qsort(ptes, ptelen, sizeof(*ptes), pte_pa_cmp);
	return 0;
}

static int dramaddr_cmp(const void *a, const void *b)
{
	return ramses_dramaddr_cmp(*((struct DRAMAddr *)a), *((struct DRAMAddr *)b));
}

static inline size_t
dramaddr_rcdiff(struct DRAMAddr a, struct DRAMAddr b, struct MemorySystem *msys)
{
	return ((a.row - b.row) * msys->mapping.props.col_cnt + (a.col - b.col)) *
	       msys->mapping.props.cell_size;
}

static size_t bmsetup_ranges(struct DRAMRange **dram_ranges,
                             struct PTE *ptes, size_t ptelen, size_t pagesz,
                             size_t elen, struct MemorySystem *msys,
                             void *tmpbuf)
{
	const size_t ecnt = ptelen * (pagesz / elen);

	struct DRAMAddr *tmp = NULL;
	if (tmpbuf != NULL) {
		tmp = (struct DRAMAddr *)tmpbuf;
	} else {
		tmp = malloc(ecnt * sizeof(*tmp));
	}

	if (tmp != NULL) {
		size_t ei = 0;
		for (size_t page = 0; page < ptelen; page++) {
			for (size_t off = 0; off < pagesz; off += elen) {
				tmp[ei++] = ramses_resolve(msys, ptes[page].pa + off);
			}
		}
		assert(ei == ecnt);
		qsort(tmp, ecnt, sizeof(*tmp), dramaddr_cmp);

		size_t rangelen = 1;
		struct DRAMAddr last = tmp[0];
		for (size_t i = 1; i < ecnt; i++) {
			struct DRAMAddr cur = tmp[i];
			if (!ramses_dramaddr_same(DRAM_BANK, last, cur) ||
			    dramaddr_rcdiff(cur, last, msys) != elen)
			{
				rangelen++;
			}
			last = cur;
		}

		struct DRAMRange *ranges = malloc(rangelen * sizeof(*ranges));
		if (ranges != NULL) {
			size_t ri = 0;
			ranges[0].start = tmp[0];
			ranges[0].entry_cnt = 1;
			last = tmp[0];
			for (size_t i = 1; i < ecnt; i++) {
				struct DRAMAddr cur = tmp[i];
				if (!ramses_dramaddr_same(DRAM_BANK, last, cur) ||
				    dramaddr_rcdiff(cur, last, msys) != elen)
				{
					ri++;
					ranges[ri].start = cur;
					ranges[ri].entry_cnt = 1;
				} else {
					ranges[ri].entry_cnt++;
				}
				last = cur;
			}
			assert(ri == rangelen - 1);
			*dram_ranges = ranges;
		} else {
			rangelen = 0;
		}

		if (tmp != tmpbuf) {
			free(tmp);
		}
		return rangelen;
	} else {
		return 0;
	}
}

static inline size_t ceildiv(size_t a, size_t b)
{
	return (a / b) + !!(a % b);
}

int ramses_bufmap(struct BufferMap *bmap, void *buf, size_t len,
                  struct Translation *trans, struct MemorySystem *msys,
                  int flags)
{
	const size_t pagesz = ramses_translate_granularity(trans);
	const size_t ptelen = ceildiv(len, pagesz);
	struct PTE *ptes;
	struct DRAMRange *ranges = NULL;
	size_t rangelen;
	size_t elen;
	void *tmpbuf;

	elen = ramses_msys_granularity(msys, pagesz);
	if (flags & BUFMAP_NOCLOBBER) {
		tmpbuf = NULL;
	} else {
		tmpbuf = buf;
	}

	errno = 0;
	ptes = malloc(ptelen * sizeof(*ptes));
	if (ptes == NULL) {
		return 1;
	}

	if (ptelen * sizeof(physaddr_t) >= len) {
		tmpbuf = NULL;
	}
	if (bmsetup_ptes(ptes, ptelen, trans,
	                 align_down((uintptr_t)buf, pagesz), tmpbuf))
	{
		goto err_free_ptes;
	}

	if (ptelen * (pagesz / elen) * sizeof(struct DRAMAddr) >= len) {
		tmpbuf = NULL;
	}
	rangelen = bmsetup_ranges(&ranges, ptes, ptelen, pagesz, elen, msys, tmpbuf);
	if (!rangelen) {
		goto err_free;
	}

	if ((flags & BUFMAP_ZEROFILL) && !(flags & BUFMAP_NOCLOBBER)) {
		memset(buf, 0, len);
	}

	bmap->bufbase = buf;
	bmap->ptes = ptes;
	bmap->pte_cnt = ptelen;
	bmap->page_size = pagesz;
	bmap->ranges = ranges;
	bmap->range_cnt = rangelen;
	bmap->entry_len = elen;
	bmap->msys = msys;
	return 0;

	err_free:
		free(ranges);
	err_free_ptes:
		free(ptes);
		return 1;
}

void ramses_bufmap_free(struct BufferMap *bm)
{
	free(bm->ptes);
	free(bm->ranges);
}

struct DRAMAddr ramses_bufmap_addr(struct BufferMap *bm, size_t ri, size_t ei)
{
	if (ri >= bm->range_cnt || ei >= bm->ranges[ri].entry_cnt) {
		return RAMSES_BADDRAMADDR;
	}
	const size_t cell_off = (ei * bm->entry_len) / bm->msys->mapping.props.cell_size;
	struct DRAMAddr da = bm->ranges[ri].start;
	da.row += (da.col + cell_off) / bm->msys->mapping.props.col_cnt;
	da.col = (da.col + cell_off) % bm->msys->mapping.props.col_cnt;

	return da;
}

struct BMPos ramses_bufmap_next(struct BufferMap *bm, struct BMPos p,
                                enum DRAMLevel lvl)
{
	struct DRAMAddr ida = ramses_bufmap_addr(bm, p.ri, p.ei);
	size_t colents = ((bm->msys->mapping.props.col_cnt - ida.col) *
	                   bm->msys->mapping.props.cell_size
	                 ) / bm->entry_len;
	size_t ri = p.ri;
	size_t ei = p.ei;
	struct DRAMAddr da = ida;
	while (ramses_dramaddr_cmp(da, RAMSES_BADDRAMADDR) != 0 &&
	       ramses_dramaddr_same(lvl, ida, da))
	{
		if (lvl == DRAM_ROW) {
			size_t rements = bm->ranges[ri].entry_cnt - ei;
			if (rements > colents) {
				assert(colents);
				ei += colents;
				colents = 0;
			} else {
				colents -= rements;
				ri++;
				ei = 0;
			}
		} else {
			ri++;
			ei = 0;
		}
		da = ramses_bufmap_addr(bm, ri, ei);
	}
	return (struct BMPos){ .ri = ri, .ei = ei };
}

size_t ramses_bufmap_entrycnt(struct BufferMap *bm,
                              struct BMPos start, struct BMPos end)
{
	size_t ret = 0;
	size_t ri = start.ri;
	size_t ei = start.ei;
	while (ri < bm->range_cnt && ri < end.ri) {
		ret += bm->ranges[ri].entry_cnt - ei;
		ri++;
		ei = 0;
	}
	if (ri < bm->range_cnt) {
		ret += end.ei - ei;
	}
	return ret;
}

static int dramrange_start_cmp(const void *a, const void *b)
{
	return ramses_dramaddr_cmp(((struct DRAMRange *)a)->start, ((struct DRAMRange *)b)->start);
}

struct entryeval_arg {
	struct DRAMAddr addr;
	struct BufferMap *bm;
	size_t ri;
};

static int entry_eval(size_t ei, void *arg)
{
	struct entryeval_arg *a = (struct entryeval_arg *)arg;
	struct DRAMAddr estart = ramses_bufmap_addr(a->bm, a->ri, ei);
	int r = ramses_dramaddr_cmp(a->addr, estart);
	if (r > 0) {
		size_t diff = (a->addr.col - estart.col) * a->bm->msys->mapping.props.cell_size;
		if (diff < a->bm->entry_len) {
			return 0;
		}
	}
	return r;
}

int ramses_bufmap_find(struct BufferMap *bm, struct DRAMAddr addr, struct BMPos *pos)
{
	bool found;
	size_t ri = 0;
	size_t ei = 0;
	struct DRAMRange item = { .start = addr, .entry_cnt = 0 };
	found = binsearch(&item, bm->ranges, bm->range_cnt, sizeof(*(bm->ranges)),
	                  dramrange_start_cmp, &ri);
	assert(ri < bm->range_cnt);
	if (!found) {
		struct entryeval_arg earg = {
			.addr = addr,
			.bm = bm,
			.ri = ri
		};
		found = binsearch_idx(bm->ranges[ri].entry_cnt, entry_eval, &earg, &ei);
		assert(ei < bm->ranges[ri].entry_cnt);
	}
	if (pos != NULL && found) {
		pos->ri = ri;
		pos->ei = ei;
	}
	return found ? 0 : 1;
}

struct samelvl_eval_arg {
	struct DRAMAddr addr;
	struct BufferMap *bm;
	size_t ri;
	enum DRAMLevel lvl;
};

static int samelvl_range_eval(size_t ri, void *arg)
{
	struct samelvl_eval_arg *a = (struct samelvl_eval_arg *)arg;
	struct DRAMAddr da = a->addr;
	struct DRAMAddr db = ramses_bufmap_addr(a->bm, ri, 0);
	return ramses_dramaddr_same(a->lvl, da, db) ? 0 : ramses_dramaddr_cmp(da, db);
}

static int samelvl_entry_eval(size_t ei, void *arg)
{
	struct samelvl_eval_arg *a = (struct samelvl_eval_arg *)arg;
	struct DRAMAddr da = a->addr;
	struct DRAMAddr db = ramses_bufmap_addr(a->bm, a->ri, ei);
	return ramses_dramaddr_same(a->lvl, da, db) ? 0 : ramses_dramaddr_cmp(da, db);
}

int ramses_bufmap_find_same(struct BufferMap *bm, struct DRAMAddr a,
                            enum DRAMLevel lvl, struct BMPos *pos)
{
	bool found;
	size_t ri = 0;
	size_t ei = 0;
	struct samelvl_eval_arg earg = { .addr = a, .bm = bm, .ri = 0, .lvl = lvl };
	found = binsearch_idx(bm->range_cnt, samelvl_range_eval, &earg, &ri);
	assert(ri < bm->range_cnt);
	if (!found && lvl == DRAM_ROW) {
		earg.ri = ri;
		found = binsearch_idx(bm->ranges[ri].entry_cnt, samelvl_entry_eval,
		                      &earg, &ei);
		assert(ei < bm->ranges[ri].entry_cnt);
	}

	if (pos != NULL && found) {
		pos->ri = ri;
		pos->ei = ei;
	}
	return found ? 0 : 1;
}

int ramses_bufmap_find_pte(struct BufferMap *bm, physaddr_t pa, size_t *ptepos)
{
	bool found;
	size_t pos;
	struct PTE pframe = { .pa = align_down(pa, bm->page_size), .va = 0 };
	found = binsearch(&pframe, bm->ptes, bm->pte_cnt, sizeof(*(bm->ptes)),
	                  pte_pa_cmp, &pos);
	if (found) *ptepos = pos;
	return found ? 0 : 1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static void bm_ptepos(struct BufferMap *bm, physaddr_t pa, size_t *pos)
{
	int r = ramses_bufmap_find_pte(bm, pa, pos);
	assert(!r);
}
#pragma GCC diagnostic pop

int ramses_bufmap_get_entry(struct BufferMap *bm, struct BMPos bp,
                            struct AddrEntry *entry)
{
	struct DRAMAddr da = ramses_bufmap_addr(bm, bp.ri, bp.ei);
	if (ramses_dramaddr_cmp(da, RAMSES_BADDRAMADDR) == 0) {
		return 1;
	}
	physaddr_t pa = ramses_resolve_reverse(bm->msys, da);
	size_t ptepos;
	bm_ptepos(bm, pa, &ptepos);

	entry->dramaddr = da;
	entry->virtp = bm->ptes[ptepos].va + (pa % bm->page_size);
	#ifdef ADDR_DEBUG
	entry->physaddr = pa;
	#endif
	return 0;
}

size_t ramses_bufmap_get_entries(struct BufferMap *bm,
                                 struct BMPos start, struct BMPos end,
                                 struct AddrEntry *entries, size_t maxents)
{
	size_t enti = 0;
	struct BMPos p = { .ri = start.ri, .ei = start.ei };
	struct DRAMAddr da = ramses_bufmap_addr(bm, p.ri, p.ei);

	while (enti < maxents &&
	       ramses_dramaddr_cmp(da, RAMSES_BADDRAMADDR) != 0 &&
	       (p.ri < end.ri || (p.ri == end.ri && p.ei < end.ei)))
	{
		physaddr_t pa = ramses_resolve_reverse(bm->msys, da);
		size_t ptepos;
		bm_ptepos(bm, pa, &ptepos);

		entries[enti].dramaddr = da;
		entries[enti].virtp = bm->ptes[ptepos].va + (pa % bm->page_size);
		#ifdef ADDR_DEBUG
		entries[enti].physaddr = pa;
		#endif

		enti++;
		p = ramses_bufmap_nextpos(bm, p);
		da = ramses_bufmap_addr(bm, p.ri, p.ei);
	}
	return enti;
}
