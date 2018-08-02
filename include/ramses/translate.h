/*
 * Copyright (c) 2017-2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

/* Virtual Address Translation */

#ifndef RAMSES_TRANSLATE_H
#define RAMSES_TRANSLATE_H 1

#include <ramses/types.h>

#include <stddef.h>

/* Union argument to pass either a pointer, physical address or an int */
union TranslateArg {
	void *p;
	physaddr_t pa;
	int val;
};

/*
 * Translates virtual address `addr' into physical address.
 * Returns RAMSES_BADADDR and sets errno if errors occur.
 * Sets errno to ENODATA if requested address is not mapped to memory.
 */
typedef physaddr_t (*ramses_translate_fn_t)(uintptr_t addr,
                                            int, union TranslateArg);
/*
 * Translates a virtually contiguous range of memory starting at `addr'
 * (truncated to granularity alignment) spanning `npages' entries.
 * Stores the results in `*out'.
 * Returns the number of page entries translated.
 * An entry not mapped in memory is translated to RAMSES_BADADDR.
 */
typedef size_t (*ramses_translate_range_fn_t)
(uintptr_t addr, size_t npages, physaddr_t *out, int, union TranslateArg);

struct Translation {
	ramses_translate_fn_t translate;
	ramses_translate_range_fn_t translate_range;
	int page_shift;
	union TranslateArg arg;
};


static inline physaddr_t
ramses_translate(struct Translation *t, uintptr_t addr)
{
	return t->translate(addr, t->page_shift, t->arg);
}

static inline size_t
ramses_translate_range(struct Translation *t, uintptr_t addr, size_t npages, physaddr_t *out)
{
	return t->translate_range(addr, npages, out, t->page_shift, t->arg);
}

/* Return the virt <-> phys address mapping granularity, in bytes */
static inline size_t ramses_translate_granularity(struct Translation *t)
{
	return 1ULL << t->page_shift;
}

#endif /* translate.h */
