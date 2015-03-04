/*
 * Core functions for Marvell System On Chip
 *
 * Copyright (C) 2012 Marvell
 *
 * Lior Amsalem <alior@marvell.com>
 * Gregory CLEMENT <gregory.clement@free-electrons.com>
 * Thomas Petazzoni <thomas.petazzoni@free-electrons.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __ARCH_MVEBU_COMMON_H
#define __ARCH_MVEBU_COMMON_H

#define LSP_VERSION    "linux-3.10.39-2014_T3.0.eng_drop_v1"

void mvebu_restart(char mode, const char *cmd);

void armada_xp_cpu_die(unsigned int cpu);
void mvebu_pmsu_set_cpu_boot_addr(int hw_cpu, void *boot_addr);
int mvebu_boot_cpu(int cpu);

#endif
