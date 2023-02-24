/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <stdbool.h>
#include <stddef.h>

struct provider_clk {
	const char *provider_name;
	u32 idx;
	struct clk *ck;
};

struct clkchk_cfg_t {
	bool aee_excp_on_fail;
	bool warn_on_fail;
	const char * const *compatible;
	const char * const *off_pll_names;
	const char * const *all_clk_names;
	void (*setup_provider_clk)(struct provider_clk *pvdck);
	void (*dump_pwr_status)(void);
};

int clkchk_init(struct clkchk_cfg_t *cfg);
