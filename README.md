xdrv-16c750a
============

Xenomai Real-Time driver for 16C750A UARTs found in TI AM335x series MPUs.

Dependencies:
- Xenomai v2.6.2.1

Supported UART modes:
- basic UART mode [supported]
- IrDA mode [not supported]
- CIT mode [not supported]


Driver uses Xenomai message queue services to exchange data with application. For driver configuration IOCTL system is used, but in the near future this may change.

Before using this driver you must disable in-kernel omap drivers.
