xdrv-16c750a
============

Xenomai Real-Time driver for 16C750A UARTs found in TI AM335x series MPUs.

Dependencies:
- Linux kernel 3.8.13 with Xenomai (I got mine from: https://github.com/cdsteinkuehler/linux-dev)
  That kernel is specifically patched and configured to work with TI's BeagleBone system; if you're developing
  for a different platform, you may need to modify the code to support your needs.

Supported UART modes:
- basic UART mode [supported] - Interrupt mode (DMA transfer is still in development)
- IrDA mode [not supported]
- CIT mode [not supported]


Driver uses Xenomai RTDM API services to exchange data with application. For driver configuration IOCTL system is used.

Before using this driver you MUST disable in-kernel OMAP tty/serial drivers. Unfortunately, there is no easy way to do this except some kernel code hacking.

# Building

First, you will need to modify the Makefile so that LINUX_SRC points to your kernel source tree.

Next, you need to disable the default OMAP serial drivers in the Linux kernel and export some functions from $(LINUX_SRC)/arch/arm/mach-omap2/omap_device.c
and $(LINUX_SRC)/arch/arm/mach-omap2/omap_hwmod.c. The included patch xenomai-uart.patch works for my particular source tree. The functions that need to
be exported are:

    omap_device_disable_clocks
    omap_device_delte
    omap_device_enable_clocks
    omap_hwmod_lookup
    omap_device_shutdown
    omap_device_build
    omap_device_enable
    omap_device_get_rt_va

You'll also need to add the line #include <linux/export.h> to the top of omap_hwmod.c

When the project is configured do the following in driver root directory:

    make clean
    
and then specify name of the used port, for example:

    make am335x

This will produce kernel module which you can insert with:

    insmod xuart-am335x.ko
    
    
