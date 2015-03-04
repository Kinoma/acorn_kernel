/*
 * Coherency fabric (Aurora) support for Armada 370 and XP platforms.
 *
 * Copyright (C) 2012 Marvell
 *
 * Yehuda Yitschak <yehuday@marvell.com>
 * Gregory Clement <gregory.clement@free-electrons.com>
 * Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 * The Armada 370 and Armada XP SOCs have a coherency fabric which is
 * responsible for ensuring hardware coherency between all CPUs and between
 * CPUs and I/O masters. This file initializes the coherency fabric and
 * supplies basic routines for configuring and controlling hardware coherency
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mbus.h>
#include <linux/clk.h>
#include <asm/smp_plat.h>
#include <asm/cacheflush.h>
#include "armada-370-xp.h"

/*
 * Enable the I/O coherency workaround on Armada 375. This workaround
 * consists in using the two channels of the first XOR engine to
 * trigger a XOR transaction that serves as the I/O coherency barrier.
 */
#define ARMADA_375_COHERENCY_WA

unsigned long __cpuinitdata coherency_phys_base;
static void __iomem *coherency_base;
static void __iomem *coherency_cpu_base;

/* Coherency fabric registers */
#define COHERENCY_FABRIC_CFG_OFFSET		   0x4

#define IO_SYNC_BARRIER_CTL_OFFSET		   0x0

enum {
	COHERENCY_FABRIC_TYPE_NONE,
	COHERENCY_FABRIC_TYPE_ARMADA_370_XP,
	COHERENCY_FABRIC_TYPE_ARMADA_375,
};

/*
 * The "marvell,coherency-fabric" compatible string is kept for
 * backward compatibility reasons, and is equivalent to
 * "marvell,armada-370-coherency-fabric".
 */
static struct of_device_id of_coherency_table[] = {
	{.compatible = "marvell,coherency-fabric",
	 .data = (void*) COHERENCY_FABRIC_TYPE_ARMADA_370_XP },
	{.compatible = "marvell,armada-370-coherency-fabric",
	 .data = (void*) COHERENCY_FABRIC_TYPE_ARMADA_370_XP },
	{.compatible = "marvell,armada-375-coherency-fabric",
	 .data = (void*) COHERENCY_FABRIC_TYPE_ARMADA_375 },
	{ /* end of list */ },
};

/* Function defined in coherency_ll.S */
int ll_set_cpu_coherent(void __iomem *base_addr, unsigned int hw_cpu_id);

int set_cpu_coherent(unsigned int hw_cpu_id, int smp_group_id)
{
	if (!coherency_base) {
		pr_warn("Can't make CPU %d cache coherent.\n", hw_cpu_id);
		pr_warn("Coherency fabric is not initialized\n");
		return 1;
	}

	return ll_set_cpu_coherent(coherency_base, hw_cpu_id);
}

#ifdef ARMADA_375_COHERENCY_WA

static void __iomem *xor_base, *xor_high_base;
static dma_addr_t coherency_wa_buf_phys[CONFIG_NR_CPUS];
static void *coherency_wa_buf[CONFIG_NR_CPUS];
static bool coherency_wa_enabled;

#define XOR_CONFIG(chan)            (0x10 + (chan * 4))
#define XOR_ACTIVATION(chan)        (0x20 + (chan * 4))
#define WINDOW_BAR_ENABLE(chan)     (0x240 + ((chan) << 2))
#define WINDOW_BASE(w)              (0x250 + ((w) << 2))
#define WINDOW_SIZE(w)              (0x270 + ((w) << 2))
#define WINDOW_REMAP_HIGH(w)        (0x290 + ((w) << 2))
#define WINDOW_OVERRIDE_CTRL(chan)  (0x2A0 + ((chan) << 2))
#define XOR_DEST_POINTER(chan)      (0x2B0 + (chan * 4))
#define XOR_BLOCK_SIZE(chan)        (0x2C0 + (chan * 4))
#define XOR_INIT_VALUE_LOW           0x2E0
#define XOR_INIT_VALUE_HIGH          0x2E4

static inline void mvebu_hwcc_armada375_sync_io_barrier_wa(void)
{
	int idx = smp_processor_id();

	/* Write '1' to the first word of the buffer */
	writel(0x1, coherency_wa_buf[idx]);

	/* Wait untill the engine is idle */
	while ((readl(xor_base + XOR_ACTIVATION(idx)) >> 4) & 0x3)
		;

	dmb();

	/* Trigger channel */
	writel(0x1, xor_base + XOR_ACTIVATION(idx));

	/* Poll the data until it is cleared by the XOR transaction */
	while (readl(coherency_wa_buf[idx]))
		;
}

