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
#include "msys_int.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* Mapping configs */
#include "map/naive_msys.h"
#include "map/x86/intel_msys.h"

static const struct MapConfig *MAP_CONFIGS[] = {
	&MAP_NAIVE_CONFIG,
	&MAP_INTEL_CONFIG
};
static const size_t
MAP_CONFIGS_LEN = sizeof(MAP_CONFIGS) / sizeof(*MAP_CONFIGS);

/* Remapping configs */
#include "remap_msys.h"

static const struct RemapConfig *REMAP_CONFIGS[] = {
	&REMAP_RKMIRROR_CONFIG,
	&REMAP_RASXOR_CONFIG
};
static const size_t
REMAP_CONFIGS_LEN = sizeof(REMAP_CONFIGS) / sizeof(*REMAP_CONFIGS);


static const struct MapConfig *find_mapcfg(const char *name)
{
	for (int i = 0; i < MAP_CONFIGS_LEN; i++) {
		if (!strcmp(name, MAP_CONFIGS[i]->meta.name)) {
			return MAP_CONFIGS[i];
		}
	}
	return NULL;
}
static const struct RemapConfig *find_remapcfg(const char *name)
{
	for (int i = 0; i < REMAP_CONFIGS_LEN; i++) {
		if (!strcmp(name, REMAP_CONFIGS[i]->meta.name)) {
			return REMAP_CONFIGS[i];
		}
	}
	return NULL;
}
static int find_param(const char *name, size_t len,
                      const struct MSYSParam *params,
                      int start, int end)
{
	for (int i = start; i < end; i++) {
		if (!strncmp(name, params[i].name, len) && params[i].name[len] == '\0')
		{
			return i;
		}
	}
	return -1;
}

static inline int field_end(char c)
{
	switch (c) {
		case '\0': case ':': case ';':
			return 1;
		default:
			return 0;
	}
}

static inline const char *chrnul(const char *s, int c)
{
	for (char t = *s;
	     t != c && t != '\0';
	     t = *(++s));
	return s;
}

static size_t read_field(const char *str, size_t *idx, char *buf, size_t buflen)
{
	size_t si = *idx;
	size_t di = 0;
	char c = str[si];
	while (!field_end(c) && di < buflen - 1) {
		if (c == '#') {
			const char *base = &str[si + 1];
			si += chrnul(base, '\n') - base;
		} else if (!isspace(c)) {
			buf[di++] = c;
		}
		c = str[++si];
	}
	buf[di] = '\0';
	*idx = si;
	return di;
}

static int optchoice(const char *str, const char *choices)
{
	const size_t slen = strlen(str);
	int pos = 0;
	const char *base = choices;
	const char *next = chrnul(base, ':');
	while (*base != '\0') {
		size_t optlen = next - base;
		if (optlen == slen && !strncmp(str, base, optlen)) {
			return pos;
		} else {
			pos++;
			base = *next != '\0' ? next + 1 : next;
			next = chrnul(base, ':');
		}
	}
	return -1;
}

static inline void *clone(const void *src, size_t sz)
{
	void *ret = sz ? malloc(sz) : NULL;
	if (ret != NULL) memcpy(ret, src, sz);
	return ret;
}

static inline void free_allocs(void **allocs, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		free(allocs[i]);
	}
}


#define ERR_OOM 1
#define ERR_FIELDLEN 2
#define ERR_BADTYPE 3
#define ERR_BADCFG 4
#define ERR_NOCFG 5
#define ERR_POSARG_MISSING 6
#define ERR_POSARG_BAD 7
#define ERR_BADARG 8
#define ERR_MAPINIT 9
#define ERR_REMAPINIT 10
#define ERR_WTF 11
#define ERR_INT_MANYPARAMS 12
#define ERR_INT_PARAMTYPE  13
#define ERR_KEYARG_NOVAL 14
#define ERR_KEYARG_BADINT 15
#define ERR_EOF 16
#define ERR_FLAGVAL 17

