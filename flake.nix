{
  description = "nx-3D: a multiplayer FPS (dedicated server + Vulkan client)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # Third-party game engine (Vulkan renderer, physics, audio, UI).
    # Not a flake itself, so it is used as a plain source tree.
    spear = {
      url = "github:NeuronActivation/spear";
      flake = false;
    };

    # Networking library. The upstream repo (`Nexilislib/nexilis`) is private
    # and is fetched over SSH on purpose (matches the .gitmodules URL), and it
    # has no flake.nix of its own. If you already have the submodule checked
    # out locally you can instead use:
    #
    #   nix flake lock --override-input nexilis path:./nexilis
    #
    nexilis = {
      url = "git+ssh://git@github.com/Nexilislib/nexilis.git";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, spear, nexilis }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems f;

      # The game server. Waits for a keypress on stdin to stop, which is fine
      # when run interactively but makes systemd services exit immediately, so
      # the NixOS module feeds it a never-ending stream of input.
      mkServer = pkgs: nexilisPkg: pkgs.stdenv.mkDerivation {
        pname = "nx-3d-server";
        version = "0.1.0";

        src = self.outPath;

        nativeBuildInputs = [ pkgs.cmake ];
        buildInputs = [ nexilisPkg pkgs.boost pkgs.openssl ];

        preConfigure = ''
          # Work on a writable copy so hardcoded paths can be patched.
          cp -a "$src" "$PWD/repo"
          chmod -R u+w "$PWD/repo"
          export NX3D_REPO_ROOT="$PWD/repo"

          # The upstream skeleton expects nexilis to be pre-built at
          # ../nexilis/nexilis/build; in Nix it is a store path instead.
          sed -i "s|set(CMAKE_PREFIX_PATH \".*\")|set(CMAKE_PREFIX_PATH \"${nexilisPkg}\")|" \
            repo/server/CMakeLists.txt

          cd "$PWD/repo/server"
        '';

        # By the install phase the cmake module has cd'd into the build dir.
        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin"
          cp "$PWD/nx-3D-server" "$out/bin/"
          runHook postInstall
        '';

        meta.mainProgram = "nx-3D-server";
      };

      # The game client. Compiles the spear engine as a CMake subdirectory,
      # exactly like a manual build would, but with the source coming from the
      # `spear` flake input instead of an (empty, uninitialised) submodule.
      mkClient = pkgs: nexilisPkg: pkgs.stdenv.mkDerivation {
        pname = "nx-3d-client";
        version = "0.1.0";

        src = self.outPath;

        nativeBuildInputs = [ pkgs.cmake pkgs.pkg-config ];
        buildInputs = [
          nexilisPkg
          pkgs.boost
          pkgs.openssl
          pkgs.sdl3
          pkgs.sdl3-image
          pkgs.sdl3-ttf
          pkgs.glew
          pkgs.libGL
          pkgs.libGLU
          pkgs.vulkan-loader
          pkgs.vulkan-headers
          pkgs.bullet
          pkgs.glm
          pkgs.glslang
          pkgs.spirv-tools
          pkgs.shaderc
          pkgs.stb
          pkgs.vulkan-validation-layers
        ];

        preConfigure = ''
          cp -a "$src" "$PWD/repo"
          chmod -R u+w "$PWD/repo"
          export NX3D_REPO_ROOT="$PWD/repo"

          sed -i "s|set(CMAKE_PREFIX_PATH \".*\")|set(CMAKE_PREFIX_PATH \"${nexilisPkg}\")|" \
            repo/client/CMakeLists.txt

          # Point the CMake subdirectory at the spear flake input instead of
          # the (empty) ../spear submodule.
          sed -i "s|add_subdirectory(../spear/engine spear_engine)|add_subdirectory(${spear}/engine spear_engine)|" \
            repo/client/CMakeLists.txt

          # Assets are installed to $out/share/nx-3D (PROJECT_ROOT is used to
          # resolve the assets/ directory at runtime).
          sed -i "s|set(PROJECT_ROOT \".*\")|set(PROJECT_ROOT \"$out/share/nx-3D\")|" \
            repo/client/CMakeLists.txt

cd "$PWD/repo/client"
          '';

        # By the install phase the cmake module has cd'd into the build dir.
        installPhase = ''
          runHook preInstall
          mkdir -p "$out/bin" "$out/share/nx-3D"
          cp "$PWD/nx_3D_client" "$out/bin/"
          cp -a "$NX3D_REPO_ROOT/assets" "$out/share/nx-3D/assets"
          runHook postInstall
        '';

        meta.mainProgram = "nx_3D_client";
      };
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};

          nexilisPkg = pkgs.stdenv.mkDerivation {
            pname = "nexilis";
            version = "0.1.0";

            src = nexilis;

            nativeBuildInputs = [ pkgs.cmake ];
            buildInputs = [ pkgs.boost pkgs.openssl ];

            preConfigure = ''
              cp -a "$src" "$PWD/nexilis-src"
              chmod -R u+w "$PWD/nexilis-src"
              cd "$PWD/nexilis-src"

              # The CMake project lives in a nested "nexilis" directory
              # (client/server reference ../nexilis/nexilis/build).
              if [ -d nexilis ]; then
                cd nexilis
              fi

              export NEXILIS_SOURCE_ROOT="$PWD"
            '';

            # nexilis's CMakeLists installs libraries and the CMake package
            # config but never copies the headers, so do that here (the
            # exported Nexilis::nexilis target expects them at $out/include).
            # By the install phase the cmake module has cd'd into the build
            # dir, so `make install` resolves the right project.
            installPhase = ''
              runHook preInstall
              make install
              mkdir -p "$out/include"
              cp -a "$NEXILIS_SOURCE_ROOT/include/." "$out/include/"
              runHook postInstall
            '';
          };
        in
        {
          nexilis = nexilisPkg;
          server = mkServer pkgs nexilisPkg;
          client = mkClient pkgs nexilisPkg;
        });

      apps = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          server = self.packages.${system}.server;
          client = self.packages.${system}.client;
        in
        {
          server = { type = "app"; program = "${server}/bin/nx-3D-server"; };
          client = { type = "app"; program = "${client}/bin/nx_3D_client"; };
        });

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          server = self.packages.${system}.server;
          client = self.packages.${system}.client;
        in
        {
          default = pkgs.mkShell {
            name = "nx-3D-dev";

            # Everything both components need to build (and debug) in place.
            inputsFrom = [ server client ];
            packages = [ pkgs.git pkgs.gdb pkgs.cmake pkgs.ninja ];

            shellHook = ''
              echo "nx-3D development shell"
              echo "  build server: cmake -S server -B server/build && cmake --build server/build"
              echo "  build client: cmake -S client -B client/build && cmake --build client/build"
              # The engine enables VK_LAYER_KHRONOS_validation unconditionally,
              # so make the validation layer visible to the Vulkan loader.
              export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
            '';
          };
        });

      nixosModules.default = { config, lib, pkgs, ... }:
        let
          packages = self.packages.${pkgs.system};
        in
        {
          options.services.nx3dServer = {
            enable = lib.mkEnableOption "the headless nx-3D dedicated game server";

            package = lib.mkOption {
              type = lib.types.package;
              default = packages.server;
              description = "The nx-3D server package to run as a service.";
            };
          };

          options.programs.nx3dClient = {
            enable = lib.mkEnableOption "the nx-3D game client";

            package = lib.mkOption {
              type = lib.types.package;
              default = packages.client;
              description = "The nx-3D client package to install.";
            };
          };

          config = lib.mkMerge [
            (lib.mkIf config.services.nx3dServer.enable {
              systemd.services.nx3dServer = {
                description = "nx-3D dedicated game server";
                wantedBy = [ "multi-user.target" ];
                after = [ "network.target" ];

                serviceConfig = {
                  Type = "simple";
                  # The server only stops when it reads input on stdin, so feed
                  # it a stream that never ends (otherwise it exits on EOF).
                  ExecStart = "${pkgs.bash}/bin/bash -c 'tail -f /dev/null | ${config.services.nx3dServer.package}/bin/nx-3D-server'";
                  DynamicUser = true;
                  Restart = "on-failure";
                  RestartSec = 3;
                  ProtectSystem = "strict";
                  ProtectHome = true;
                  NoNewPrivileges = true;
                };
              };
            })

            (lib.mkIf config.programs.nx3dClient.enable {
              environment.systemPackages = [
                config.programs.nx3dClient.package
                pkgs.vulkan-loader
                pkgs.mesa
                pkgs.sdl3
              ];
            })
          ];
        };
    };
}