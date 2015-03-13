/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

This software file (the "File") is owned and distributed by Marvell
International Ltd. and/or its affiliates ("Marvell") under the following
alternative licensing terms.  Once you have made an election to distribute the
File under one of the following license alternatives, please (i) delete this
introductory statement regarding license alternatives, (ii) delete the two
license alternatives that you have not elected to use and (iii) preserve the
Marvell copyright notice above.


********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or
modify this File in accordance with the terms and conditions of the General
Public License Version 2, June 1991 (the "GPL License"), a copy of which is
available along with the File in the license.txt file or by writing to the Free
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
on the worldwide web at http://www.gnu.org/licenses/gpl.txt.

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
DISCLAIMED.  The GPL License provides additional details about this warranty
disclaimer.
*******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>

#include <msApiTypes.h>
#include "mv802_3.h"
#include "mv_switch.h"
#include "mv_phy.h"


static ssize_t mv_switch_help(char *buf)
{
	int off = 0;

	off += scnprintf(buf + off, PAGE_SIZE, "cat help                            - show this help\n");
	off += scnprintf(buf + off, PAGE_SIZE, "cat stats                           - show statistics for switch all ports info\n");
	off += scnprintf(buf + off, PAGE_SIZE, "cat status                          - show switch status\n");
	off += scnprintf(buf + off, PAGE_SIZE, "cat atu_show                        - show switch MAC Table\n");
	off += scnprintf(buf + off, PAGE_SIZE, "echo p grp        > port_add        - map switch port to a network device\n");
	off += scnprintf(buf + off, PAGE_SIZE, "echo p            > port_del        - unmap switch port from a network device\n");
	off += scnprintf(buf + off, PAGE_SIZE, "echo p r t   > reg_r                - read switch register.  t: 1-phy, 2-port, 3-global, 4-global2, 5-smi\n");
	off += scnprintf(buf + off, PAGE_SIZE, "echo p r t v > reg_w                - write switch register. t: 1-phy, 2-port, 3-global, 4-global2, 5-smi\n");
#ifdef CONFIG_MV_SW_PTP
	off += scnprintf(buf + off, PAGE_SIZE, "echo p r t   > ptp_reg_r            - read ptp register.  p: 15-PTP Global, 14-TAI Global, t: not used\n");
	off += scnprintf(buf + off, PAGE_SIZE, "echo p r t v > ptp_reg_w            - write ptp register. p: 15-PTP Global, 14-TAI Global, t: not used\n");
#endif
	off += scnprintf(buf + off, PAGE_SIZE, "echo p en          > power_set      - set port power state.\n");
	off += scnprintf(buf + off, PAGE_SIZE, "\ten: 0-down, 1-up \n");
	off += scnprintf(buf + off, PAGE_SIZE, "echo p   	   > power_get	    - get port power state\n");

	return off;
}

static ssize_t mv_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *name = attr->attr.name;
	int off = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (!strcmp(name, "stats"))
		mv_switch_stats_print();
	else if (!strcmp(name, "status"))
		mv_switch_status_print();
	else if (!strcmp(name, "atu_show"))
		mv_switch_atu_print();
	else
		off = mv_switch_help(buf);

	return off;
}

