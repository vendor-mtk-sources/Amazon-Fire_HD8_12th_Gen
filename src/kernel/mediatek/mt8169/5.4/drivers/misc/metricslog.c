/*
* metricslog.c
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

#include <linux/miscdevice.h>
#include <linux/metricslog.h>

static const struct amazon_logger_ops *logger_ops = NULL;
void amazon_logger_ops_set(const struct amazon_logger_ops *ops)
{
    logger_ops = ops;
}
EXPORT_SYMBOL(amazon_logger_ops_set);

const struct amazon_logger_ops *amazon_logger_ops_get(void)
{
    return logger_ops;
}
EXPORT_SYMBOL(amazon_logger_ops_get);
