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
#define WMI_WMBC_METHOD "\\_SB.PCI0.AMW0.WMBC"
#define WMI_WMBD_METHOD "\\_SB.PCI0.AMW0.WMBD"

/* _SB_.WFDE._WDG */
#define WMI_EVENT_WFDE "A6FEA33E-DABF-46F5-BFC8-460D961BEC9F"

/* Fan modes (only tested on Aero 15 Classic-XA) */
#define FAN_SILENT_MODE 0x57
#define FAN_CUSTOM_MODE 0x70
#define FAN_GAMING_MODE 0x71

/* WMBC method IDs */
#define CPUTEMP 0xE1
#define GPUTEMP 0xE2
#define FAN1RPM 0xE4
#define FAN2RPM 0xE5

struct gigabyte_laptop_wmi {
	struct device *hwmon_dev;
	int fan_mode;
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

	status = acpi_get_handle(NULL, (acpi_string) WMI_WMBC_METHOD, &handle);
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

	status = acpi_get_handle(NULL, (acpi_string) WMI_WMBD_METHOD, &handle);
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
static umode_t gigabyte_laptop_hwmon_is_visible(const void *data, enum hwmon_sensor_types type,
					u32 attr, int channel)
{
	return 0;
}

static int gigabyte_laptop_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
					u32 attr, int channel, long *val)
{
	// TODO: Implement
	return 0;
}

static int gigabyte_laptop_hwmon_write(struct device _dev, enum hwmon_sensor_types type,
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
				HWMON_F_INPUT | HWMON_F_LABEL,
				HWMON_F_INPUT | HWMON_F_LABEL),
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
static ssize_t fan_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*int ret, output;

	ret = gigabyte_laptop_get_devstate(FAN_SILENT_MODE, &output);
	if (!ret)
		return sysfs_emit(buf, "%d\n", 4);*/
	struct gigabyte_laptop_wmi *gigabyte;

	gigabyte = platform_get_drvdata(platform_device);
	return sysfs_emit(buf, "%d\n", gigabyte->fan_mode);
}

static ssize_t fan_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	// TODO: Implement
	struct gigabyte_laptop_wmi *gigabyte;

	gigabyte = platform_get_drvdata(platform_device);
	sscanf(buf, "%d\n", &gigabyte->fan_mode);
	return count;
}

static DEVICE_ATTR_RW(fan_mode);
//static DEVICE_ATTR_RW(fan_custom_speed);
//static DEVICE_ATTR_RW(charge_mode);
//static DEVICE_ATTR_RW(charge_limit);

static struct attribute *gigabyte_laptop_attributes[] = {
	&dev_attr_fan_mode.attr,
	//&dev_attr_fan_custom_speed.attr,
	//&dev_attr_charge_mode.attr,
	//&dev_attr_charge_limit.attr,
	NULL
};

static const struct attribute_group gigabyte_laptop_attr_group = {
	//.is_visible = gigabyte_laptop_sysfs_is_visible,
	.attrs = gigabyte_laptop_attributes,
};

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
	sysfs_remove_group(&platform_device->dev.kobj, &gigabyte_laptop_attr_group);
	platform_driver_unregister(&platform_driver);
	platform_device_unregister(platform_device);
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

	platform_set_drvdata(platform_device, gigabyte);

	result = platform_device_add(platform_device);
	if (result) {
		pr_warn("Unable to add platform device\n");
		goto fail_platform_device;
	}

	result = platform_driver_register(&platform_driver);
	if (result) {
		pr_warn("Unable to register platform driver\n");
		return result;
	}

	result = sysfs_create_group(&platform_device->dev.kobj,
					&gigabyte_laptop_attr_group);
	if (result)
		goto fail_sysfs;
	pr_info("Hello, World!\n");
	return 0;

fail_sysfs:
	platform_device_del(platform_device);
fail_platform_device:
	platform_device_put(platform_device);
	return result;
}

module_init(gigabyte_laptop_init);
module_exit(gigabyte_laptop_exit);
