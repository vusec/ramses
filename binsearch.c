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

#include <ramses/binsearch.h>

#include <stdint.h>

bool binsearch_idx(size_t maxidx, int (*evalfn)(size_t, void *),
                  void *evalarg, size_t *pos)
{
	size_t p = 0;
	size_t left = maxidx / 2;
	size_t right = maxidx / 2 + (maxidx % 2);
	while (right) {
		size_t idx = p + left;
		int r = evalfn(idx, evalarg);
		if (r == 0) {
			/* Found */
			if (pos != NULL) {
				*pos = idx;
			}
			return true;
		} else if (r > 0) {
			p = idx;
			left = right / 2;
			right = (right > 1) ? right / 2 + (right % 2) : 0;
		} else {
			right = (left / 2) + (left % 2);
			left = left / 2;
		}
	}
	/* Not found */
	if (pos != NULL) {
		*pos = p;
	}
	return false;
}

struct arrayidx_arg {
	const void *item;
	const void *base;
	compar_fn_t compar;
	size_t memb_size;
};

static int arrayidx_eval(size_t idx, void *arg)
{
	struct arrayidx_arg *a = (struct arrayidx_arg *)arg;
	return a->compar(a->item,
	                 (const void *)((uintptr_t)(a->base) + a->memb_size * idx));
}

bool binsearch(const void *item, const void *base, size_t nmemb, size_t size,
              compar_fn_t compar, size_t *pos)
{
	struct arrayidx_arg arg = {
		.item = item,
		.base = base,
		.compar = compar,
		.memb_size = size
	};
	return binsearch_idx(nmemb, arrayidx_eval, &arg, pos);
}
