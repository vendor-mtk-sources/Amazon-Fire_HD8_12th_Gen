// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt) "[clkchk] " fmt

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/syscore_ops.h>
#include "clkchk.h"

#define AEE_EXCP_CHECK_PLL_FAIL	0
#define CLKDBG_CCF_API_4_4	1
#define MAX_PLLS		32

#define TOTAL_CLK_NUM		1050
#define MAX_SUBSYS_CLK_NUM	300

#if AEE_EXCP_CHECK_PLL_FAIL
#include <mt-plat/aee.h>
#endif

#if !CLKDBG_CCF_API_4_4

/* backward compatible */

static const char *clk_hw_get_name(const struct clk_hw *hw)
{
	return __clk_get_name(hw->clk);
}

static bool clk_hw_is_prepared(const struct clk_hw *hw)
{
	return __clk_is_prepared(hw->clk);
}

static bool clk_hw_is_enabled(const struct clk_hw *hw)
{
	return __clk_is_enabled(hw->clk);
}

static unsigned long clk_hw_get_rate(const struct clk_hw *hw)
{
	return __clk_get_rate(hw->clk);
}

static struct clk_hw *clk_hw_get_parent(const struct clk_hw *hw)
{
	return __clk_get_hw(clk_get_parent(hw->clk));
}

#endif /* !CLKDBG_CCF_API_4_4 */

static struct clkchk_cfg_t *clkchk_cfg;

static const char *get_provider_name(struct device_node *node, u32 *cells)
{
	const char *name;
	const char *p;
	u32 cc;

	if (of_property_read_u32(node, "#clock-cells", &cc) != 0)
		cc = 0;

	if (cells != NULL)
		*cells = cc;

	if (cc == 0U) {
		if (of_property_read_string(node,
			"clock-output-names", &name) < 0)
			name = node->name;

		return name;
	}

	if (of_property_read_string(node, "compatible", &name) < 0)
		name = node->name;

	p = strchr(name, (int)'-');

	if (p != NULL)
		return p + 1;
	else
		return name;
}

static struct provider_clk *get_all_provider_clks(void)
{
	static struct provider_clk provider_clks[TOTAL_CLK_NUM];
	struct device_node *node = NULL;
	unsigned int n = 0;

	if (provider_clks[0].ck != NULL)
		return provider_clks;

	do {
		const char *node_name;
		u32 cells;

		node = of_find_node_with_property(node, "#clock-cells");

		if (node == NULL)
			break;

		node_name = get_provider_name(node, &cells);

		if (cells != 0U) {
			unsigned int i;

			for (i = 0; i < MAX_SUBSYS_CLK_NUM; i++) {
				struct of_phandle_args pa;
				struct clk *ck;

				pa.np = node;
				pa.args[0] = i;
				pa.args_count = 1;
				ck = of_clk_get_from_provider(&pa);

				if (PTR_ERR(ck) == -EINVAL)
					break;
				else if (IS_ERR_OR_NULL(ck))
					continue;

				provider_clks[n].ck = ck;
				provider_clks[n].idx = i;
				provider_clks[n].provider_name = node_name;
				++n;
			}
		}
	} while (node != NULL && n < TOTAL_CLK_NUM);

	return provider_clks;
}

static struct provider_clk *__clk_dbg_lookup_pvdck(const char *name)
{
	struct provider_clk *pvdck = get_all_provider_clks();
	const char *ck_name;

	for (; pvdck->ck != NULL; pvdck++) {
		ck_name = __clk_get_name(pvdck->ck);
		if (ck_name && !strcmp(ck_name, name))
			return pvdck;
	}

	return NULL;
}

struct clk *__clk_dbg_lookup(const char *name)
{
	struct provider_clk *pvdck = __clk_dbg_lookup_pvdck(name);

	if (pvdck)
		return pvdck->ck;

	return NULL;
}


static const char *ccf_state(struct clk_hw *hw)
{
	if (clk_hw_is_enabled(hw))
		return "enabled";

	if (clk_hw_is_prepared(hw))
		return "prepared";

	return "disabled";
}

static void print_enabled_clks(void)
{
	const char * const *cn = clkchk_cfg->all_clk_names;

	pr_info("enabled clks:\n");

	for (; *cn != NULL; cn++) {
		struct clk *c = __clk_dbg_lookup(*cn);
		struct clk_hw *c_hw = __clk_get_hw(c);
		struct clk_hw *p_hw;

		if (IS_ERR_OR_NULL(c) || c_hw == NULL)
			continue;

		p_hw = clk_hw_get_parent(c_hw);

		if (p_hw == NULL)
			continue;

		if (!clk_hw_is_prepared(c_hw) &&
			!clk_hw_is_enabled(c_hw))
			continue;

		pr_info("[%-17s: %8s, %3d, %3d, %10ld, %17s]\n",
			clk_hw_get_name(c_hw),
			ccf_state(c_hw),
			clk_hw_is_prepared(c_hw),
			clk_hw_is_enabled(c_hw),
			clk_hw_get_rate(c_hw),
			p_hw != NULL ? clk_hw_get_name(p_hw) : "- ");
	}
}

static void check_pll_off(void)
{
	static struct clk *off_plls[MAX_PLLS];

	struct clk **c;
	int invalid = 0;
	char buf[128] = {0};
	int n = 0;

	if (off_plls[0] == NULL) {
		const char * const *pn = clkchk_cfg->off_pll_names;
		struct clk **end = off_plls + MAX_PLLS - 1;

		for (c = off_plls; *pn != NULL && c < end; pn++, c++)
			*c = __clk_dbg_lookup(*pn);
	}

	for (c = off_plls; *c != NULL; c++) {
		struct clk_hw *c_hw = __clk_get_hw(*c);

		if (c_hw == NULL)
			continue;

		if (!clk_hw_is_prepared(c_hw) && !clk_hw_is_enabled(c_hw))
			continue;

		n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s ",
				clk_hw_get_name(c_hw));

		invalid++;
	}

	if (invalid == 0)
		return;

	/* invalid. output debug info */

	pr_info("unexpected unclosed PLL: %s\n", buf);
	print_enabled_clks();

#if AEE_EXCP_CHECK_PLL_FAIL
	if (clkchk_cfg->aee_excp_on_fail)
		aee_kernel_exception("clkchk", "unclosed PLL: %s\n", buf);
#endif

	if (clkchk_cfg->warn_on_fail)
		WARN_ON(true);
}

static int clkchk_syscore_suspend(void)
{
	clkchk_cfg->dump_pwr_status();
	check_pll_off();

	return 0;
}

static void clkchk_syscore_resume(void)
{
}

static struct syscore_ops clkchk_syscore_ops = {
	.suspend = clkchk_syscore_suspend,
	.resume = clkchk_syscore_resume,
};

int clkchk_init(struct clkchk_cfg_t *cfg)
{
	const char * const *c;
	bool match = false;

	if (cfg == NULL || cfg->compatible == NULL
		|| cfg->all_clk_names == NULL || cfg->off_pll_names == NULL) {
		pr_info("Invalid clkchk_cfg.\n");
		return -EINVAL;
	}

	clkchk_cfg = cfg;

	for (c = cfg->compatible; *c != NULL; c++) {
		if (of_machine_is_compatible(*c) != 0) {
			match = true;
			break;
		}
	}

	if (!match)
		return -ENODEV;

	register_syscore_ops(&clkchk_syscore_ops);

	return 0;
}
EXPORT_SYMBOL(clkchk_init);

MODULE_LICENSE("GPL");
