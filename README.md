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

- Aero 15 Classic (SA/WA/XA/YA)

## Adding a new model

TODO

## How to install

```
make
sudo insmod gigabyte-laptop.ko
```

## How to remove
```
sudo rmmod gigabyte_laptop
```
