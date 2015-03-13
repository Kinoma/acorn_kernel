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

obj-y	+= common/mvCommon.o common/mvStack.o common/mvDebug.o

obj-$(CONFIG_MV_ETH_PP2_1)		+= common/mvList.o

obj-$(CONFIG_MV_ETH_INCLUDE_PHY)	+= phy/mvEthPhy.o phy/phy_sysfs.o

obj-$(CONFIG_MV_ETH_INCLUDE_NETMUX)	+= netmux/

obj-$(CONFIG_MV_ETH_NETA) += neta/
obj-$(CONFIG_MV_ETH_PP2) += pp2/
ifeq ($(CONFIG_MV_INCLUDE_SWITCH),y)
DSDT_OBJS		= $(QD_DIR)/src/driver/gtDrvConfig.o $(QD_DIR)/src/driver/gtDrvEvents.o \
	                  $(QD_DIR)/src/driver/gtHwCntl.o $(QD_DIR)/src/platform/gtMiiSmiIf.o	\
	                  $(QD_DIR)/src/platform/platformDeps.o $(QD_DIR)/src/platform/gtSem.o	\
	                  $(QD_DIR)/src/platform/gtDebug.o $(QD_DIR)/src/msapi/gtBrgFdb.o 	\
	                  $(QD_DIR)/src/msapi/gtBrgStp.o $(QD_DIR)/src/msapi/gtBrgVlan.o 	\
	                  $(QD_DIR)/src/msapi/gtEvents.o $(QD_DIR)/src/msapi/gtPortCtrl.o 	\
	                  $(QD_DIR)/src/msapi/gtPortStat.o $(QD_DIR)/src/msapi/gtPortStatus.o 	\
	                  $(QD_DIR)/src/msapi/gtQosMap.o $(QD_DIR)/src/msapi/gtPIRL.o  		\
	                  $(QD_DIR)/src/msapi/gtPhyCtrl.o $(QD_DIR)/src/msapi/gtPhyInt.o 	\
	                  $(QD_DIR)/src/msapi/gtSysConfig.o $(QD_DIR)/src/msapi/gtSysCtrl.o	\
	                  $(QD_DIR)/src/msapi/gtVersion.o $(QD_DIR)/src/msapi/gtUtils.o 	\
	                  $(QD_DIR)/src/msapi/gtBrgVtu.o $(QD_DIR)/src/msapi/gtPortRmon.o 	\
	                  $(QD_DIR)/src/msapi/gtSysStatus.o $(QD_DIR)/src/msapi/gtPortRateCtrl.o\
	                  $(QD_DIR)/src/msapi/gtPortPav.o $(QD_DIR)/src/msapi/gtVct.o		\
					  $(QD_DIR)/src/msapi/gtPIRL2.o $(QD_DIR)/src/msapi/gtCCPVT.o		\
					  $(QD_DIR)/src/msapi/gtPCSCtrl.o $(QD_DIR)/src/msapi/gtWeight.o	\
					  $(QD_DIR)/src/msapi/gtBrgStu.o

obj-$(CONFIG_MV_INCLUDE_SWITCH) += $(DSDT_OBJS)
obj-$(CONFIG_MV_INCLUDE_SWITCH)	+= switch/mv_switch.o switch/mv_switch_sysfs.o switch/mv_phy.o
endif
