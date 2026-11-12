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

As of version 0.2.0, the accepted range is 0-229. Earlier versions require fulfilling the following conditions:

- The number between 25 and 100.
- The number is divisible by 5.

**Node:** `/sys/devices/platform/aorus_laptop/fan_custom_speed`

**Example:** To set the custom fan speed to 50 percent:

```sh
# Version 0.2.0+
echo '114' | sudo tee /sys/devices/platform/aorus_laptop/fan_custom_speed
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

## Display mux (GIGABYTE AERO X16 2WH only)

The two nodes in this section are enabled **only on the GIGABYTE AERO X16 2WH**, the one model they were tested on (two units, BIOS FB05 and FB06). The driver matches the exact product name. On every other model both nodes return "No such device".

**Models we assume to work the same, without any guarantee.** We have not tested them and the driver does not enable the nodes there:

| Model | Why we assume it | What is known |
|---|---|---|
| AERO X16 1WH | Gigabyte ships one firmware image for all four AERO X16 variants; the code that routes the panel is in that shared image | The stored mode (`gpu_mux_mode` below) has been used on this model by others, including a boot in Discrete mode. `panel_mux` has not been tried. |
| AERO X16 1TH | same shared firmware image | nothing tested |
| AERO X16 1VH | same shared firmware image | nothing tested |

For any other Aero or AORUS model we know nothing. To try the nodes on a model that is not enabled, load the module with `force_display_mux=1`. `panel_mux` then writes one bit of the embedded controller directly, and the same bit may do something else on another model. If you try it, please report what happened, with model name and BIOS version.

### GPU mux mode

The Aero X16 has a display mux in front of the internal panel. The firmware stores a display mode and applies it at power-on, so a change only takes effect after a reboot. The modes are named after Gigabyte's own software:

- `0`: Dynamic (on the Aero X16 2WH, the panel stays on the integrated GPU)
- `1`: Discrete (the panel is on the discrete GPU)
- `2`: MsHybrid

Discrete mode changes which GPU owns the internal panel from power-on. The Type-C display outputs stay on the integrated GPU. Make sure your system can show a picture through the discrete GPU before you reboot into this mode.

Reading this node was tested (both units read `0`). Writing it and booting in another mode was **not** tested by us.

**Node:** `/sys/devices/platform/aorus_laptop/gpu_mux_mode`

**Example:** To use discrete mode from the next boot:

```sh
echo '1' | sudo tee /sys/devices/platform/aorus_laptop/gpu_mux_mode
```

### Panel mux

The Aero X16 2WH can also switch the display mux while the system is running. This goes through the embedded controller, not through the stored mode above: `0` puts the internal panel on the integrated GPU, and `1` puts it on the discrete GPU. The switch is immediate (the write takes about 10 ms).

This is meant for handing the panel to a discrete GPU that the running system does not use itself, e.g. one that is passed through to a virtual machine, and for taking it back afterwards. Keep the following in mind:

- Neither GPU gets a hot-plug signal. The GPU that receives the panel has to re-train its link before a picture appears (switch its output off and on, or restart its graphics driver). Expect one to two seconds without a picture.
- If the discrete GPU is not driving its internal display output, the panel shows nothing while this is set to `1`. Have a way to write `0` again without seeing the screen (a key binding, a timer, SSH).
- The firmware resets this from the stored GPU mux mode at every boot.

**Node:** `/sys/devices/platform/aorus_laptop/panel_mux`

**Example:** To route the internal panel to the discrete GPU, and back:

```sh
echo '1' | sudo tee /sys/devices/platform/aorus_laptop/panel_mux
echo '0' | sudo tee /sys/devices/platform/aorus_laptop/panel_mux
```
