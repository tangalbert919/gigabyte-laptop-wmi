# Installation

## How to install

For methods 1 and 2, you must copy `aorus-laptop.conf` to `/etc/modules-load.d/` so the kernel module is loaded on boot automatically. The NixOS method does this for you.

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

The last command has to be run after every reboot. If you have updated the kernel, you must run `make` before loading the kernel module.

### Method 3: NixOS

**Note:** If you have Secure Boot enabled, the module has to be signed or it will not load.

This repository is a flake, so the driver is packaged and built against your own kernel package set. Add it to your `flake.nix`:

```nix
inputs.gigabyte-laptop-wmi = {
  url = "github:tangalbert919/gigabyte-laptop-wmi";
  # Reuse your nixpkgs instead of pulling in a second copy of it.
  inputs.nixpkgs.follows = "nixpkgs";
};
```

and enable it in the configuration of the laptop:

```nix
{
  imports = [ inputs.gigabyte-laptop-wmi.nixosModules.default ];

  hardware.gigabyte-laptop.enable = true;
}
```

This builds the module against `config.boot.kernelPackages`, so it is rebuilt on every kernel update, and loads `aorus_laptop` at boot - no DKMS, no manual `insmod`, and no `modules-load.d` entry needed. The same flake also exposes `overlays.default` (giving `pkgs.aorus-laptop`) if you would rather wire up `boot.extraModulePackages` and `boot.kernelModules` yourself.

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

If you installed the driver with the flake, set `hardware.gigabyte-laptop.enable = false;` (or drop the import) and rebuild.