static void __init armada_375_coherency_init_wa(void)
{
	const struct mbus_dram_target_info *dram;
	struct device_node *xor_node;
	struct property *xor_status;
	struct clk *xor_clk;
	u32 win_enable = 0;
	int i;

	/*
	 * Since the workaround uses one XOR engine, we grab a
	 * reference to its Device Tree node first.
	 */
	xor_node = of_find_compatible_node(NULL, NULL, "marvell,orion-xor");
	BUG_ON(!xor_node);

	/*
	 * Then we mark it as disabled so that the real XOR driver
	 * will not use it.
	 */
	xor_status = kzalloc(sizeof(struct property), GFP_KERNEL);
	BUG_ON(!xor_status);

	xor_status->value = kstrdup("disabled", GFP_KERNEL);
	BUG_ON(!xor_status->value);

	xor_status->length = 8;
	xor_status->name = kstrdup("status", GFP_KERNEL);
	BUG_ON(!xor_status->name);

	of_update_property(xor_node, xor_status);

	/*
	 * And we remap the registers, get the clock, and do the
	 * initial configuration of the XOR engine.
	 */
	xor_base = of_iomap(xor_node, 0);
	xor_high_base = of_iomap(xor_node, 1);

	xor_clk = of_clk_get_by_name(xor_node, NULL);
	BUG_ON(!xor_clk);

	clk_prepare_enable(xor_clk);

	dram = mv_mbus_dram_info();

	for (i = 0; i < 8; i++) {
		writel(0, xor_base + WINDOW_BASE(i));
		writel(0, xor_base + WINDOW_SIZE(i));
		if (i < 4)
			writel(0, xor_base + WINDOW_REMAP_HIGH(i));
	}

	for (i = 0; i < dram->num_cs; i++) {
		const struct mbus_dram_window *cs = dram->cs + i;
		writel((cs->base & 0xffff0000) |
		       (cs->mbus_attr << 8) |
		       dram->mbus_dram_target_id, xor_base + WINDOW_BASE(i));
		writel((cs->size - 1) & 0xffff0000, xor_base + WINDOW_SIZE(i));

		win_enable |= (1 << i);
		win_enable |= 3 << (16 + (2 * i));
	}

	writel(win_enable, xor_base + WINDOW_BAR_ENABLE(0));
	writel(win_enable, xor_base + WINDOW_BAR_ENABLE(1));
	writel(0, xor_base + WINDOW_OVERRIDE_CTRL(0));
	writel(0, xor_base + WINDOW_OVERRIDE_CTRL(1));

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		coherency_wa_buf[i] = kzalloc(PAGE_SIZE, GFP_KERNEL);
		BUG_ON(!coherency_wa_buf[i]);

		/*
		 * We can't use the DMA mapping API, since we don't
		 * have a valid 'struct device' pointer
		 */
		coherency_wa_buf_phys[i] =
			virt_to_phys(coherency_wa_buf[i]);
		BUG_ON(!coherency_wa_buf_phys[i]);

		/*
		 * Configure the XOR engine for memset operation, with
		 * a 128 bytes block size
		 */
		writel(0x444, xor_base + XOR_CONFIG(i));
		writel(128, xor_base + XOR_BLOCK_SIZE(i));
		writel(coherency_wa_buf_phys[i], xor_base + XOR_DEST_POINTER(i));
	}

	writel(0x0, xor_base + XOR_INIT_VALUE_LOW);
	writel(0x0, xor_base + XOR_INIT_VALUE_HIGH);

	coherency_wa_enabled = true;
}
#endif

static inline void mvebu_hwcc_sync_io_barrier(void)
{
#ifdef ARMADA_375_COHERENCY_WA
	if (coherency_wa_enabled) {
		mvebu_hwcc_armada375_sync_io_barrier_wa();
		return;
	}
#endif
	writel(0x1, coherency_cpu_base + IO_SYNC_BARRIER_CTL_OFFSET);
	while (readl(coherency_cpu_base + IO_SYNC_BARRIER_CTL_OFFSET) & 0x1);
}