static ssize_t mv_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	unsigned long   flags;
	int             err, port, reg, type, state;
	unsigned int    v;
	MV_U16          val;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read arguments */
	err = port = reg = type = val = 0;
	sscanf(buf, "%d %d %d %x", &port, &reg, &type, &v);

	local_irq_save(flags);
	if (!strcmp(name, "reg_r")) {
		err = mv_switch_reg_read(port, reg, type, &val);
	} else if (!strcmp(name, "reg_w")) {
		val = (MV_U16)v;
		err = mv_switch_reg_write(port, reg, type, v);
#ifdef CONFIG_MV_SW_PTP
	} else if (!strcmp(name, "ptp_reg_r")) {
		err = mv_switch_ptp_reg_read(port, reg, &val);
	} else if (!strcmp(name, "ptp_reg_w")) {
		val = (MV_U16)v;
		err = mv_switch_ptp_reg_write(port, reg, val);
#endif
	} else if (!strcmp(name, "power_set")) {
		state = reg;
#if 0
		err = mv_switch_port_power_set(port, state != 0);
#endif
		err = mv_phy_port_power_state_set(port, state != 0);
		mvOsPrintf(" - %s, set port(%d) power %s!\n",
			err == 0 ? "SUCCESS" : "FAILED", port, state == 0 ? "off" : "on");
		goto out;

	} else if (!strcmp(name, "power_get")) {
		GT_BOOL state;
		err = mv_phy_port_power_state_get(port, &state);
		/*for mv_phy_port_power_state_get, power_down is true while in sysfs power_on i true*/
		state = !state;
		mvOsPrintf("- %s, port(%d) power is %s!\n",
			err == 0 ? "SUCCESS" : "FAILED", port, state == GT_FALSE ? "off" : "on");
		goto out;
	}
	printk(KERN_ERR "switch register access: type=%d, port=%d, reg=%d", type, port, reg);

	if (err)
		printk(KERN_ERR " - FAILED, err=%d\n", err);
	else
		printk(KERN_ERR " - SUCCESS, val=0x%04x\n", val);

out:
	local_irq_restore(flags);

	return err ? -EINVAL : len;
}

static ssize_t mv_switch_netdev_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	const char      *name = attr->attr.name;
	int             err = 0, port = 0, group;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	/* Read arguments */
	sscanf(buf, "%d %d", &port, &group);


	if (!strcmp(name, "port_add"))
		err = mv_switch_port_add(port, group);
	else if (!strcmp(name, "port_del"))
		err = mv_switch_port_del(port);

	if (err)
		printk(KERN_ERR " - FAILED, err=%d\n", err);
	else
		printk(KERN_ERR " - SUCCESS\n");

	return err ? -EINVAL : len;
}

static DEVICE_ATTR(reg_r,       S_IWUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(reg_w,       S_IWUSR, mv_switch_show, mv_switch_store);
#ifdef CONFIG_MV_SW_PTP
static DEVICE_ATTR(ptp_reg_r,   S_IWUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(ptp_reg_w,   S_IWUSR, mv_switch_show, mv_switch_store);
#endif
static DEVICE_ATTR(status,      S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(stats,       S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(help,        S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(port_add,    S_IWUSR, mv_switch_show, mv_switch_netdev_store);
static DEVICE_ATTR(port_del,    S_IWUSR, mv_switch_show, mv_switch_netdev_store);
static DEVICE_ATTR(atu_show,    S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(power_set,   S_IRUSR, mv_switch_show, mv_switch_store);
static DEVICE_ATTR(power_get,   S_IRUSR, mv_switch_show, mv_switch_store);


static struct attribute *mv_switch_attrs[] = {
	&dev_attr_reg_r.attr,
	&dev_attr_reg_w.attr,
#ifdef CONFIG_MV_SW_PTP
	&dev_attr_ptp_reg_r.attr,
	&dev_attr_ptp_reg_w.attr,
#endif
	&dev_attr_status.attr,
	&dev_attr_stats.attr,
	&dev_attr_help.attr,
	&dev_attr_port_add.attr,
	&dev_attr_port_del.attr,
	&dev_attr_atu_show.attr,
	&dev_attr_power_set.attr,
	&dev_attr_power_get.attr,
	NULL
};

static struct attribute_group mv_switch_group = {
	.name = "mv_switch",
	.attrs = mv_switch_attrs,
};


static dev_t  base_dev;

static ssize_t mv_carrier_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int port_no = MINOR(dev->devt) - MINOR(base_dev);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	strcpy(buf, mv_str_link_state(port_no));
	return strlen(buf);
}


static ssize_t mv_speed_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int port_no = MINOR(dev->devt) - MINOR(base_dev);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	strcpy(buf, mv_str_speed_state(port_no));
	return strlen(buf);
}


static ssize_t mv_duplex_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	int port_no = MINOR(dev->devt) - MINOR(base_dev);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	strcpy(buf, mv_str_duplex_state(port_no));
	return strlen(buf);
}

static ssize_t mv_link_power_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	unsigned long state;
	int rc;
	int port_no = MINOR(dev->devt) - MINOR(base_dev);

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	/*state = simple_strtoul(buf, NULL, 10);*/
	rc = kstrtoul(buf, 10, &state);
	if (rc) {
		pr_err("%s: kstrtoul failed\n", __func__);
		return 0;
	}
#if 0
	mv_switch_port_power_set(port_no, state != 0);
#endif
	mv_phy_port_power_state_set(port_no, state != 0);
	return strlen(buf);
}


static ssize_t mv_link_power_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	GT_BOOL state;
	int port_no = MINOR(dev->devt) - MINOR(base_dev);

	mv_phy_port_power_state_get(port_no, &state);

	/*return sprintf(buf, "%d\n", mv_switch_port_power_get(port_no));*/
	return sprintf(buf, "%d\n", !state);
}

static ssize_t mv_mac_addr_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int port_no = MINOR(dev->devt) - MINOR(base_dev);
	uint8_t (*mac_addresses)[6];
	int    rc;
	size_t i, n;
	ssize_t len = 0;

	n = mv_switch_get_peer_count();
	if (!n)
		return n;

	mac_addresses = kmalloc(n * sizeof(*mac_addresses), GFP_KERNEL);
	if (!mac_addresses) {
		pr_err("mac_address buffer failed, count: %u\n", n);
		return 0;
	}

	n = mv_switch_get_peer_mac_addresses(mac_addresses, n, port_no);
	for (i = 0; i < n; ++i) {
		rc = sprintf(buf + len, "%02x:%02x:%02x:%02x:%02x:%02x\n",
					mac_addresses[i][0], mac_addresses[i][1],
					mac_addresses[i][2], mac_addresses[i][3],
					mac_addresses[i][4], mac_addresses[i][5]);
		if (rc > 0)
			len += rc;
	}
	kfree(mac_addresses);
	return len;
}


