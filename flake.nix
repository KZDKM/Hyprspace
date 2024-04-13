{
  description = "Hyprspace";

  # Nixpkgs / NixOS version to use.
  inputs.nixpkgs.follows = "hyprland/nixpkgs";

  inputs.hyprland = {
    url = github:hyprwm/Hyprland/185a3b48814cc4a1afbf32a69792a6161c4038cd;
  };
  inputs.hyprlandPlugins = {
    url = "github:hyprwm/hyprland-plugins";
    inputs.hyprland.follows = "hyprland";
  };

  outputs = { self, nixpkgs, hyprland, hyprlandPlugins }:
    let
      # System types to support.
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      # Nixpkgs instantiated for supported system types.
      pkgsFor = forAllSystems (system:
        import nixpkgs {
          localSystem.system = system;
          overlays = with self.overlays; [ hyprland-plugins ];
        });

      version = "0.1";
      mkHyprlandPlugin = hyprlandPlugins.overlays.mkHyprlandPlugin;
      lib = nixpkgs.lib;
    in

    {
      overlays = {
        default = self.overlays.hyprland-plugins.Hyprspace;
        hyprland-plugins = lib.composeManyExtensions [
          mkHyprlandPlugin
          (final: prev:
            let
              inherit (final) callPackage;
            in
            {
              Hyprspace = callPackage
                ({ lib
                 , hyprland
                 , hyprlandPlugins
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
                { };
            })
        ];
      };

      # Provide some binary packages for selected system types.
      packages = forAllSystems
        (system:
          {
            inherit (pkgsFor.${system}) Hyprspace;
          });

      # The default package for 'nix build'. This makes sense if the
      # flake provides only one package or there is a clear "main"
      # package.
      defaultPackage = forAllSystems
        (system: self.packages.${system}.Hyprspace);

      # A NixOS module
      # TODO: Pass in configuration options.
      nixosModules.Hyprspace =
        { pkgs, ... }:
        {
          nixpkgs.overlays = self.overlays;
          environment.systemPackages = [ pkgs.Hyprspace ];
        };
    };
}
