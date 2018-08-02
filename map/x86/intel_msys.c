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

#include "intel_msys.h"

#include <ramses/map/x86/intel.h>

#include <stdlib.h>


static const struct MSYSParam MAP_INTEL_PARAMS[] = {
	{.name = "sandy:ivyhaswell", .type = 'p'},
	{.name = "2chan", .type = 'f'},
	{.name = "2dimm", .type = 'f'},
	{.name = "2rank", .type = 'f'},
	{.name = "pcibase", .type = 'i'},
	{.name = "tom", .type = 'i'},
};

static int intel_config(struct Mapping *m, union MSYSArg *args,
                        void **allocs, size_t *nallocs)
{
	struct IntelCntrlOpts *opts = calloc(1, sizeof(*opts));
	if (!opts) {
		return 1;
	}
	if (args[1].flag) opts->geom |= INTEL_DUALCHAN;
	if (args[2].flag) opts->geom |= INTEL_DUALDIMM;
	if (args[3].flag) opts->geom |= INTEL_DUALRANK;
	opts->pcibase = args[4].num;
	opts->mem_top = args[5].num;

	switch (args[0].flag) {
		case 0:
			ramses_map_x86_intel_sandy(m, opts);
			break;
		case 1:
			ramses_map_x86_intel_ivyhaswell(m, opts);
			break;
		default:
			free(opts);
			return 2;
	}
	*allocs = opts;
	*nallocs = 1;
	return 0;
}

const struct MapConfig MAP_INTEL_CONFIG = {
	.meta = {
		.name = "intel",
		.params = MAP_INTEL_PARAMS,
		.nparams = 6
	},
	.func = intel_config
};
