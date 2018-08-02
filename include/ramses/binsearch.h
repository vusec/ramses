/*
 * Copyright (c) 2017-2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_BINSEARCH_H
#define RAMSES_BINSEARCH_H 1

#include <stddef.h>
#include <stdbool.h>

typedef int (*eval_fn_t)(size_t, void *);
typedef int (*compar_fn_t)(const void *, const void *);

/*
 * Search for `item' in the sorted array `base', which contains `nmemb' entries
 * of size `size', using `compar' as comparison function.
 * Works similarly to the standard library's qsort function.
 * Returns true if found, false otherwise; sets *pos to the offset of the
 * element, if found, or the offset of the greatest element less than `item',
 * otherwise.
 * If `pos' is NULL, it will be ignored.
 */
bool binsearch(const void *item, const void *base, size_t nmemb, size_t size,
               compar_fn_t compar, size_t *pos);

/*
 * Perform a binary search on the interval [0, maxidx) using evalfn to guide
 * the search. evalfn's return value is analogous to a comparison functions'.
 */
bool binsearch_idx(size_t maxidx, eval_fn_t evalfn, void *evalarg, size_t *pos);

#endif /* binsearch.h */
