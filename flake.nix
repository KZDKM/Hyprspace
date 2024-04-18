{
  description = "Hyprspace";

  inputs = {
    nixpkgs.follows = "hyprland/nixpkgs";

    hyprland = {
      type = "github";
      owner = "hyprwm";
      repo = "Hyprland";
    };

    hyprlandPlugins = {
      url = "github:hyprwm/hyprland-plugins";
      inputs.hyprland.follows = "hyprland";
    };
  };

  outputs = {
    self,
    nixpkgs,
    hyprland,
    hyprlandPlugins,
  }: let
    # System types to support.
    supportedSystems = ["x86_64-linux" "aarch64-linux"];
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    # Nixpkgs instantiated for supported system types.
    pkgsFor = forAllSystems (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = with self.overlays; [hyprland-plugins];
      });

    version = "0.1";
    mkHyprlandPlugin = hyprlandPlugins.overlays.mkHyprlandPlugin;
    lib = nixpkgs.lib;
  in {
    overlays = {
      default = self.overlays.hyprland-plugins.Hyprspace;
      hyprland-plugins = lib.composeManyExtensions [
        mkHyprlandPlugin
        (final: prev: let
          inherit (final) callPackage;
        in {
          Hyprspace =
            callPackage
            ({
              lib,
              hyprland,
              hyprlandPlugins,
            }:
              hyprlandPlugins.mkHyprlandPlugin {
                pluginName = "Hyprspace";
                inherit version;
                src = ./.;

                inherit (hyprland) nativeBuildInputs;

                meta = with lib; {
                  homepage = "https://github.com/KZDKM/Hyprspace";
                  description = "Workspace overview plugin for Hyprland";
                  license = licenses.gpl2Only;
                  platforms = platforms.linux;
                };
              })
            {};
        })
      ];
    };

    # Provide some binary packages for selected system types.
    packages =
      forAllSystems
      (system: {
        inherit (pkgsFor.${system}) Hyprspace;
        default = self.packages.${system}.Hyprspace;
      });

    # The default environment for 'nix develop'
    devShells = forAllSystems (system: {
      default = pkgsFor.${system}.mkShell {
        shellHook = ''
          meson setup build --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
        name = "Hyprspace-shell";
        nativeBuildInputs = with pkgsFor.${system}; [gcc13];
        buildInputs = [hyprland.packages.${system}.hyprland];
        inputsFrom = [
          hyprland.packages.${system}.hyprland
          self.packages.${system}.Hyprspace
        ];
      };
    });

    formatter = forAllSystems (system: pkgsFor.${system}.alejandra);
  };
}
