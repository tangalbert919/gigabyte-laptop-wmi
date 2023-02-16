// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  gigabyte-laptop.c - Gigabyte laptop WMI driver
 *
 *  Copyright (C) 2023 Albert Tang
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
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
#define WMI_METHOD_WMBC "ABBC0F6F-8EA1-11D1-00A0-C90629100000" // Seems to only return values
#define WMI_METHOD_WMBD "ABBC0F75-8EA1-11D1-00A0-C90629100000" // Will probably do most of the work.
#define WMI_STRING_WMBC "\\_SB.PCI0.AMW0.WMBC"
#define WMI_STRING_WMBD "\\_SB.PCI0.AMW0.WMBD"

/* WMI method arguments */
#define FAN_SILENT_MODE  0x57
#define CHARGING_MODE    0x64
#define CHARGING_LIMIT   0x65
#define FAN_DEEP_CONTROL 0x67
#define FAN_CUSTOM_TYPE  0x6A
#define FAN_CUSTOM_MODE  0x70
#define FAN_GAMING_MODE  0x71
#define TEMP_CPU         0xE1
#define TEMP_GPU         0xE2
#define FAN_CPU_RPM      0xE4
#define FAN_GPU_RPM      0xE5
#define FAN_THREE_RPM    0xE8
#define FAN_FOUR_RPM     0xE9

struct gigabyte_laptop_wmi {
	struct platform_device *pdev;
	struct device *hwmon_dev;
	int fan_mode;
	int fan_custom_speed;
	int charge_mode;
	int charge_limit;
};

static struct platform_device *platform_device;

/* WMI methods ********************************************/

