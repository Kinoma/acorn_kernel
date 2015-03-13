# Makefile for mvebu_net Marvell network drivers


CPU_ARCH    = ARM
ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
ENDIAN      = BE
else
ENDIAN      = LE
endif

MVEBU_NET_FLAGS := -DMV_LINUX -DMV_CPU_$(ENDIAN) -DMV_$(CPU_ARCH)
export MVEBU_NET_FLAGS

ccflags-y       += $(MVEBU_NET_FLAGS)

PLAT_DIR 	:= drivers/net/ethernet/mvebu_net
export PLAT_DIR

INCLUDE_DIRS	+= -I$(PLAT_DIR)
INCLUDE_DIRS	+= -I$(PLAT_DIR)/common
INCLUDE_DIRS	+= -I$(srctree)/arch/arm/mach-mvebu/linux_oss
INCLUDE_DIRS	+= -I$(PLAT_DIR)/switch
INCLUDE_DIRS	+= -I$(PLAT_DIR)/netmux
INCLUDE_DIRS	+= -I$(PLAT_DIR)/phy
INCLUDE_DIRS	+= -I$(srctree)/arch/arm/mach-mvebu
ifeq ($(CONFIG_MV_INCLUDE_SWITCH),y)
INCLUDE_DIRS   += -I$(PLAT_DIR)/$(QD_DIR)/Include
INCLUDE_DIRS   += -I$(PLAT_DIR)/$(QD_DIR)/Include/h/msApi
INCLUDE_DIRS   += -I$(PLAT_DIR)/$(QD_DIR)/Include/h/driver
INCLUDE_DIRS   += -I$(PLAT_DIR)/$(QD_DIR)/Include/h/platform
endif
export INCLUDE_DIRS

ifeq ($(CONFIG_MV_INCLUDE_SWITCH),y)
QD_DIR:= switch/qd-dsdt-3.3
export QD-DIR
endif

ccflags-y	+= $(INCLUDE_DIRS)


obj-$(CONFIG_MV_ETH_INCLUDE_NETMUX)	+= netmux/

obj-$(CONFIG_MV_ETH_NETA) += neta/
obj-$(CONFIG_MV_ETH_PP2) += pp2/
obj-y += common/
obj-y += switch/qd-dsdt-3.3/src/driver/
obj-y += switch/qd-dsdt-3.3/src/msapi/
obj-y += switch/qd-dsdt-3.3/src/platform/
obj-y += switch/
obj-y += phy/
