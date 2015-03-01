ifeq ($(CHOST),)
CHOST := armv6j-hardfloat-linux-gnueabi
endif
CC := $(CHOST)-gcc
CXX := $(CHOST)-g++
LD := $(CHOST)-ld
SYSROOT := /usr/$(CHOST)
PKG_CONFIG_SYSROOT_DIR := $(SYSROOT)
PKG_CONFIG_PATH := $(PKG_CONFIG_SYSROOT_DIR)/usr/lib/pkgconfig
PKG_CONFIG := PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config

OPTFLAGS += -Ofast -pipe -march=armv6j -mfpu=vfp -mfloat-abi=hard
WFLAGS += -Wall -Wextra -Wcast-qual -Wconversion -Wdisabled-optimization -Wdouble-promotion -Wmissing-declarations -Wpacked -Wno-parentheses -Wredundant-decls -Wno-sign-conversion -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wno-vla
CFLAGS += -std=c99 $(OPTFLAGS) $(WFLAGS)
CXXFLAGS += -std=c++11 $(OPTFLAGS) $(WFLAGS) -Wnoexcept -Wzero-as-null-pointer-constant
LDFLAGS += -Wl,--as-needed

KERNEL_DIR := ../linux
VMLINUX := $(KERNEL_DIR)/vmlinux
INSTALL_MOD_PATH := /mnt/raspi
KERNEL_MAKE_OPTS := -C '$(KERNEL_DIR)' \
	ARCH=arm CFLAGS='' LDFLAGS='' \
	arch-y='-D__LINUX_ARM_ARCH__=6 -march=armv6j -fno-stack-protector' tune-y='' \
	INSTALL_MOD_PATH='$(INSTALL_MOD_PATH)' M='$(shell pwd)'

.PHONY : all clean install

all : gpio_input.ko dump_events

clean :
	$(MAKE) $(KERNEL_MAKE_OPTS) clean
	rm -f dump_events

install : all
	$(MAKE) $(KERNEL_MAKE_OPTS) modules_install

$(VMLINUX) :
	$(MAKE) $(KERNEL_MAKE_OPTS) CROSS_COMPILE='$(CHOST)-' M='' vmlinux

gpio_input.ko : gpio_input.c $(VMLINUX)
	$(MAKE) $(KERNEL_MAKE_OPTS) obj-m=gpio_input.o modules
