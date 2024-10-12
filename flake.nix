{
  description = "Hyprspace";

  inputs = {
    systems = {
      type = "github";
      owner = "nix-systems";
      repo = "default-linux";
    };
    hyprland = {
      owner = "hyprwm";
      repo = "Hyprland";
      type = "github";
      inputs.systems.follows = "systems";
    };
  };

  outputs = {
    self,
    systems,
    hyprland,
    ...
  }: let
    inherit (builtins) concatStringsSep elemAt head readFile split substring;
    inherit (hyprland.inputs) nixpkgs;

    perSystem = attrs:
      nixpkgs.lib.genAttrs (import systems) (system:
        attrs system (import nixpkgs {
          inherit system;
          overlays = [hyprland.overlays.hyprland-packages];
        }));

    # Generate version
    mkDate = longDate: (concatStringsSep "-" [
      (substring 0 4 longDate)
      (substring 4 2 longDate)
      (substring 6 2 longDate)
    ]);

    version =
      (head (split "'"
        (elemAt
          (split " version: '" (readFile ./meson.build))
          2)))
      + "+date=${mkDate (self.lastModifiedDate or "19700101")}_${self.shortRev or "dirty"}";
  in {
    packages = perSystem (system: pkgs: {
      Hyprspace = let
        hyprlandPkg = hyprland.packages.${system}.hyprland;
      in
        pkgs.gcc14Stdenv.mkDerivation {
          pname = "Hyprspace";
          inherit version;
          src = ./.;

          nativeBuildInputs = hyprlandPkg.nativeBuildInputs;
          buildInputs = [hyprlandPkg] ++ hyprlandPkg.buildInputs;
          dontUseCmakeConfigure = true;

          meta = with pkgs.lib; {
            homepage = "https://github.com/KZDKM/Hyprspace";
            description = "Workspace overview plugin for Hyprland";
            license = licenses.gpl2Only;
            platforms = platforms.linux;
          };
        };
      default = self.packages.${system}.Hyprspace;
    });

    devShells = perSystem (system: pkgs: {
      default = pkgs.mkShell {
        name = "Hyprspace-shell";
        nativeBuildInputs = with pkgs; [gcc14];
        buildInputs = [hyprland.packages.${system}.hyprland];
        inputsFrom = [
          hyprland.packages.${system}.hyprland
          self.packages.${system}.Hyprspace
        ];
        shellHook = ''
          meson setup build --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
      };
    });

    formatter = perSystem (_: pkgs: pkgs.alejandra);
  };
}
