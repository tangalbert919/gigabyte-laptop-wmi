// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  aorus-laptop.c - AORUS laptop WMI driver
 *
 *  Copyright (C) 2023 Albert Tang
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/wmi.h>

#define GIGABYTE_LAPTOP_VERSION "0.01"
#define GIGABYTE_LAPTOP_FILE  KBUILD_MODNAME

MODULE_AUTHOR("Albert Tang");
MODULE_DESCRIPTION("Gigabyte laptop WMI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(GIGABYTE_LAPTOP_VERSION);

/* _SB_.PCI0.AMW0._WDG */
#define WMI_EVENT "ABBC0F72-8EA1-11D1-00A0-C90629100000" // Hopefully, it's used for hotkeys
#define WMI_METHOD_WMBC "ABBC0F6F-8EA1-11D1-00A0-C90629100000" // Seems to only return values
#define WMI_METHOD_WMBD "ABBC0F75-8EA1-11D1-00A0-C90629100000" // Will probably do most of the work.

/* WMI method arguments */
// Not supported by Aero 14 W
#define GPU_QBOOST       0x51
#define FAN_SILENT_MODE  0x57
#define CHARGING_MODE    0x64
#define CHARGING_LIMIT   0x65
// Supported by Aero 14 W
#define FAN_CUSTOM_MODE  0x67
#define FAN_INDEX_VALUE  0x68
#define FAN_FIXED_MODE   0x6A
#define FAN_CUSTOM_SPEED 0x6B
#define BATT_CYCLE2      0x6D
#define BATT_CYCLE       0x6E
#define FAN_AUTO_MODE    0x70
#define FAN_GAMING_MODE  0x71
#define USB_SLEEP        0x7A
#define USB_HIBERNATE    0x7B
#define WIFI_TOGGLE      0xC2
#define TOUCHPAD_ENABLED 0xCA
#define TEMP_CPU         0xE1
#define TEMP_GPU         0xE2
#define FAN_CPU_RPM      0xE4
#define FAN_GPU_RPM      0xE5
#define FAN_THREE_RPM    0xE8 // 2023 AORUS 17
#define FAN_FOUR_RPM     0xE9 // 2023 AORUS 17X
#define FAN_SILENT_OLD   0xFA // Older Aero and P-series models

// Fan curves
#define FAN_CURVE_POINTS 15

struct fan_curve_data {
	u8 temperature[FAN_CURVE_POINTS];
	u8 speed[FAN_CURVE_POINTS];
};

struct gigabyte_laptop_wmi {
	struct platform_device *pdev;
	struct device *hwmon_dev;
	int fan_mode;
	int fan_custom_display_speed;
	int fan_custom_internal_speed;
	int charge_mode;
	int charge_limit;
	int gpu_boost;
	u8 fan_silent_method;
	struct fan_curve_data fan_curve;
	int fan_curve_index;
};

static struct platform_device *platform_device;

static u8 fan_modes[] = {
	0,
	FAN_SILENT_MODE,
	FAN_GAMING_MODE,
	FAN_CUSTOM_MODE,
	FAN_AUTO_MODE,
	FAN_FIXED_MODE
};

/* WMI methods ********************************************/

/* WMBC method (checks value in EC) */
static int gigabyte_laptop_get_devstate2(u32 method_id, u32 arg2, int *result)
{
	union acpi_object *obj;
	acpi_status status;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_buffer input = { sizeof(arg2), &arg2 };

	status = wmi_evaluate_method(WMI_METHOD_WMBC, 0, method_id, &input, &buffer);
	if (ACPI_FAILURE(status))
		return -1;

	obj = buffer.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER)
		*result = obj->integer.value;
	else {
		kfree(obj);
		return -EINVAL;
	}
	kfree(obj);
	return 0;
}

static int gigabyte_laptop_get_devstate(u32 method_id, int *result) {
	return gigabyte_laptop_get_devstate2(method_id, 0, result);
}

