/*
* mtk_wdt_func.h
*
* Copyright 2021 Amazon Technologies, Inc. All Rights Reserved.
*
* The code contained herein is licensed under the GNU General Public
* License Version 2.
* You may obtain a copy of the GNU General Public License
* Version 2 or later at the following locations:
*
* http://www.opensource.org/licenses/gpl-license.html
* http://www.gnu.org/copyleft/gpl.html
*/

#ifndef _MTK_WDT_FUNC_H
#define _MTK_WDT_FUNC_H

struct wdt_mode_ops {
	int (*mtk_wdt_mode_config_for_sysrq)(void);
	void (*deactive_mtk_wdd)(void);
};

void wdt_mode_ops_set(const struct wdt_mode_ops *ops);
const struct wdt_mode_ops *wdt_mode_ops_get(void);
#endif /* _MTK_WDT_FUNC_H */
