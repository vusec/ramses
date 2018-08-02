/*
 * Copyright (c) 2016 Andrei Tatar
 * Copyright (c) 2017-2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef _HAMTIME_RAMSES_BITOPS_H
#define _HAMTIME_RAMSES_BITOPS_H 1

#define LS_BITMASK(n) ((1ULL << (n)) - 1)
#define BIT(n,x) (((x) >> (n)) & 1)
#define POP_BIT(n,x) (((x) & LS_BITMASK(n)) + (((x) >> ((n)+1)) << (n)))

static inline int leastsetbit(long long v)
{
	if (v) {
		int ret;
		v = (v ^ (v-1)) >> 1;
		for (ret = 0; v; ret++) {
			v >>= 1;
		}
		return ret;
	} else {
		return -1;
	}
}

#endif /* bitops.h */