/* WMBD method (sets value in EC) */
static int gigabyte_laptop_set_devstate(u32 method_id, u32 arg2, int *result)
{
	union acpi_object *obj;
	acpi_status status;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_buffer input = { sizeof(arg2), &arg2 };

	status = wmi_evaluate_method(WMI_METHOD_WMBD, 0, method_id, &input, &buffer);
	if (ACPI_FAILURE(status))
		return -1;

	obj = buffer.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER)
		*result = obj->integer.value;
	else {
		kfree(obj);
		return -EINVAL;
	}
	kfree(obj);
	return 0;
}

/* hwmon **************************************************/

/*
 * Helper method. Reverses byte order of fan RPM.
 * This is needed, since the embedded controller stores the value in big-endian
 * while x86 is little-endian.
 */
static u16 convert_fan_rpm(int val)
{
	u16 fan_rpm = val;
	return rol16(fan_rpm, 8);
}

static umode_t gigabyte_laptop_hwmon_is_visible(const void *data, enum hwmon_sensor_types type,
					u32 attr, int channel)
{
	switch (type) {
		case hwmon_temp:
			switch (attr) {
				case hwmon_temp_input:
					return 0444;
				default:
					break;
			}
			break;
		case hwmon_fan:
			switch (attr) {
				case hwmon_fan_input:
					return 0444;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return 0;
}

static int gigabyte_laptop_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
					u32 attr, int channel, long *val)
{
	int ret, output;
	u8 result;
	u8 fan_channels[] = { FAN_CPU_RPM, FAN_GPU_RPM, FAN_THREE_RPM, FAN_FOUR_RPM };

	switch (type) {
		case hwmon_temp:
			switch (channel) {
				case 0:
					ret = gigabyte_laptop_get_devstate(TEMP_CPU, &output);
					if (ret)
						break;
					*val = output * 1000;
					break;
				case 1:
					ret = gigabyte_laptop_get_devstate(TEMP_GPU, &output);
					if (ret)
						break;
					*val = output * 1000;
					break;
				case 2:
					// Motherboard temp cannot be read through WMI
					ret = ec_read(0x62, &result);
					if (ret)
						break;
					*val = result * 1000;
					break;
				default:
					*val = 0;
					break;
			}
			break;
		case hwmon_fan:
			ret = gigabyte_laptop_get_devstate(fan_channels[channel], &output);
			if (ret)
				break;
			*val = convert_fan_rpm(output);
			break;
		default:
			break;
	}
	return 0;
}

static const struct hwmon_channel_info *gigabyte_laptop_hwmon_info[] = {
	HWMON_CHANNEL_INFO(temp,
				HWMON_T_INPUT,
				HWMON_T_INPUT,
				HWMON_T_INPUT),
	HWMON_CHANNEL_INFO(fan,
				HWMON_F_INPUT,
				HWMON_F_INPUT,
				HWMON_F_INPUT,
				HWMON_F_INPUT),
	NULL
};

static const struct hwmon_ops gigabyte_laptop_hwmon_ops = {
	.read = gigabyte_laptop_hwmon_read,
	.is_visible = gigabyte_laptop_hwmon_is_visible,
};

static const struct hwmon_chip_info gigabyte_laptop_chip_info = {
	.ops = &gigabyte_laptop_hwmon_ops,
	.info = gigabyte_laptop_hwmon_info,
};

/* sysfs **************************************************/

/*
 * Fan mode.
 * 0 = normal fan mode
 * 1 = silent fan mode
 * 2 = gaming fan mode
 * 3 = custom fan mode
 * 4 = auto-maximum mode (requires custom mode)
 * 5 = fixed speed mode (requires custom mode)
 */
static int disable_custom_fan_mode(int mode)
{
	int ret, result;

	if (mode == 5) {
		ret = gigabyte_laptop_set_devstate(FAN_FIXED_MODE, 0, &result);
		if (ret)
			return ret;
	} else if (mode == 4) {
		// Auto-maximum mode can only be turned off through gaming or silent mode
		ret = gigabyte_laptop_set_devstate(FAN_GAMING_MODE, 0, &result);
		if (ret)
			return ret;
	}

	ret = gigabyte_laptop_set_devstate(FAN_CUSTOM_MODE, 0, &result);
	if (ret)
		return ret;

	return 0;
}

static int set_fan_mode(struct gigabyte_laptop_wmi *gigabyte, u32 fan_mode)
{
	int ret, result;

	if (fan_mode == FAN_FIXED_MODE || fan_mode == FAN_AUTO_MODE) {
		if (gigabyte->fan_mode < 3) { // If custom mode is off, enable it
			if (gigabyte->fan_mode > 0) {
				ret = gigabyte_laptop_set_devstate(fan_modes[gigabyte->fan_mode], 0, &result);
				if (ret)
					return ret;
			}

			ret = gigabyte_laptop_set_devstate(FAN_CUSTOM_MODE, 1, &result);
			if (ret)
				return ret;
		}

		if (gigabyte->fan_mode > 3) { // Fixed or auto mode active
			if (gigabyte->fan_mode == 4) {
				// Auto-maximum mode can only be turned off through gaming or silent mode
				ret = gigabyte_laptop_set_devstate(FAN_GAMING_MODE, 0, &result);
				if (ret)
					return ret;
			} else {
				ret = gigabyte_laptop_set_devstate(fan_modes[gigabyte->fan_mode], 0, &result);
				if (ret)
					return ret;
			}
		}

		if (fan_mode == FAN_AUTO_MODE) {
			ret = gigabyte_laptop_set_devstate(fan_mode, gigabyte->fan_custom_internal_speed, &result);
			if (ret)
				return ret;
		} else {
			ret = gigabyte_laptop_set_devstate(fan_mode, 1, &result);
			if (ret)
				return ret;
		}
	} else if (fan_mode == FAN_CUSTOM_MODE) {
		if (gigabyte->fan_mode > 3) {
			pr_warn("Custom mode is already enabled\n");
			return 0;
		} else if (gigabyte->fan_mode > 0) {
			ret = gigabyte_laptop_set_devstate(fan_modes[gigabyte->fan_mode], 0, &result);
			if (ret)
				return ret;
		}

		ret = gigabyte_laptop_set_devstate(FAN_CUSTOM_MODE, 1, &result);
		if (ret)
			return ret;
	} else {
		if (gigabyte->fan_mode >= 3) { // Disable custom mode first. Will revert to normal mode.
			ret = disable_custom_fan_mode(gigabyte->fan_mode);
			if (ret)
				return ret;
		} else if (gigabyte->fan_mode > 0) {
				ret = gigabyte_laptop_set_devstate(fan_modes[gigabyte->fan_mode], 0, &result);
				if (ret)
					return ret;
		}

		if (fan_mode != 0) {
			ret = gigabyte_laptop_set_devstate(fan_mode, 1, &result);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static ssize_t fan_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gigabyte->fan_mode);
}

static ssize_t fan_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int fan_mode = 0;
	struct gigabyte_laptop_wmi *gigabyte;

	ret = kstrtouint(buf, 0, &fan_mode);
	if (ret) {
		pr_err("kstrtouint failed\n");
		return count;
	}

	gigabyte = dev_get_drvdata(dev);

	if (gigabyte->fan_mode == fan_mode) {
		pr_warn("Already set to that fan mode\n");
		return count;
	}

	if (fan_mode > 5) {
		pr_err("Invalid fan mode\n");
		return -EINVAL;
	} else {
		ret = set_fan_mode(gigabyte, fan_modes[fan_mode]);
		if (ret)
			return ret;
	}

	gigabyte->fan_mode = fan_mode;
	return count;
}

/*
 * Custom fan speed. Only works if custom mode is enabled.
 * Must be in multiples of five, between 25 and 100.
 */
static ssize_t fan_custom_speed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gigabyte->fan_custom_display_speed);
}

