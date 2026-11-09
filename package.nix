# SPDX-License-Identifier: GPL-2.0-or-later
#
# Nix package definition for the aorus-laptop out-of-tree kernel module.
#
# This file is meant to be called from a *kernel* package set, so that the
# module is always built against the kernel it will be loaded into:
#
#   config.boot.kernelPackages.callPackage ./package.nix { }
#
# It is not a standalone package: it needs the `kernel` and
# `kernelModuleMakeFlags` attributes that only exist inside a kernel package
# set. Use the flake outputs (packages.default / nixosModules.default) if you
# just want to install it.
{
  lib,
  stdenv,
  kernel,
  kernelModuleMakeFlags,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "aorus-laptop";
  # Keep in sync with the tags in this repository.
  version = "0.2.0";

  # Build exactly the sources of the checkout/flake revision being evaluated,
  # so the module never drifts from the driver code next to it.
  src = ./.;

  nativeBuildInputs = kernel.moduleBuildDependencies;

  # The kernel build system expects non-PIC objects and does not build with
  # -Werror=format-security, both of which stdenv enables by default.
  hardeningDisable = [
    "pic"
    "format"
  ];

  makeFlags = kernelModuleMakeFlags ++ [
    "KERNELRELEASE=${kernel.modDirVersion}"
    "KDIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
  ];

  enableParallelBuilding = true;

  # The upstream Makefile only knows `all` and `clean` - there is no
  # modules_install - and it leaves the built module in the source root, so
  # install it by hand, uncompressed (depmod handles that fine and it keeps the
  # package independent of the kernel's module compression setting).
  installPhase = ''
    runHook preInstall
    install -Dm444 aorus-laptop.ko \
      "$out/lib/modules/${kernel.modDirVersion}/kernel/drivers/platform/x86/aorus-laptop.ko"
    runHook postInstall
  '';

  meta = {
    description = "Linux kernel module for Gigabyte AERO/AORUS laptops to interact with the embedded controller";
    homepage = "https://github.com/tangalbert919/gigabyte-laptop-wmi";
    license = lib.licenses.gpl2Plus;
    platforms = lib.platforms.linux;
  };
})
