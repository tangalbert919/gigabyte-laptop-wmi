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

The last command has to be run after every reboot. If you have updated the kernel, you must run `make` before loading the kernel module.

### Method 3: As a nix module
**Note:** save this as for example gigabyte-laptop-wmi.nix and call it in your configuration using extraModulePackages = [ (config.boot.kernelPackages.callPackage ./gigabyte-laptop-wmi.nix {}) ];

```
{ stdenv, lib, fetchFromGitHub, kernel, kernelModuleMakeFlags, kmod }:
let
  modPath = "drivers/gigabyte";
  modDestDir = "$out/lib/modules/${kernel.modDirVersion}/kernel/${modPath}";

in stdenv.mkDerivation rec {
  pname = "gigabyte-laptop-wmi";
  version = "0.1.0";

  src = fetchFromGitHub {
    owner = "tangalbert919";
    repo = "gigabyte-laptop-wmi";
    rev = "${version}";
    sha256 = "sha256-+ZRyrI3PJRIEFEcOrKh9Zuhg07o/YMkycspOBPDAaeU=";
  };

  hardeningDisable = [ "pic" "format" ];
  nativeBuildInputs = kernel.moduleBuildDependencies;

  makeFlags = kernelModuleMakeFlags ++ [
    "KERNELRELEASE=${kernel.modDirVersion}"
    "KDIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
    "INSTALL_MOD_PATH=${modDestDir}"
  ];

  enableParallelBuilding = true;
  installPhase = ''
    runHook preInstall
    mkdir -p ${modDestDir}
    find . -name '*.ko' -exec cp --parents '{}' ${modDestDir} \;
    find ${modDestDir} -name '*.ko' -exec xz -f '{}' \;
    runHook postInstall
  '';
  meta = {
    description = "Linux kernel module for Gigabyte laptops to interact with the embedded controller";
    homepage = "https://github.com/tangalbert919/gigabyte-laptop-wmi";
    license = lib.licenses.gpl2;
    maintainers = [ lib.maintainers.makefu ];
    platforms = lib.platforms.linux;
  };
}

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

If you have installed the nix module, simply remove it from your configuration and nixos-rebuild switch