static ssize_t fan_custom_speed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, output;
	unsigned int speed;
	u8 real_speed;
	struct gigabyte_laptop_wmi *gigabyte;

	ret = kstrtouint(buf, 0, &speed);
	if (ret)
		return ret;

	if ((speed < 25 || speed > 100) || speed % 5 != 0) {
		pr_warn("Invalid custom fan speed: Must be a multiple of 5 and between 25 and 100\n");
		return -EINVAL;
	}

	if (speed == 25)
		real_speed = 0x39;
	else if (speed == 30)
		real_speed = 0x44;
	else if (speed == 35)
		real_speed = 0x50;
	else if (speed == 40)
		real_speed = 0x5B;
	else if (speed == 45)
		real_speed = 0x67;
	else if (speed == 50)
		real_speed = 0x72;
	else if (speed == 55)
		real_speed = 0x7D;
	else if (speed == 60)
		real_speed = 0x89;
	else if (speed == 65)
		real_speed = 0x94;
	else if (speed == 70)
		real_speed = 0xA0;
	else if (speed == 75)
		real_speed = 0xAB;
	else if (speed == 80)
		real_speed = 0xB7;
	else if (speed == 85)
		real_speed = 0xC2;
	else if (speed == 90)
		real_speed = 0xCE;
	else if (speed == 95)
		real_speed = 0xD9;
	else if (speed == 100)
		real_speed = 0xE5;

	ret = gigabyte_laptop_set_devstate(FAN_CUSTOM_SPEED, real_speed, &output);
	if (ret)
		return ret;

	gigabyte = dev_get_drvdata(dev);
	gigabyte->fan_custom_display_speed = speed;
	gigabyte->fan_custom_internal_speed = real_speed;
	return count;
}