static dma_addr_t mvebu_hwcc_dma_map_page(struct device *dev, struct page *page,
				  unsigned long offset, size_t size,
				  enum dma_data_direction dir,
				  struct dma_attrs *attrs)
{
	if (dir != DMA_TO_DEVICE)
		mvebu_hwcc_sync_io_barrier();
	return pfn_to_dma(dev, page_to_pfn(page)) + offset;
}


static void mvebu_hwcc_dma_unmap_page(struct device *dev, dma_addr_t dma_handle,
			      size_t size, enum dma_data_direction dir,
			      struct dma_attrs *attrs)
{
	if (dir != DMA_TO_DEVICE)
		mvebu_hwcc_sync_io_barrier();
}

static void mvebu_hwcc_dma_sync(struct device *dev, dma_addr_t dma_handle,
			size_t size, enum dma_data_direction dir)
{
	if (dir != DMA_TO_DEVICE)
		mvebu_hwcc_sync_io_barrier();
}

static struct dma_map_ops mvebu_hwcc_dma_ops = {
	.alloc			= arm_dma_alloc,
	.free			= arm_dma_free,
	.mmap			= arm_dma_mmap,
	.map_page		= mvebu_hwcc_dma_map_page,
	.unmap_page		= mvebu_hwcc_dma_unmap_page,
	.get_sgtable		= arm_dma_get_sgtable,
	.map_sg			= arm_dma_map_sg,
	.unmap_sg		= arm_dma_unmap_sg,
	.sync_single_for_cpu	= mvebu_hwcc_dma_sync,
	.sync_single_for_device	= mvebu_hwcc_dma_sync,
	.sync_sg_for_cpu	= arm_dma_sync_sg_for_cpu,
	.sync_sg_for_device	= arm_dma_sync_sg_for_device,
	.set_dma_mask		= arm_dma_set_mask,
};

static int mvebu_hwcc_platform_notifier(struct notifier_block *nb,
				       unsigned long event, void *__dev)
{
	struct device *dev = __dev;

	if (event != BUS_NOTIFY_ADD_DEVICE)
		return NOTIFY_DONE;
	set_dma_ops(dev, &mvebu_hwcc_dma_ops);

	return NOTIFY_OK;
}

static struct notifier_block mvebu_hwcc_platform_nb = {
	.notifier_call = mvebu_hwcc_platform_notifier,
};

static void __init armada_370_coherency_init(struct device_node *np)
{
	struct resource res;
	of_address_to_resource(np, 0, &res);
	coherency_phys_base = res.start;
	/*
	 * Ensure secondary CPUs will see the updated value,
	 * which they read before they join the coherency
	 * fabric, and therefore before they are coherent with
	 * the boot CPU cache.
	 */
	sync_cache_w(&coherency_phys_base);
	coherency_base = of_iomap(np, 0);
	coherency_cpu_base = of_iomap(np, 1);
	set_cpu_coherent(cpu_logical_map(smp_processor_id()), 0);
}

static void __init armada_375_coherency_init(struct device_node *np)
{
	coherency_cpu_base = of_iomap(np, 0);
#ifdef ARMADA_375_COHERENCY_WA
	armada_375_coherency_init_wa();
#endif
}

static int coherency_type(void)
{
	struct device_node *np;

	np = of_find_matching_node(NULL, of_coherency_table);
	if (np) {
                const struct of_device_id *match =
                    of_match_node(of_coherency_table, np);
		int type;

		type = (int) match->data;

		/* Armada 370/XP coherency works in both UP and SMP */
		if (type == COHERENCY_FABRIC_TYPE_ARMADA_370_XP)
			return type;

		/* Armada 38x coherency works only on SMP */
		else if (type == COHERENCY_FABRIC_TYPE_ARMADA_375 && is_smp())
			return type;
	}

	return COHERENCY_FABRIC_TYPE_NONE;
}

int coherency_available(void)
{
	return coherency_type() != COHERENCY_FABRIC_TYPE_NONE;
}

int __init coherency_init(void)
{
	int type = coherency_type();
	struct device_node *np;

	np = of_find_matching_node(NULL, of_coherency_table);

	if (type == COHERENCY_FABRIC_TYPE_ARMADA_370_XP)
		armada_370_coherency_init(np);
	else if (type == COHERENCY_FABRIC_TYPE_ARMADA_375)
		armada_375_coherency_init(np);

	return 0;
}

static int __init coherency_late_init(void)
{
	if (coherency_available())
		bus_register_notifier(&platform_bus_type,
				      &mvebu_hwcc_platform_nb);
	return 0;
}

postcore_initcall(coherency_late_init);
