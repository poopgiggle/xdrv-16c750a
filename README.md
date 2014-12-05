xdrv-16c750a
============

Xenomai Real-Time driver for 16C750A UARTs found in TI AM335x series MPUs.

Dependencies:
- Linux kernel 3.2.21 with Xenomai v2.6.2.1 (available at: https://github.com/nradulovic/linux-am335x-xeno) - driver was tested with branch: "xeno-var_som_am33-netico-tmod". Other xeno-* branches may work too.

Supported UART modes:
- basic UART mode [supported] - Interrupt mode (DMA transfer is still in development)
- IrDA mode [not supported]
- CIT mode [not supported]


Driver uses Xenomai RTDM API services to exchange data with application. For driver configuration IOCTL system is used.

Before using this driver you MUST disable in-kernel OMAP tty/serial drivers. Unfortunately, there is no easy way to do this except some kernel code hacking.

# Building

Include directories:

    <linux_root>/include
    <linux_root>/include/xenomai [managed by Makefile]
    <driver_root>/inc/port [managed by Makefile]
    <driver_root>/inc/drv [managed by Makefile]
    <driver_root>/inc/dbg [managed by Makefile]
    <driver_root>/inc/circbuff [managed by Makefile]
    <driver_root>/inc/drv [managed by Makefile]
    
Port specific include paths for AM335x

    <linux_root>/arch/arm/plat-omap
    <linux_root>/arch/arm/mach-omap2

When the project is configured do the following in driver root directory:

    make clean
    
and then specify name of the used port, for example:

    make am335x

This will produce kernel module which you can insert with:

    insmod xuart-am335x.ko
    
    