/*
 * Charge mode.
 * 0 = default mode
 * 1 = custom mode
 */
static ssize_t charge_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gigabyte->charge_mode);
}

static ssize_t charge_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, output;
	unsigned int mode;
	struct gigabyte_laptop_wmi *gigabyte;

	ret = kstrtouint(buf, 0, &mode);
	if (ret)
		return ret;

	if (mode > 1) {
		pr_err("Invalid charge mode\n");
		return -EINVAL;
	}

	// Only bit 2 affects the charging mode, so shift 2 bits to the left.
	ret = gigabyte_laptop_set_devstate(CHARGING_MODE, mode << 2, &output);
	if (ret)
		return ret;

	gigabyte = dev_get_drvdata(dev);
	gigabyte->charge_mode = mode;
	return count;
}

/*
 * Maximum charge limit. Only works if custom charge mode is enabled.
 * Can be set between 60 and 100 percent.
 */
static ssize_t charge_limit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gigabyte->charge_limit);
}

static ssize_t charge_limit_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, output;
	unsigned int limit;
	struct gigabyte_laptop_wmi *gigabyte;

	ret = kstrtouint(buf, 0, &limit);
	if (ret)
		return ret;

	if (limit > 100 || limit < 60) {
		pr_err("Invalid charge limit\n");
		return -EINVAL;
	}

	ret = gigabyte_laptop_set_devstate(CHARGING_LIMIT, limit, &output);
	if (ret)
		return ret;

	gigabyte = dev_get_drvdata(dev);
	gigabyte->charge_limit = limit;
	return count;
}

/*
 * TODO: Implement fan curve (0x68)
 */
static ssize_t fan_curve_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gigabyte->fan_curve_index);
}

static ssize_t fan_curve_index_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int index;
	struct gigabyte_laptop_wmi *gigabyte;

	ret = kstrtouint(buf, 0, &index);
	if (ret)
		return ret;

	gigabyte = dev_get_drvdata(dev);
	gigabyte->fan_curve_index = index;
	return count;
}

static ssize_t fan_curve_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);
	int index = gigabyte->fan_curve_index;

	return sysfs_emit(buf, "%d %d\n", gigabyte->fan_curve.temperature[index],
		gigabyte->fan_curve.speed[index]);
}

