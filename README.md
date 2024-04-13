# gigabyte-laptop-wmi

This is an experimental kernel driver for Gigabyte laptops to interact with
the embedded controller.

## Overview

Virtually all Gigabyte laptops have most of their sensor data and controls in
the embedded controller (EC), which can only be accessed through Gigabyte's
Control Center on Windows. Because it is implemented as a WMI device,
interacting with it on Linux is difficult.

This kernel driver enables interaction with the EC via WMI methods:
* On Aero/AORUS models, there are two: `WMBC` and `WMBD`.
* On Sabre/Gigabyte Gaming models, there is one: `WMBB`. (Currently not supported)

The controls are available through sysfs, while the sensor data are available
through HWMON.

## Model support

The following models are currently supported:
- All Aero 15/15X models made after 2018 (Intel Core i7-8750H or newer)
- All Aero 17 models
- The Aero 14 OLED (2023)
- All AORUS models

The following models are compatible, but **not** yet supported:
- All Aero 14 models made before 2019 (see issue [TODO: Create issue regarding old Aero 14 models])
- P series models (e.g. P56XT, P34W, P55W)

The following models are not supported:
- All Sabre models (retired in 2018)
- All Gigabyte Gaming models (e.g. G7, replaced Sabre series)
- All U series models

## Installation/Usage

All information for this have been moved to [INSTALL.md](INSTALL.md) and [USAGE.md](USAGE.md). You can also check the [wiki](https://github.com/tangalbert919/gigabyte-laptop-wmi/wiki) on how to install and use this kernel driver.
