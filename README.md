# gigabyte-laptop-wmi

This is an experimental kernel driver for Gigabyte Aero/AORUS laptops to
interact with the embedded controller. This allows for fan controls and custom
charging without the need to reboot into Windows.

Gigabyte Gaming models (2024 and older) are not supported. Since they are just
rebadged Clevo laptops (specifically, the NP5x and NP7x series), you can just use
[these drivers](https://github.com/wessel-novacustom/clevo-keyboard/tree/master).

## Overview

Virtually all Gigabyte laptops have most of their sensor data and controls in
the embedded controller (EC), which can only be accessed through Gigabyte's
Control Center on Windows. Because it is implemented as a Windows Management
Interface Device (WMI), interacting with it on Linux is difficult.

This kernel driver enables interaction with the EC via WMI methods `WMBC` and
`WMBD`. The controls are made available through sysfs, while the sensor data
are available through HWMON.

The objective is to eliminate the need to use "hacks" to interact with the EC,
such as calling ACPI directly from userspace (as root) or by loading `ec-sys`
to set specific bits in EC memory ourselves (see [this repository](https://github.com/jertel/p37-ec) and [this fork](https://github.com/christiansteinert/p37-ec-aero-14)).

## Model support

The following models are currently supported:

- All Aero models
- All AORUS models
- Gigabyte Gaming (2025+) models (e.g. A16 GA6H)
- P series models (e.g. P56XT, P34W, P55W)

The following models are not supported:

- All Sabre models
- Gigabyte Gaming (2024 and older, e.g. G5/G7) models
  - Sabre and Gigabyte Gaming models are rebadged Clevo laptops, use [this driver](https://github.com/wessel-novacustom/clevo-keyboard/tree/master) instead.
- All U series models

## Installation/Usage

All information for this have been moved to [INSTALL.md](INSTALL.md) and [USAGE.md](USAGE.md). You can also check the [wiki](https://github.com/tangalbert919/gigabyte-laptop-wmi/wiki) on how to install and use this kernel driver.
