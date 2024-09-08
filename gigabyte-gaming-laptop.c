// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  gigabyte-gaming-laptop.c - Gigabyte Gaming laptop WMI driver
 *
 *  Copyright (C) 2024 Albert Tang
 */
#include "linux/device/driver.h"
#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/wmi.h>

#define WMI_METHOD_WMBB "ABBC0F6D-8EA1-11D1-00A0-C90629100000" // Gaming/Sabre/U

MODULE_AUTHOR("Albert Tang");
MODULE_DESCRIPTION("Gigabyte laptop WMI driver");
MODULE_LICENSE("GPL");

struct gigabyte_laptop_wmi {
	struct wmi_device *wdev;
	struct device *hwmon_dev;
	int fan_mode;
	int fan_custom_display_speed;
	int fan_custom_internal_speed;
	int charge_mode;
	int charge_limit;
};

static const struct wmi_device_id gigabyte_wmi_id_table[] = {
	{ .guid_string = WMI_METHOD_WMBB },
    { },
};
MODULE_DEVICE_TABLE(wmi, gigabyte_wmi_id_table);

static int gigabyte_wmi_probe(struct wmi_device *wdev, const void *context)
{
	// TODO: begin implementation
	pr_info("Hello world!\n");
	pr_info("Object instances: %d\n", wmidev_instance_count(wdev));
	return 0;
}

static void gigabyte_wmi_remove(struct wmi_device *wdev)
{
	// TODO: begin implementation
	pr_info("Goodbye world!\n");
}

static struct wmi_driver gigabyte_wmi_driver = {
	.driver = {
		.name = "gigabyte-wmi",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = gigabyte_wmi_probe,
	.remove = gigabyte_wmi_remove,
	.id_table = gigabyte_wmi_id_table,
	.no_singleton = true,
};

module_wmi_driver(gigabyte_wmi_driver);

/*static int __init init_gigabyte_wmi(void)
{
	if (!wmi_has_guid(WMI_METHOD_WMBC) ||
		!wmi_has_guid(WMI_METHOD_WMBD)) {
		pr_warn("No known WMI GUID found!\n");
		return -ENODEV;
	}

	return wmi_driver_register(&gigabyte_wmi_driver);
}
late_initcall(init_gigabyte_wmi);

static void __exit exit_gigabyte_wmi(void)
{
	wmi_driver_unregister(&gigabyte_wmi_driver);
}
module_exit(exit_gigabyte_wmi);*/
