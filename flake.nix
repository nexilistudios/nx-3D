{
  description = "nx-3D: a multiplayer FPS (dedicated server + Vulkan client)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # The same commits as the nexilis and spear submodules (CI checks this).
    # GitHub archives carry no submodules, so the flake fetches them itself.
    # After bumping a submodule, update its rev here and run `nix flake lock`.
    nexilis = {
      url = "github:Nexilislib/nexilis/b98bb2b6be02e04113a6892a9672136c313f32cc";
      flake = false;
    };
    spear = {
      url = "github:NeuronActivation/spear/1bd16f7be3a1741f2a5e4150e981f4f6d82ea889";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      nexilis,
      spear,
    }:
    let
      forAllSystems =
        f:
        nixpkgs.lib.genAttrs [
          "x86_64-linux"
          "aarch64-linux"
        ] (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (
        pkgs:
        let
          build =
            component:
            pkgs.callPackage ./nix/package.nix {
              nexilis-src = nexilis;
              spear-src = spear;
              inherit component;
            };
        in
        rec {
          server = build "server";
          client = build "client";
          # `nix run` launches the client.
          default = client;
        }
      );

      checks = forAllSystems (pkgs: {
        inherit (self.packages.${pkgs.stdenv.hostPlatform.system}) server client;
        nixos-module = pkgs.testers.runNixOSTest (import ./nix/test.nix self);
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          name = "nx-3D-dev";
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.client ];
          packages = [
            pkgs.clang-tools
            pkgs.gdb
          ];

          shellHook = ''
            # A host-wide LD_LIBRARY_PATH (e.g. from NixOS audio/GPU setups)
            # can leak libraries built against a different glibc into the
            # link step ("undefined reference to sqrtf@GLIBC_2.43").
            unset LD_LIBRARY_PATH
            # Spear enables VK_LAYER_KHRONOS_validation when the loader finds it.
            export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
          '';
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-tree);

      nixosModules.default = import ./nix/module.nix self;
    };
}