static const char *ERRMSGS[] = {
	[0] = "Success",
	[ERR_OOM] = "Unable to allocate memory",
	[ERR_FIELDLEN] = "Field too long",
	[ERR_BADTYPE] = "Unknown configuration type",
	[ERR_BADCFG] = "Unknown configurator",
	[ERR_NOCFG] = "Configurator not specified",
	[ERR_POSARG_MISSING] = "Missing positional argument",
	[ERR_POSARG_BAD] = "Unknown positional argument value",
	[ERR_BADARG] = "Unknown argument",
	[ERR_MAPINIT] = "Error initialising map configuration",
	[ERR_REMAPINIT] = "Error initialising remap configuration",
	[ERR_WTF] = "If you see this, somewhere things went terribly wrong",
	[ERR_INT_MANYPARAMS] = "BADAPI: too many parameters",
	[ERR_INT_PARAMTYPE] = "BADAPI: unknown parameter type",
	[ERR_KEYARG_NOVAL] = "No value supplied to keyword argument",
	[ERR_KEYARG_BADINT] = "Bad format for numerical argument",
	[ERR_EOF] = "Unexpected end of file",
	[ERR_FLAGVAL] = "Flag argument supplied with value",
};

static inline int suffix2shift(char suffix)
{
	int shift = 0;
	switch (suffix) {
		case 't': case 'T':
			shift += 10;
		case 'g': case 'G':
			shift += 10;
		case 'm': case 'M':
			shift += 10;
		case 'k': case 'K':
			shift += 10;
			break;
	}
	return shift;
}

static inline int intval(const char *s, long long *nval)
{
	char *end = NULL;
	long long val = strtoll(s, &end, 0);
	int shift = suffix2shift(*end);
	if (*end == '\0' || shift) {
		*nval = val << shift;
		return 0;
	} else {
		return ERR_KEYARG_BADINT;
	}
}

static int msys_writeout(struct MemorySystem *m,
                         struct Remapping *inst_remaps, size_t inst_top,
                         struct Remapping **remaps, size_t remap_top,
                         void **allocs, size_t alloc_top)
{
	struct Remapping *inst_clone;
	struct Remapping **out_remaps;
	void **out_allocs;
	size_t atop = alloc_top + !!inst_top + !!remap_top;

	inst_clone = clone(inst_remaps, inst_top * sizeof(*inst_remaps));
	out_remaps = clone(remaps, remap_top * sizeof(*remaps));
	out_allocs = clone(allocs, atop * sizeof(*allocs));
	if ((inst_top && inst_clone == NULL) ||
	    (remap_top && out_remaps == NULL) ||
	    (atop && out_allocs == NULL))
	{
		free(inst_clone);
		free(out_remaps);
		free(out_allocs);
		return ERR_OOM;
	}

	if (inst_top) out_allocs[alloc_top++] = inst_clone;
	if (remap_top) out_allocs[alloc_top++] = out_remaps;
	m->nremaps = remap_top;
	m->remaps = out_remaps;
	m->nallocs = alloc_top;
	m->allocs = out_allocs;
	return 0;
}

#define MAX_REMAPS 32
#define MAX_ALLOCS 128
#define MAX_FIELDLEN 1024
#define MAX_CFGARGS 128