static ssize_t fan_curve_data_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, output;
	u16 data;
	u32 payload;
	struct gigabyte_laptop_wmi *gigabyte;

	ret = kstrtou16(buf, 0, &data);
	if (ret)
		return ret;

	gigabyte = dev_get_drvdata(dev);
	// likely payload: speed, temp, index
	//payload = gigabyte->fan_curve.speed[gigabyte->fan_curve_index] << 16 | gigabyte->fan_curve.temperature[gigabyte->fan_curve_index] << 8 | (u8) gigabyte->fan_curve_index;
	payload = data << 8 | gigabyte->fan_curve_index;

	ret = gigabyte_laptop_set_devstate(FAN_INDEX_VALUE, payload, &output);
	if (ret)
		return ret;

	gigabyte->fan_curve.temperature[gigabyte->fan_curve_index] = payload;
	gigabyte->fan_curve.speed[gigabyte->fan_curve_index] = payload >> 8;
	return count;
}

static ssize_t battery_cycle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret, cyc1, cyc2;

	ret = gigabyte_laptop_get_devstate(BATT_CYCLE, &cyc1);
	if (ret)
		return ret;
	ret = gigabyte_laptop_get_devstate(BATT_CYCLE2, &cyc2);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d\n", max(cyc1, cyc2));
}

#define TOGGLE_DEVICE(_device, _id) \
static ssize_t _device##_toggle_show(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	int ret, output; \
	ret = gigabyte_laptop_get_devstate(_id, &output); \
	if (ret) \
		return ret; \
	return sysfs_emit(buf, "%d\n", output); \
} \
static DEVICE_ATTR_RO(_device##_toggle);
TOGGLE_DEVICE(usb_charge_s3, USB_SLEEP);
TOGGLE_DEVICE(usb_charge_s4, USB_HIBERNATE);

static DEVICE_ATTR_RW(fan_mode);
static DEVICE_ATTR_RW(fan_custom_speed);
static DEVICE_ATTR_RW(charge_mode);
static DEVICE_ATTR_RW(charge_limit);
static DEVICE_ATTR_RW(fan_curve_index);
static DEVICE_ATTR_RW(fan_curve_data);
static DEVICE_ATTR_RO(battery_cycle);

static struct attribute *gigabyte_laptop_attributes[] = {
	&dev_attr_fan_mode.attr,
	&dev_attr_fan_custom_speed.attr,
	&dev_attr_charge_mode.attr,
	&dev_attr_charge_limit.attr,
	&dev_attr_usb_charge_s3_toggle.attr,
	&dev_attr_usb_charge_s4_toggle.attr,
	&dev_attr_fan_curve_index.attr,
	&dev_attr_fan_curve_data.attr,
	&dev_attr_battery_cycle.attr,
	NULL
};

static const struct attribute_group gigabyte_laptop_attr_group = {
	//.is_visible = gigabyte_laptop_sysfs_is_visible,
	.attrs = gigabyte_laptop_attributes,
};

#define DMI_EXACT_MATCH_GIGABYTE_LAPTOP_FAMILY(name) \
	{ .matches = { \
		DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "GIGABYTE"), \
		DMI_EXACT_MATCH(DMI_PRODUCT_FAMILY, name), \
	}}

#define DMI_EXACT_MATCH_GIGABYTE_LEGACY_DEVICE(name) \
	{ .matches = { \
		DMI_EXACT_MATCH(DMI_BOARD_VENDOR, "GIGABYTE"), \
		DMI_EXACT_MATCH(DMI_PRODUCT_NAME, name), \
	}}

static const struct dmi_system_id gigabyte_laptop_known_working_platforms[] = {
	DMI_EXACT_MATCH_GIGABYTE_LAPTOP_FAMILY("AERO"),
	DMI_EXACT_MATCH_GIGABYTE_LAPTOP_FAMILY("AORUS"),
	// For older Aero models
	DMI_EXACT_MATCH_GIGABYTE_LAPTOP_FAMILY("Intel"),
	DMI_EXACT_MATCH_GIGABYTE_LEGACY_DEVICE("Aero 14"),
	DMI_EXACT_MATCH_GIGABYTE_LEGACY_DEVICE("P64V6"),
	DMI_EXACT_MATCH_GIGABYTE_LEGACY_DEVICE("P64V7"),
	{ }
};

