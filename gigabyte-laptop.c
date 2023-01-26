// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  gigabyte-laptop.c - Gigabyte laptop WMI driver
 *
 *  Copyright (C) 2023 Albert Tang
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/input/sparse-keymap.h>
#include <linux/wmi.h>

#define GIGABYTE_LAPTOP_VERSION "0.01"
#define GIGABYTE_LAPTOP_FILE  KBUILD_MODNAME

MODULE_AUTHOR("Albert Tang");
MODULE_DESCRIPTION("Gigabyte laptop WMI driver");
MODULE_LICENSE("GPL");

/* _SB_.PCI0.AMW0._WDG */
#define WMI_METHOD_WMBC "ABBC0F6F-8EA1-11D1-00A0-C90629100000" // Seems to only return values
#define WMI_METHOD_WMBD "ABBC0F75-8EA1-11D1-00A0-C90629100000" // Will probably do most of the work.
#define WMI_EVENT_GUID "ABBC0F72-8EA1-11D1-00A0-C90629100000"
#define WMI_WMBC_METHOD "\\_SB.PCI0.AMW0.WMBC"

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

static const struct key_entry gigabyte_laptop_keymap[] = {

};

struct gigabyte_laptop_wmi {
	struct input_dev *inputdev;
};

static struct platform_device *platform_device;

/* The WMBC method is used here. */
static int gigabyte_laptop_get_devstate(u32 arg1, struct acpi_buffer *output, int *result)
{
	union acpi_object args[3];
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
	params.pointer = &args;

	status = acpi_evaluate_object(handle, NULL, &params, &buffer);
	if (ACPI_FAILURE(status))
		return -1;

	union acpi_object *obj = buffer.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER)
		*result = obj->integer.value;
	else
		return -EIO;
	return 0;
}

static void gigabyte_laptop_notify(u32 value, void *context)
{
	struct gigabyte_laptop_wmi *gigabyte = context;
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;
	int code;

	status = wmi_get_event_data(value, &response);
	if (status != AE_OK) {
		pr_err("bad event status 0x%x\n", status);
		return;
	}

	obj = (union acpi_object *)response.pointer;

	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		code = obj->integer.value;

		if (!sparse_keymap_report_event(gigabyte->inputdev, code, 1, true))
			pr_info("Unknown key %x pressed\n", code);
	}

	kfree(obj);
}

static int gigabyte_laptop_input_setup(struct gigabyte_laptop_wmi *gigabyte)
{
	int err;

	gigabyte->inputdev = input_allocate_device();
	if (!gigabyte->inputdev)
		return -ENOMEM;

	gigabyte->inputdev->name = "Gigabyte laptop WMI hotkeys";
	gigabyte->inputdev->phys = GIGABYTE_LAPTOP_FILE "/input0";
	gigabyte->inputdev->id.bustype = BUS_HOST;
	gigabyte->inputdev->dev.parent = &platform_device->dev;

	err = sparse_keymap_setup(gigabyte->inputdev, gigabyte_laptop_keymap, NULL);
	if (err) {
		pr_err("Unable to setup input device keymap\n");
		goto err_free_dev;
	}

	err = input_register_device(gigabyte->inputdev);
	if (err) {
		pr_err("Unable to register input device\n");
		goto err_free_dev;
	}

	return 0;

err_free_dev:
	input_free_device(gigabyte->inputdev);
	return err;
}

static void gigabyte_laptop_input_exit(struct gigabyte_laptop_wmi *gigabyte)
{
	if (gigabyte->inputdev)
		input_unregister_device(gigabyte->inputdev);
	gigabyte->inputdev = NULL;
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
	wmi_remove_notify_handler(WMI_EVENT_GUID);
	gigabyte = platform_get_drvdata(platform_device);
	platform_driver_unregister(&platform_driver);
	platform_device_unregister(platform_device);
	gigabyte_laptop_input_exit(gigabyte);
	kfree(gigabyte);
}

static int __init gigabyte_laptop_init(void)
{
	struct gigabyte_laptop_wmi *gigabyte;
	acpi_status status;
	int result;

	if (!wmi_has_guid(WMI_EVENT_GUID) ||
		!wmi_has_guid(WMI_METHOD_WMBC) ||
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

	// Probably move this somewhere else.
	result = gigabyte_laptop_input_setup(gigabyte);
	if (result)
		return result;
	
	status = wmi_install_notify_handler(WMI_EVENT_GUID,
									gigabyte_laptop_notify, gigabyte);
	if (ACPI_FAILURE(status)) {
		gigabyte_laptop_input_exit(gigabyte);
		return -ENODEV;
	}

	pr_info("Hello, World!\n");
	return 0;

fail_platform_device:
	platform_device_put(platform_device);
	return result;
}

module_init(gigabyte_laptop_init);
module_exit(gigabyte_laptop_exit);
