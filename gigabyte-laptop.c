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
#include <linux/module.h>
#include <linux/sparse-keymap.h>
#include <linux/wmi.h>

#define GIGABYTE_LAPTOP_VERSION "0.01"
#define GIGABYTE_LAPTOP_FILE  KBUILD_MODNAME

MODULE_AUTHOR("Albert Tang");
MODULE_DESCRIPTION("Gigabyte laptop WMI driver");
MODULE_LICENSE("GPL");

/* _SB_.PCI0.AMW0._WDG */
#define WMI_METHOD_WMBC "ABBC0F6F-8EA1-11D1-00A0-C90629100000"
#define WMI_METHOD_WMBD "ABBC0F75-8EA1-11D1-00A0-C90629100000"
#define WMI_EVENT_GUID "ABBC0F72-8EA1-11D1-00A0-C90629100000"

static void gigabyte_wmi_notify(u32 value, void *context)
{
    // TODO: implement something
}

static void wmi_input_setup(void)
{
    status = wmi_install_notify_handler(WMI_EVENT_GUID, gigabyte_wmi_notify,
                                    (void *)0);
    if (ACPI_FAILURE(status)) {
        // something something
        return -ENODEV;
    }
}

static void __exit gigabyte_laptop_exit(void)
{
    pr_info("Goodbye, World!\n");
    wmi_remove_notify_handler(WMI_EVENT_GUID);
}

static int __init gigabyte_laptop_init(void)
{
    if (!wmi_has_guid(WMI_EVENT_GUID)) {
        pr_warning("No known WMI GUID found!\n");
        return -ENODEV;
    }
    pr_info("Hello, World!\n");
    return 0;
}

module_init(gigabyte_laptop_init);
module_exit(gigabyte_laptop_exit);
