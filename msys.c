/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
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

#include <ramses/msys.h>

static size_t gcd(size_t a, size_t b)
{
	while (b) {
		size_t t = b;
		b = a % b;
		a = t;
	}
	return a;
}

size_t ramses_msys_granularity(struct MemorySystem *m, size_t pagesz)
{
	size_t gran = gcd(pagesz, m->mapping.props.granularity);
	for (size_t i = 0; i < m->nremaps; i++) {
		gran = gcd(gran, ramses_map_twiddle_gran(&m->mapping, m->remaps[i]->gran));
	}
	return gran;
}

struct DRAMAddr ramses_resolve(struct MemorySystem *m, physaddr_t addr)
{
	return ramses_remap_chain(m->remaps, m->nremaps,
	                          ramses_map(&m->mapping, addr));
}

physaddr_t ramses_resolve_reverse(struct MemorySystem *m, struct DRAMAddr addr)
{
	return ramses_map_reverse(&m->mapping,
		ramses_remap_chain_reverse(m->remaps, m->nremaps, addr)
	);
}
