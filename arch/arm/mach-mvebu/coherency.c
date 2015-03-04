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
#include <linux/pci.h>
#include <asm/smp_plat.h>
#include <asm/cacheflush.h>
#include "armada-370-xp.h"


unsigned long coherency_phys_base;
void __iomem *coherency_base;
static void __iomem *coherency_cpu_base;
bool coherency_hard_mode;

/* Coherency fabric registers */
#define COHERENCY_FABRIC_CFG_OFFSET		   0x4

#define IO_SYNC_BARRIER_CTL_OFFSET		   0x0

enum {
	COHERENCY_FABRIC_TYPE_NONE,
	COHERENCY_FABRIC_TYPE_ARMADA_370_XP,
	COHERENCY_FABRIC_TYPE_ARMADA_375,
	COHERENCY_FABRIC_TYPE_ARMADA_380,
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
	{.compatible = "marvell,armada-380-coherency-fabric",
	 .data = (void*) COHERENCY_FABRIC_TYPE_ARMADA_380 },
	{ /* end of list */ },
};

/* Functions defined in coherency_ll.S */
int ll_enable_coherency(void);
void ll_add_cpu_to_smp_group(void);

int set_cpu_coherent(void)
{
	if (!coherency_base) {
		pr_warn("Can't make current CPU cache coherent.\n");
		pr_warn("Coherency fabric is not initialized\n");
		return 1;
	}

	ll_add_cpu_to_smp_group();
	return ll_enable_coherency();
}

static inline void mvebu_hwcc_sync_io_barrier(void)
{
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
	.alloc			= arm_coherent_dma_alloc,
	.free			= arm_coherent_dma_free,
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
	set_cpu_coherent();
}

static void __init armada_375_coherency_init(struct device_node *np)
{
	coherency_cpu_base = of_iomap(np, 0);
}

static void __init armada_380_coherency_init(struct device_node *np)
{
	coherency_cpu_base = of_iomap(np, 0);
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

		if (type == COHERENCY_FABRIC_TYPE_ARMADA_370_XP ||
			type == COHERENCY_FABRIC_TYPE_ARMADA_375    ||
			type == COHERENCY_FABRIC_TYPE_ARMADA_380)
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

	if (type != COHERENCY_FABRIC_TYPE_NONE)
		coherency_hard_mode = true;
	else
		coherency_hard_mode = false;

	np = of_find_matching_node(NULL, of_coherency_table);

	if (type == COHERENCY_FABRIC_TYPE_ARMADA_370_XP)
		armada_370_coherency_init(np);
	else if (type == COHERENCY_FABRIC_TYPE_ARMADA_375)
		armada_375_coherency_init(np);
	else if (type == COHERENCY_FABRIC_TYPE_ARMADA_380)
		armada_380_coherency_init(np);

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

static int __init coherency_pci_notify_init(void)
{
	if (coherency_available())
		bus_register_notifier(&pci_bus_type,
				       &mvebu_hwcc_platform_nb);
	return 0;
}

arch_initcall(coherency_pci_notify_init);
