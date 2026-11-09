{
  description = "gigabyte-laptop-wmi - ACPI/WMI kernel module for Gigabyte AERO/AORUS laptops (fan control, charging, sensors)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      # The driver talks to an ACPI WMI device, which only exists on x86_64:
      # arm64 kernels are built without CONFIG_ACPI_WMI, so the module does not
      # compile there (and the hardware is Intel anyway).
      systems = [ "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      # `nix build github:tangalbert919/gigabyte-laptop-wmi` builds the module
      # against nixpkgs' default kernel. Prefer the NixOS module below, which
      # builds it against *your* boot.kernelPackages.
      packages = forAllSystems (pkgs: {
        default = pkgs.linuxPackages.callPackage ./package.nix { };
        aorus-laptop = pkgs.linuxPackages.callPackage ./package.nix { };
      });

      # For imperative use: pkgs.aorus-laptop is built against pkgs.linuxPackages.
      overlays.default = final: _prev: {
        aorus-laptop = final.linuxPackages.callPackage ./package.nix { };
      };

      # Adds hardware.gigabyte-laptop.enable; builds the module against
      # config.boot.kernelPackages and loads it on boot.
      nixosModules.default = import ./nixos-module.nix;
      nixosModules.gigabyte-laptop-wmi = self.nixosModules.default;
    };
}