/* Driver init ********************************************/

static int probe_custom_fan_speed(int speed)
{
	if (speed == 0x39)
		return 25;
	else if (speed == 0x44)
		return 30;
	else if (speed == 0x50)
		return 35;
	else if (speed == 0x5B)
		return 40;
	else if (speed == 0x67)
		return 45;
	else if (speed == 0x72)
		return 50;
	else if (speed == 0x7D)
		return 55;
	else if (speed == 0x89)
		return 60;
	else if (speed == 0x94)
		return 65;
	else if (speed == 0xA0)
		return 70;
	else if (speed == 0xAB)
		return 75;
	else if (speed == 0xB7)
		return 80;
	else if (speed == 0xC2)
		return 85;
	else if (speed == 0xCE)
		return 90;
	else if (speed == 0xD9)
		return 95;
	else if (speed == 0xE5)
		return 100;
	else // For something like 0x5D, which is unknown
		return 40;
}

static int gigabyte_laptop_probe(struct device *dev)
{
	int ret, output;
	u8 result;
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	// Older devices are using a different method ID for silent fan mode.
	// In that case, newer devices won't return anything when using that ID.
	ret = gigabyte_laptop_get_devstate(FAN_SILENT_OLD, &output);
	if (output < 0) { // -1 on newer devices
		pr_info("Newer model detected, using new silent fan mode ID");
		gigabyte->fan_silent_method = FAN_SILENT_MODE;
	}
	else { // 0 on older devices
		pr_info("Older model detected, using old ID");
		gigabyte->fan_silent_method = FAN_SILENT_OLD;
	}

	// Set silent fan mode ID.
	fan_modes[1] = gigabyte->fan_silent_method;

	// Get current fan mode.
	ret = gigabyte_laptop_get_devstate(gigabyte->fan_silent_method, &output);
	if (ret)
		return ret;
	else if (output) {
		gigabyte->fan_mode = 1;
		goto obtain_custom_fan_speed;
	}
	ret = gigabyte_laptop_get_devstate(FAN_GAMING_MODE, &output);
	if (ret)
		return ret;
	else if (output) {
		gigabyte->fan_mode = 2;
		goto obtain_custom_fan_speed;
	}
	ret = gigabyte_laptop_get_devstate(FAN_CUSTOM_MODE, &output);
	if (ret)
		return ret;
	else if (output) {
		// Auto-maximum mode can't be read through WMI, so read EC register containing it
		ret = ec_read(0xD, &result);
		if (ret)
			return AE_ERROR;
		output = (result >> 7) & 0x1;
		if (output) {
			gigabyte->fan_mode = 4;
			goto obtain_custom_fan_speed;
		}
		ret = gigabyte_laptop_get_devstate(FAN_FIXED_MODE, &output);
		if (ret)
			return ret;
		else if (output)
			gigabyte->fan_mode = 5;
		else
			gigabyte->fan_mode = 3;
		goto obtain_custom_fan_speed;
	}
	// If all checks return 0, we are most likely in normal fan mode
	gigabyte->fan_mode = 0;

obtain_custom_fan_speed:
	ret = gigabyte_laptop_get_devstate(FAN_CUSTOM_SPEED, &output);
	if (ret)
		return ret;
	else if (output) {
		gigabyte->fan_custom_display_speed = probe_custom_fan_speed(output);
		gigabyte->fan_custom_internal_speed = output;
	}

	ret = gigabyte_laptop_get_devstate(CHARGING_MODE, &output);
	if (ret)
		return ret;
	else if (output)
		gigabyte->charge_mode = output;

	ret = gigabyte_laptop_get_devstate(CHARGING_LIMIT, &output);
	if (ret)
		return ret;
	else if (output)
		gigabyte->charge_limit = output;

	// Get the fan curve. Used by custom mode.
	for (u8 i = 0; i < FAN_CURVE_POINTS; i++) {
		ret = gigabyte_laptop_get_devstate2(FAN_INDEX_VALUE, i, &output);
		if (ret)
			return ret;
		else if (output) {
			gigabyte->fan_curve.temperature[i] = output;
			gigabyte->fan_curve.speed[i] = output >> 8;
		}
	}

	return 0;
}

