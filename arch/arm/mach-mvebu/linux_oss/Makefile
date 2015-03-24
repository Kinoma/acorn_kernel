ifeq ($(CONFIG_CPU_BIG_ENDIAN),y)
ENDIAN      = BE
else
ENDIAN      = LE
endif

ccflags-$(CONFIG_ARCH_MULTIPLATFORM) := -I$(srctree)/$(src)/include \
	-I$(srctree)/arch/arm/plat-orion/include

AFLAGS_coherency_ll.o		:= -Wa,-march=armv7-a

obj-y	+= mvOs.o

COMMON_DIR	:= $(srctree)/arch/arm/mach-mvebu/include/mach
ccflags-y	+= -I$(COMMON_DIR) -DMV_CPU_$(ENDIAN)
