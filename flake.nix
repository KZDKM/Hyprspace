{
  description = "Hyprspace";

  inputs = {
    hyprland = {
      type = "github";
      owner = "hyprwm";
      repo = "Hyprland";
    };

    hyprlandPlugins = {
      type = "github";
      owner = "hyprwm";
      repo = "hyprland-plugins";

      inputs.hyprland.follows = "hyprland";
    };
  };

  outputs = {
    self,
    hyprland,
    hyprlandPlugins,
  }: let
    inherit (hyprland.inputs) nixpkgs;
    inherit (nixpkgs) lib;

    # System types to support.
    supportedSystems = ["x86_64-linux" "aarch64-linux"];
    perSystem = attrs:
      lib.genAttrs supportedSystems (system: let
        pkgs = import nixpkgs {
          localSystem.system = system;
          overlays = with self.overlays; [hyprland-plugins];
        };
      in
        attrs system pkgs);

    version = "0.1";
  in {
    overlays = {
      default = self.overlays.hyprland-plugins.Hyprspace;

      # https://github.com/hyprwm/hyprland-plugins/blob/00d147d7f6ad2ecfbf75efe4a8402723c72edd98/flake.nix#L41
      hyprland-plugins = lib.composeManyExtensions [
        hyprlandPlugins.overlays.mkHyprlandPlugin
        (final: prev: {
          Hyprspace =
            final.callPackage
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
    packages = perSystem (system: pkgs: {
      inherit (pkgs) Hyprspace;
      default = self.packages.${system}.Hyprspace;
    });

    # The default environment for 'nix develop'
    devShells = perSystem (system: pkgs: {
      default = pkgs.mkShell {
        shellHook = ''
          meson setup build --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
        name = "Hyprspace-shell";
        nativeBuildInputs = with pkgs; [gcc13];
        buildInputs = [hyprland.packages.${system}.hyprland];
        inputsFrom = [
          hyprland.packages.${system}.hyprland
          self.packages.${system}.Hyprspace
        ];
      };
    });

    formatter = perSystem (_: pkgs: pkgs.alejandra);
  };
}
