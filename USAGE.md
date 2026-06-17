# Usage

**DISCLAIMER:** I recommend reading this entire document so you are able to use this kernel driver correctly. Incorrect usage may damage your machine.

## Where are the nodes?

All available nodes are found at the following path:

```text
/sys/devices/platform/aorus_laptop
```

You can write to these nodes using `echo` and `tee`. Keep in mind that you must be logged in as `root` or using `sudo` for this.

## Fan modes

Aero/AORUS laptops currently support six fan modes. They are implemented in the kernel driver and recognized in the following order, starting from zero:

- Normal mode
- Silent mode
- Gaming mode
- Custom mode
- Auto mode
- Fixed mode

The last two modes will enable custom mode automatically, as they are considered "custom modes". Custom mode will be automatically disabled if the first three modes are enabled.

**Node:** `/sys/devices/platform/aorus_laptop/fan_mode`

**Example:** To switch to gaming fan mode:

```sh
echo '2' | sudo tee /sys/devices/platform/aorus_laptop/fan_mode
```

## Custom fan speed

Aero/AORUS laptops support setting a custom fan speed. However, this only takes effect if either auto or fixed mode is enabled.

As of version 0.2.0, the accepted range is 0-255. Earlier versions require fulfilling the following conditions:

- The number between 25 and 100.
- The number is divisible by 5.

**Node:** `/sys/devices/platform/aorus_laptop/fan_custom_speed`

**Example:** To set the custom fan speed to 50 percent:

```sh
# Version 0.2.0+
echo '128' | sudo tee /sys/devices/platform/aorus_laptop/fan_custom_speed
# Before version 0.2.0
echo '50' | sudo tee /sys/devices/platform/aorus_laptop/fan_custom_speed
```

## Charging mode

**Disclaimer:** Charging mode (and limit) is not supported on the following models:

- [Aero 14-W/K](https://www.gigabyte.com/Laptop/AERO-14--GTX-970M-965M)
- [Aero 14-W6](https://www.gigabyte.com/Laptop/AERO-14--GTX-1060)
- [Aero 14-W7](https://www.gigabyte.com/Laptop/AERO-14--i7-7700HQ)

Aero/AORUS laptops support two charging modes: Normal (0) and custom (1). The custom charging mode simply stops the laptop from passing its charging limit.

**Node:** `/sys/devices/platform/aorus_laptop/charge_mode`

## Charging limit

Aero/AORUS laptops support a charging limit. Charging mode must be set to custom for it to take effect. It will only accept numbers between 60 and 100.

**Node:** `/sys/devices/platform/aorus_laptop/charge_limit`

**Example:** To set the charging limit to 80 percent:

```text
echo '80' | sudo tee /sys/devices/platform/aorus_laptop/charge_limit
```

## Fan curve data (added in version 0.1.0)

Aero/AORUS laptops support setting a custom fan curve. Custom fan mode must be enabled for it to take effect.

**Nodes:**

```text
/sys/devices/platform/aorus_laptop/fan_curve_index
/sys/devices/platform/aorus_laptop/fan_curve_data
```

The index node specifies which index in the fan curve can be modified, while the data node holds the temperature (in Celsius) and fan speed for that index, printed in that order (for readability). There are 255 indices available, but only 15 can be modified. This is to ensure compatibility with Gigabyte's Control Center software on dual-boot systems.

Each index should contain both temperature and fan speed in strictly non-decreasing order. The temperature can be any number between 0 and 100, and the fan speed can be any number between 0 and 255.

You must set the index node to the index you wish to modify first. The data node can only take a single 16-bit number, so you must combine your specified temperature and fan speed first, in reverse order. An easy way to do this is to multiply the fan speed by 256, and then add the temperature to the result.

**Example:** To set index 2 to half fan speed at 55 degrees Celsius:

```sh
echo '2' | sudo tee /sys/devices/platform/aorus_laptop/fan_curve_index
# 127*256+55 = 32512+55 = 32567
echo '32567' | sudo tee /sys/devices/platform/aorus_laptop/fan_curve_data
```

## Battery cycle (added in version 0.1.0)

Aero/AORUS laptops support battery cycles, but are only accessible through the embedded controller. Older models are likely to read 0 due to older Gigabyte firmware. Because there are two different battery cycle numbers, only the highest one is printed. This node is read-only.

**Node:** `/sys/devices/platform/aorus_laptop/battery_cycle`

## GPU boost (added in version 0.1.0)

**Disclaimer:** Models older than the [Aero 15 X9 Series](https://www.gigabyte.com/Laptop/AERO-15--RTX-20-Series) do not support this, as it requires NVIDIA's Dynamic Boost from their Max-Q technologies.

Aero/AORUS laptops support boosting the discrete GPU's power limit. Even though this is controlled by `nvidia-powerd`, the embedded controller can control this as well.

Laptops with RTX 3000 series GPUs or newer have three different modes, while older models have one.

**Node:** `/sys/devices/platform/aorus_laptop/gpu_boost`

**Example:** To enable GPU boost:

```sh
echo '1' | sudo tee /sys/devices/platform/aorus_laptop/gpu_boost
```

## USB toggles (added in version 0.1.0)

Aero/AORUS laptops support USB power output when they are asleep (S3) or in hibernation (S4). Newer models have dropped the latter, and will return 0 by default. These toggles are currently read-only.

**Nodes:**

```text
/sys/devices/platform/aorus_laptop/usb_charge_s3_toggle
/sys/devices/platform/aorus_laptop/usb_charge_s4_toggle
```

## Power on time (added in version 0.2.0)

Aero/AORUS laptops support "power on time", though its use is unknown. This node is read-only.

**Node:** `/sys/devices/platform/aorus_laptop/power_on_time`

## Light sensor (added in version 0.2.0)

Aero/AORUS laptops support light sensors, but it's only equipped on some models. If no light sensor is installed, it will simply return `0` by default. This node is read-only.

All VE and newer models (e.g. Aero 16 XE5) return four 8-bit values, while older models (e.g. Aero 15 VD) return a single 32-bit value. To calculate the sensor value on newer models, ignore the first value and combine the last three in reverse order using bit shift.

```text
# Format: 247 [low] [medium] [high]
# Actual value (bit shift): [high] << 16 | [medium] << 8 | [low]
# Actual value (arithmetic): [high]*65536 + [medium]*256 + [low]
```

**Node:** `/sys/devices/platform/aorus_laptop/light_sensor`

## PWM (added in version 0.2.0)

**Disclaimers:**

- Models older than the [Aero 15 X9 Series](https://www.gigabyte.com/Laptop/AERO-15--RTX-20-Series) do not support this.
- Models older than the [Aero 15 KB Series](https://www.gigabyte.com/Laptop/AERO-15--Intel-10th-Gen) use the same value for all fans.

Aero/AORUS laptops support reading the current PWM for each fan. This should not be confused with the custom fan speed, which returns the PWM for use in custom fan mode.

The sysfs and HWMON nodes are read-only, but support for writing to HWMON nodes will be added in the future. The sysfs node will only return the PWM for the CPU fan, while the HWMON nodes have separate channels for CPU and GPU fans.

**Node:** `/sys/devices/platform/aorus_laptop/fan_pwm`