static struct device_attribute neta_switch[] = {
		__ATTR(carrier, S_IRUGO, mv_carrier_show, NULL),
		__ATTR(speed, S_IRUGO, mv_speed_show, NULL),
		__ATTR(duplex, S_IRUGO, mv_duplex_show, NULL),
		/*can not __ATTR(power) because linux default create the power, can not duplicate */
		__ATTR(power_config, S_IRUSR | S_IWUSR, mv_link_power_show, mv_link_power_set),
		__ATTR(peer_mac_addresses, S_IRUGO, mv_mac_addr_show, NULL),
		NULL
};


static struct class *neta_switch_class;

int mv_switch_sysfs_init(void)
{
	int err;
	struct device *pd;
	int i;

	pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	if (!pd) {
		platform_device_register_simple("neta", -1, NULL, 0);
		pd = bus_find_device_by_name(&platform_bus_type, NULL, "neta");
	}

	if (!pd) {
		pr_err("%s: cannot find neta device\n", __func__);
		pd = &platform_bus;
	}

	pd = &platform_bus;
	err = sysfs_create_group(&pd->kobj, &mv_switch_group);
	if (err) {
		pr_info("sysfs group failed %d\n", err);
		goto out;
	}


	err = alloc_chrdev_region(&base_dev, 0, 8, "neta-switch");
	if (err)
		pr_err("Allocate chrdev failed: %d\n", err);
	else {
		neta_switch_class = class_create(THIS_MODULE, "neta-switch");
		neta_switch_class->dev_attrs = neta_switch;

		for (i = 0; i < 8; ++i) {
			device_create(neta_switch_class, pd,
						MKDEV(MAJOR(base_dev), MINOR(base_dev) + i),
						NULL, "port%d", i);
		}
	}

out:
	return err;
}

module_init(mv_switch_sysfs_init);

MODULE_AUTHOR("Dima Epshtein");
MODULE_DESCRIPTION("sysfs for Marvell switch");
MODULE_LICENSE("GPL");
