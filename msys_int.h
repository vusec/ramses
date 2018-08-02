/*
 * Copyright (c) 2018 Vrije Universiteit Amsterdam
 *
 * This program is licensed under the GPL2+.
 */

#ifndef RAMSES_MSYS_INT_H
#define RAMSES_MSYS_INT_H 1

#include <ramses/map.h>
#include <ramses/remap.h>

struct MSYSParam {
	char *name;
	char type;
};

union MSYSArg {
	int flag;
	long long int num;
	char *str;
};

typedef int (*msys_map_config_fn_t)(struct Mapping *, union MSYSArg *,
                                    void **, size_t *);
typedef int (*msys_remap_config_fn_t)(struct Remapping **, union MSYSArg *,
                                      void **, size_t *);

struct MSYSCfgMeta {
	const char *name;
	const struct MSYSParam *params;
	int nparams;
};

struct MapConfig {
	struct MSYSCfgMeta meta;
	msys_map_config_fn_t func;
};

struct RemapConfig {
	struct MSYSCfgMeta meta;
	msys_remap_config_fn_t func;
};

#endif /* msys_int.h */