int ramses_msys_load(const char *str, struct MemorySystem *m, size_t *erridx)
{
	int err = -1;
#define EBAIL(e) { err = (e); goto bail; }

	void *allocs[MAX_ALLOCS];
	size_t alloc_top = 0;
	struct Remapping inst_remaps[MAX_REMAPS];
	size_t inst_top = 0;
	struct Remapping *remaps[MAX_REMAPS];
	size_t remap_top = 0;

	int state = 0;
	size_t si = 0;
	size_t err_si = 0;
	char field[MAX_FIELDLEN];
	void *tmpallocs[MAX_CFGARGS];
	size_t tmpalloc_top = 0;

	int cfgtype = -1;
	union {
		const struct MapConfig *map;
		const struct RemapConfig *remap;
	} config = {.map = NULL};
	const struct MSYSCfgMeta *cfgmeta = NULL;
	int parambase = 0;
	union MSYSArg cfgargs[MAX_CFGARGS];

	/* Do stuff */
	while (read_field(str, &si, field, MAX_FIELDLEN) || str[si] != '\0') {
		if (!field_end(str[si])) EBAIL(ERR_FIELDLEN);

		/* Handle */
		switch (state) {
		case 0: /* Type select */
			cfgtype = optchoice(field, "map:remap");
			if (cfgtype >= 0) {
				state = 1;
			} else {
				EBAIL(ERR_BADTYPE);
			}
			break;
		case 1: /* Config select */
			switch (cfgtype) {
			case 0: /* Map */
				config.map = find_mapcfg(field);
				cfgmeta = &config.map->meta;
				break;
			case 1: /* Remap */
				config.remap = find_remapcfg(field);
				cfgmeta = &config.remap->meta;
				break;
			default: /* Should never happen */
				EBAIL(ERR_WTF);
			}
			if (config.map == NULL) {
				EBAIL(ERR_BADCFG);
			}
			if (cfgmeta->nparams <= MAX_CFGARGS) {
				memset(cfgargs, 0, cfgmeta->nparams * sizeof(*cfgargs));
			} else {
				EBAIL(ERR_INT_MANYPARAMS);
			}
			state = 2;
			break;
		case 2: /* Build args */
			if (parambase < cfgmeta->nparams &&
			    cfgmeta->params[parambase].type == 'p')
			{ /* Positional arg */
				int ch = optchoice(field, cfgmeta->params[parambase].name);
				if (ch >= 0) {
					cfgargs[parambase++].flag = ch;
				} else {
					EBAIL(ERR_POSARG_BAD);
				}
			} else { /* Flag or key-value arg */
				const char *sep = chrnul(field, '=');
				int par = find_param(field, sep - field, cfgmeta->params,
				                     parambase, cfgmeta->nparams);
				if (par >= 0) {
					/* Sanity check */
					switch (cfgmeta->params[par].type) {
					case 'f':
						if (*sep != '\0') {
							EBAIL(ERR_FLAGVAL);
						}
						break;
					case 's':
					case 'i':
						if (sep[0] == '\0' || sep[1] == '\0') {
							EBAIL(ERR_KEYARG_NOVAL);
						}
						break;
					default: /* Should never happen */
						EBAIL(ERR_INT_PARAMTYPE);
					}
					/* Handle param */
					switch (cfgmeta->params[par].type) {
					case 'f':
						cfgargs[par].flag = 1;
						break;
					case 's':
					{
						char *argstr = clone(sep + 1, strlen(sep));
						if (argstr != NULL) {
							cfgargs[par].str = argstr;
							tmpallocs[tmpalloc_top++] = argstr;
						} else {
							EBAIL(ERR_OOM);
						}
					}
						break;
					case 'i':
						if ((err = intval(&sep[1], &cfgargs[par].num))) {
							goto bail;
						}
						break;
					}
				} else {
					EBAIL(ERR_BADARG);
				}
			}
			break;
		default: /* Should never happen */
			EBAIL(ERR_WTF);
		}

		/* Successfully parsed field, set err_si to field end */
		err_si = si;

		/* What next? */
		switch (str[si]) {
		case ':':
			err_si = ++si;
			break;
		case ';':
			err_si = ++si; /* v Fallthrough v */
		case '\0': /* Finish up */
			if (state == 2) {
				if (parambase < cfgmeta->nparams &&
				    cfgmeta->params[parambase].type == 'p')
				{
					EBAIL(ERR_POSARG_MISSING);
				}
				size_t newallocs = 0;
				switch (cfgtype) {
				case 0: /* Map */
					if (config.map->func(&m->mapping, cfgargs,
					                     &allocs[alloc_top], &newallocs))
					{
						EBAIL(ERR_MAPINIT);
					}
					break;
				case 1: /* Remap */
					remaps[remap_top] = &inst_remaps[inst_top];
					if (config.remap->func(&remaps[remap_top], cfgargs,
					                       &allocs[alloc_top], &newallocs))
					{
						EBAIL(ERR_REMAPINIT);
					}
					if (remaps[remap_top] == &inst_remaps[inst_top]) {
						inst_top++;
					}
					remap_top++;
					break;
				default: /* Should never happen */
					EBAIL(ERR_WTF);
				}
				alloc_top += newallocs;
			} else if (state == 1) {
				EBAIL(ERR_NOCFG);
			}
			parambase = 0;
			config.map = NULL;
			cfgmeta = NULL;
			cfgtype = -1;
			free_allocs(tmpallocs, tmpalloc_top);
			tmpalloc_top = 0;
			state = 0;
			break;
		}
	}
	if (state != 0) {
		EBAIL(ERR_EOF);
	}

	/* Done, write out */
	if (!(err = msys_writeout(
		m, inst_remaps, inst_top, remaps, remap_top, allocs, alloc_top
	)))
	{
		return 0;
	}

#undef EBAIL
bail:
	/* Cleanup */
	free_allocs(allocs, alloc_top);
	if (erridx != NULL) {
		*erridx = err_si;
	}
	return err;
}

const char *ramses_msys_load_strerr(int err)
{
	return ERRMSGS[err];
}

void ramses_msys_free(struct MemorySystem *m)
{
	free_allocs(m->allocs, m->nallocs);
	free(m->allocs);
}
