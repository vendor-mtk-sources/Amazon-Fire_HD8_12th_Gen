/* SPDX-License-Identifier: GPL-2.0 */

enum
{
    CLK_NUM = 1
};

int acenna_clk_init(struct device* dev, struct clk** clocks)
{
    (void)dev;
    (void)clocks;

    return 0;
}

int acenna_clk_set_rate(struct clk** clocks, u64* clk_freq)
{
    (void)clocks;
    (void)clk_freq;

    return -EPERM;
}

int acenna_clk_get_rate(struct clk** clocks, u64* clk_freq)
{
    (void)clocks;
    (void)clk_freq;

    return -EPERM;
}

int acenna_clk_prepare_enable(struct clk** clocks)
{
    (void)clocks;

    return 0;
}

void acenna_clk_disable_unprepare(struct clk** clocks)
{
    (void)clocks;
}

typedef irqreturn_t (*isr)(int, void*);

int nna_irq_wrapper_cleanup(void)
{
    return 0;
}

int nna_irq_wrapper_init(void)
{
    return 0;
}

int nna_irq_wrapper_request(unsigned int irq, irq_handler_t handler, unsigned long flags, const char* name, void* dev_id, unsigned int nna_num)
{
    (void)name;
    (void)nna_num;

    return request_irq(irq, handler, flags, "nna_halt_irq", dev_id);
}
