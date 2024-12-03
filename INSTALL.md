# Installation

## How to install

### Method 1: Using DKMS

**Note:** If you have Secure Boot enabled, you must follow [these instructions](https://github.com/dell/dkms?tab=readme-ov-file#secure-boot) or the module will not load.

This module can be installed using DKMS. You can download the driver tarball from the [releases page](https://github.com/tangalbert919/gigabyte-laptop-wmi/releases) and load it into the DKMS tree:
```
dkms ldtarball driver.tar.gz
```

If you have this repository checked out locally, you can create a tarball and then load it into the DKMS tree:
```
tar -czf driver.tar.gz Makefile aorus-laptop.c dkms.conf
```

Be sure to edit the `PACKAGE_VERSION` flag in `dkms.conf` before creating the tarball.

### Method 2: Manually

**Note:** If you have Secure Boot enabled, you must sign the kernel module after compiling it. Using a signing key already enrolled into the computer is recommended.

Simply run the following commands:
```
make
sudo insmod aorus-laptop.ko
```

## How to remove

If you have installed the kernel driver with DKMS, you can run this command to remove it from the DKMS tree:
```
# Replace <version> with the version of the driver in use. Use "dkms status" if you are not sure what version you have installed.
sudo dkms remove aorus-laptop/<version> --all
```

If you have installed the kernel driver manually, you can simply run this command:
```
sudo rmmod aorus_laptop
```