static struct platform_driver platform_driver = {
	.driver = {
		.name = GIGABYTE_LAPTOP_FILE,
		.owner = THIS_MODULE,
	},
};

static void __exit gigabyte_laptop_exit(void)
{
	struct gigabyte_laptop_wmi *gigabyte;

	pr_info("Goodbye, World!\n");
	gigabyte = platform_get_drvdata(platform_device);
	hwmon_device_unregister(gigabyte->hwmon_dev);
	sysfs_remove_group(&gigabyte->pdev->dev.kobj, &gigabyte_laptop_attr_group);
	platform_driver_unregister(&platform_driver);
	platform_device_unregister(gigabyte->pdev);
	kfree(gigabyte);
}

static int __init gigabyte_laptop_init(void)
{
	struct gigabyte_laptop_wmi *gigabyte;
	int result;

	if (!wmi_has_guid(WMI_METHOD_WMBC) ||
		!wmi_has_guid(WMI_METHOD_WMBD)) {
		pr_warn("No known WMI GUID found!\n");
		return -ENODEV;
	}

	if (!dmi_check_system(gigabyte_laptop_known_working_platforms)) {
		pr_err("Laptop not supported\n");
		return -ENODEV;
	}

	result = platform_driver_register(&platform_driver);
	if (result) {
		pr_warn("Unable to register platform driver\n");
		return result;
	}

	gigabyte = kzalloc(sizeof(struct gigabyte_laptop_wmi), GFP_KERNEL);
	if (!gigabyte) {
		result = -ENOMEM;
		goto fail_platform_driver;
	}

	platform_device = platform_device_alloc(GIGABYTE_LAPTOP_FILE, -1);
	if (!platform_device) {
		pr_warn("Unable to allocate platform device\n");
		kfree(gigabyte);
		result = -ENOMEM;
		goto fail_platform_driver;
	}

	gigabyte->pdev = platform_device;
	platform_set_drvdata(gigabyte->pdev, gigabyte);

	result = platform_device_add(gigabyte->pdev);
	if (result) {
		pr_warn("Unable to add platform device\n");
		goto fail_platform_device;
	}

	result = sysfs_create_group(&gigabyte->pdev->dev.kobj,
					&gigabyte_laptop_attr_group);
	if (result)
		goto fail_sysfs;

	gigabyte->hwmon_dev = hwmon_device_register_with_info(&gigabyte->pdev->dev,
			GIGABYTE_LAPTOP_FILE, gigabyte, &gigabyte_laptop_chip_info, NULL);
	if (IS_ERR(gigabyte->hwmon_dev)) {
		result = PTR_ERR(gigabyte->hwmon_dev);
		pr_err("hwmon registration failed with %d\n", result);
		goto fail_sysfs;
	}

	result = gigabyte_laptop_probe(&gigabyte->pdev->dev);
	if (result) {
		pr_err("Probe failed\n");
		goto fail_probe;
	}
	pr_info("Hello, World!\n");
	return 0;

fail_probe:
	hwmon_device_unregister(gigabyte->hwmon_dev);
	sysfs_remove_group(&gigabyte->pdev->dev.kobj, &gigabyte_laptop_attr_group);
fail_sysfs:
	platform_device_del(gigabyte->pdev);
fail_platform_device:
	platform_device_put(gigabyte->pdev);
fail_platform_driver:
	platform_driver_unregister(&platform_driver);
	return result;
}

module_init(gigabyte_laptop_init);
module_exit(gigabyte_laptop_exit);
