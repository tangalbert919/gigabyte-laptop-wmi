# Usage

**DISCLAIMER:** I recommend reading this entire document so you are able to use this kernel driver correctly. Incorrect usage may damage your machine.

## Where are the nodes?
All available nodes are found at the following path:
```
/sys/devices/platform/aorus_laptop
```
You can write to these nodes using `echo` and `tee`. Keep in mind that you must be logged in as `root` or using `sudo` for this.

## Fan modes

**Disclaimer:** The kernel driver currently cannot enable silent fan mode on the following models:
* [Aero 14-W/K](https://www.gigabyte.com/Laptop/AERO-14--GTX-970M-965M)
* [Aero 14-W6](https://www.gigabyte.com/Laptop/AERO-14--GTX-1060)
* [Aero 14-W7](https://www.gigabyte.com/Laptop/AERO-14--i7-7700HQ)
* [Aero 14-K8](https://www.gigabyte.com/us/Laptop/AERO-14--i7-8750H)

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
```
echo '2' | sudo tee /sys/devices/platform/aorus_laptop/fan_mode
```

## Custom fan speed
Aero/AORUS laptops support setting a custom fan speed. However, this only takes effect if either auto or fixed mode is enabled.

The kernel driver only supports numbers with the following requirements:
- It is between 25 and 100.
- It is divisible by 5.

**Node:** `/sys/devices/platform/aorus_laptop/fan_custom_speed`

**Example:** To set the custom fan speed to 50 percent:
```
echo '50' | sudo tee /sys/devices/platform/aorus_laptop/fan_custom_speed
```

## Charging mode
**Disclaimer:** Charging mode (and limit) is not supported on the following models:
* [Aero 14-W/K](https://www.gigabyte.com/Laptop/AERO-14--GTX-970M-965M)
* [Aero 14-W6](https://www.gigabyte.com/Laptop/AERO-14--GTX-1060)
* [Aero 14-W7](https://www.gigabyte.com/Laptop/AERO-14--i7-7700HQ)

Aero/AORUS laptops support two charging modes: Normal (0) and custom (1). The custom charging mode simply stops the laptop from passing its charging limit.

**Node:** `/sys/devices/platform/aorus_laptop/charge_mode`

## Charging limit

Aero/AORUS laptops support a charging limit. Charging mode must be set to custom for it to take effect. It will only accept numbers between 60 and 100.

**Node:** `/sys/devices/platform/aorus_laptop/charge_limit`

**Example:** To set the charging limit to 80 percent:
```
echo '80' | sudo tee /sys/devices/platform/aorus_laptop/charge_limit
```