/* WMBC method (checks value in EC) */
static int gigabyte_laptop_get_devstate(u32 arg1, int *result)
{
	union acpi_object args[3], *obj;
	acpi_status status;
	acpi_handle handle;
	struct acpi_object_list params;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

	status = acpi_get_handle(NULL, (acpi_string) WMI_STRING_WMBC, &handle);
	if (ACPI_FAILURE(status)) {
		pr_err("Cannot get handle\n");
		return -1;
	}

	args[0].type = ACPI_TYPE_INTEGER;
	args[0].integer.value = 0;
	args[1].type = ACPI_TYPE_INTEGER;
	args[1].integer.value = arg1;
	args[2].type = ACPI_TYPE_INTEGER;
	args[2].integer.value = 0;
	params.count = 3;
	params.pointer = args;

	status = acpi_evaluate_object(handle, NULL, &params, &buffer);
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

/* WMBD method (sets value in EC) */
static int gigabyte_laptop_set_devstate(u32 arg1, u32 arg2, int *result)
{
	union acpi_object args[3], *obj;
	acpi_status status;
	acpi_handle handle;
	struct acpi_object_list params;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

	status = acpi_get_handle(NULL, (acpi_string) WMI_STRING_WMBD, &handle);
	if (ACPI_FAILURE(status)) {
		pr_err("Cannot get handle\n");
		return -1;
	}

	args[0].type = ACPI_TYPE_INTEGER;
	args[0].integer.value = 0;
	args[1].type = ACPI_TYPE_INTEGER;
	args[1].integer.value = arg1;
	args[2].type = ACPI_TYPE_INTEGER;
	args[2].integer.value = arg2;
	params.count = 3;
	params.pointer = args;

	status = acpi_evaluate_object(handle, NULL, &params, &buffer);
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

	switch (type) {
		case hwmon_temp:
			switch (channel) {
				case 0:
					ret = gigabyte_laptop_get_devstate(TEMP_CPU, &output);
					if (ret)
						break;
					*val = output;
					break;
				case 1:
					ret = gigabyte_laptop_get_devstate(TEMP_GPU, &output);
					if (ret)
						break;
					*val = output;
					break;
				default:
					*val = 0;
					break;
			}
			break;
		case hwmon_fan:
			switch (channel) {
				case 0:
					ret = gigabyte_laptop_get_devstate(FAN_CPU_RPM, &output);
					if (ret)
						break;
					*val = convert_fan_rpm(output);
					break;
				case 1:
					ret = gigabyte_laptop_get_devstate(FAN_GPU_RPM, &output);
					if (ret)
						break;
					*val = convert_fan_rpm(output);
					break;
				case 2:
					ret = gigabyte_laptop_get_devstate(FAN_THREE_RPM, &output);
					if (ret)
						break;
					*val = convert_fan_rpm(output);
					break;
				case 3:
					ret = gigabyte_laptop_get_devstate(FAN_FOUR_RPM, &output);
					if (ret)
						break;
					*val = convert_fan_rpm(output);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
	return 0;
}

static int gigabyte_laptop_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
					u32 attr, int channel, long val)
{
	// Probably won't be used.
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
	//.write = gigabyte_laptop_hwmon_write,
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
 * 4 = failed to get fan mode
 */
static int set_fan_mode(u32 fan_mode)
{
	int ret, result;

	if (fan_mode == FAN_SILENT_MODE) {
		ret = gigabyte_laptop_set_devstate(FAN_GAMING_MODE, 0, &result);
		if (ret)
			return ret;
		ret = gigabyte_laptop_set_devstate(fan_mode, 1, &result);
		if (ret)
			return ret;
	} else if (fan_mode == FAN_GAMING_MODE) {
		ret = gigabyte_laptop_set_devstate(FAN_SILENT_MODE, 0, &result);
		if (ret)
			return ret;
		ret = gigabyte_laptop_set_devstate(fan_mode, 1, &result);
		if (ret)
			return ret;
	} else if (fan_mode == 0) { // Normal fan mode
		ret = gigabyte_laptop_set_devstate(FAN_SILENT_MODE, 0, &result);
		if (ret)
			return ret;
		ret = gigabyte_laptop_set_devstate(FAN_GAMING_MODE, 0, &result);
		if (ret)
			return ret;
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

	if (fan_mode > 3) {
		pr_err("Invalid fan mode\n");
		return count;
	} else if (fan_mode == 2) {
		ret = set_fan_mode(FAN_GAMING_MODE);
		if (ret)
			return ret;
	} else if (fan_mode == 1) {
		ret = set_fan_mode(FAN_SILENT_MODE);
		if (ret)
			return ret;
	} else if (fan_mode == 0) {
		ret = set_fan_mode(0);
		if (ret)
			return ret;
	}

	gigabyte = dev_get_drvdata(dev);
	gigabyte->fan_mode = fan_mode;
	return count;
}

/*
 * Custom fan speed. Only works if custom mode is enabled.
 * Seems to only scale between 68 and 229.
 */
static ssize_t fan_custom_speed_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", gigabyte->fan_custom_speed);
}

static ssize_t fan_custom_speed_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	// TODO: Implement
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
	// TODO: Implement
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

static DEVICE_ATTR_RW(fan_mode);
static DEVICE_ATTR_RW(fan_custom_speed);
static DEVICE_ATTR_RW(charge_mode);
static DEVICE_ATTR_RW(charge_limit);

static struct attribute *gigabyte_laptop_attributes[] = {
	&dev_attr_fan_mode.attr,
	&dev_attr_fan_custom_speed.attr,
	&dev_attr_charge_mode.attr,
	&dev_attr_charge_limit.attr,
	NULL
};

static const struct attribute_group gigabyte_laptop_attr_group = {
	//.is_visible = gigabyte_laptop_sysfs_is_visible,
	.attrs = gigabyte_laptop_attributes,
};

/* Driver init ********************************************/

static int gigabyte_laptop_probe(struct device *dev)
{
	int ret, output;
	struct gigabyte_laptop_wmi *gigabyte = dev_get_drvdata(dev);

	// Get current fan mode.
	ret = gigabyte_laptop_get_devstate(FAN_SILENT_MODE, &output);
	if (ret)
		return ret;
	else if (output) {
		gigabyte->fan_mode = 1;
		goto obtain_custom_fan_speed;
	}
		gigabyte->fan_mode = 1;
	ret = gigabyte_laptop_get_devstate(FAN_GAMING_MODE, &output);
	if (ret)
		return ret;
	else if (output) {
		gigabyte->fan_mode = 2;
		goto obtain_custom_fan_speed;
	}
	// Neither silent nor gaming mode are active; must be normal fan mode
	gigabyte->fan_mode = 0;

obtain_custom_fan_speed:
	ret = gigabyte_laptop_get_devstate(FAN_CUSTOM_MODE, &output);
	if (ret)
		return ret;
	else if (output)
		gigabyte->fan_custom_speed = output;

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

	gigabyte = kzalloc(sizeof(struct gigabyte_laptop_wmi), GFP_KERNEL);
	if (!gigabyte)
		return -ENOMEM;

	platform_device = platform_device_alloc(GIGABYTE_LAPTOP_FILE, -1);
	if (!platform_device) {
		pr_warn("Unable to allocate platform device\n");
		kfree(gigabyte);
		return -ENOMEM;
	}

	gigabyte->pdev = platform_device;
	platform_set_drvdata(gigabyte->pdev, gigabyte);

	result = platform_device_add(gigabyte->pdev);
	if (result) {
		pr_warn("Unable to add platform device\n");
		goto fail_platform_device;
	}

	result = platform_driver_register(&platform_driver);
	if (result) {
		pr_warn("Unable to register platform driver\n");
		return result;
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
	platform_driver_unregister(&platform_driver);
fail_platform_device:
	platform_device_put(gigabyte->pdev);
	return result;
}

module_init(gigabyte_laptop_init);
module_exit(gigabyte_laptop_exit);
