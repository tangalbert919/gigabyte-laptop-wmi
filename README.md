# gigabyte-laptop-wmi

This is an experimental kernel driver for Gigabyte Aero/AORUS laptops to
interact with the embedded controller. This allows for fan controls and custom
charging without the need to reboot into Windows.

## Overview

This kernel driver enables interaction with the embedded controller as a WMI
device via methods `WMBC` and `WMBD`. The controls are available through sysfs,
while the sensor data are available through HWMON. In this way, using "hacks"
to interact with the controller, such as calling ACPI directly from userspace
(as root) or by loading `ec-sys` to set specific bits in its memory ourselves
(see [this repository](https://github.com/jertel/p37-ec) and
[this fork](https://github.com/christiansteinert/p37-ec-aero-14)) are no longer
required.

## Model support

The following models are currently supported:

- All Aero models
- All AORUS models
- Gigabyte Gaming (2025+) models (e.g. A16 GA6H)
- Some P-series models (e.g. P56XT, P34W, P55W)

The following models are not supported:

- All Sabre models
- Gigabyte Gaming (2024 and older, e.g. G5/G7) models
  - Sabre and Gigabyte Gaming models are rebadged Clevo laptops, use [this driver](https://github.com/wessel-novacustom/clevo-keyboard/tree/master) instead.
- All U series models

## Installation/Usage

Refer to [INSTALL.md](INSTALL.md) and [USAGE.md](USAGE.md). You can also check
the [wiki](https://github.com/tangalbert919/gigabyte-laptop-wmi/wiki) on how to
install and use this kernel driver.
