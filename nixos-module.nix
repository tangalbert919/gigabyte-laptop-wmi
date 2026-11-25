# SPDX-License-Identifier: GPL-2.0-or-later
#
# NixOS module for the aorus-laptop kernel module.
#
# The module is built against config.boot.kernelPackages, so it follows kernel
# upgrades automatically and never has to be rebuilt by hand.
#
# ./package.nix is a plain callPackage-able file: it is called from the kernel
# package set, not through the flake's own outputs, so this module has no
# dependency on the flake layer (and no dependency on the flake's nixpkgs).
# It cannot use a bare `pkgs.callPackage ./package.nix { }` like an ordinary
# package would: `kernel` and `kernelModuleMakeFlags` only exist inside a kernel
# package set, so that aborts with
#   Function called without required argument "kernel" at ./package.nix
# and would in any case build the module against the wrong kernel.
# nixos/modules/tasks/filesystems/bcachefs.nix uses the same indirection.
{
  config,
  lib,
  ...
}:

let
  cfg = config.hardware.gigabyte-laptop;
in
{
  options.hardware.gigabyte-laptop = {
    enable = lib.mkEnableOption "the aorus-laptop kernel module, which exposes fan control, charging thresholds and sensors on Gigabyte AERO/AORUS laptops through sysfs and hwmon";

    package = lib.mkOption {
      type = lib.types.package;
      default = config.boot.kernelPackages.callPackage ./package.nix { };
      defaultText = lib.literalExpression "config.boot.kernelPackages.callPackage ./package.nix { }";
      description = ''
        The aorus-laptop kernel module to install. Defaults to the module built
        from this flake against your running kernel package set.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    boot.extraModulePackages = [ cfg.package ];
    # aorus_laptop, not aorus-laptop: the module name is the .ko file name.
    boot.kernelModules = [ "aorus_laptop" ];
  };
}
