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

#include <ramses/translate/heuristic.h>

#include "bitops.h"


static physaddr_t heur_trans(uintptr_t addr, int cont_bits, union TranslateArg arg)
{
	return (addr & LS_BITMASK(cont_bits)) + arg.pa;
}

static size_t heur_range(uintptr_t addr, size_t npages, physaddr_t *out,
                         int cont_bits, union TranslateArg arg)
{
	for (size_t i = 0; i < npages; i++) {
		out[i] = arg.pa;
	}
	return npages;
}


void ramses_translate_heuristic(struct Translation *t,
                                int cont_bits, physaddr_t baseaddr)
{
	t->translate = heur_trans;
	t->translate_range = heur_range;
	t->page_shift = cont_bits;
	t->arg.pa = baseaddr;
}
