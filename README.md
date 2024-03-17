# gigabyte-laptop-wmi

This is an experimental kernel driver for Gigabyte laptops to interact with
the embedded controller.

## Overview

Virtually all Gigabyte laptops have most of their sensor data and controls in
the embedded controller (EC), which can only be accessed through Gigabyte's
Control Center on Windows. Because it is implemented as a WMI device,
interacting with it on Linux is difficult.

This kernel driver enables interaction with the EC via WMI methods `WMBC` and
`WMBD`, which are known to be present in virtually all Aero and AORUS models.
The controls are available through sysfs, while the sensor data are available
through HWMON.

## Supported models

Tested:
- Aero 15 Classic (SA/WA/XA/YA)

Untested:
- All other Aero models (at least as far back as Aero 14 W)
- Some AORUS models (at least from AORUS 15 (2019) onwards)

## Unsupported models
- All Sabre models (retired in 2018)
- All Gigabyte Gaming models (e.g. G7)
- U series
- P series models (e.g. P56XT, P34W)


## Installation/Usage

All information for this have been moved to [INSTALL.md](INSTALL.md) and [USAGE.md](USAGE.md). You can also check the [wiki](https://github.com/tangalbert919/gigabyte-laptop-wmi/wiki) on how to install and use this kernel driver.
