# The nx-3D dedicated server or game client, built from the top-level CMake
# project. Nexilis and Spear are compiled in-tree from the pinned flake inputs,
# the same way a local build compiles them from the submodules.
{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  makeWrapper,
  boost,
  openssl,
  bullet,
  glm,
  sdl3,
  sdl3-image,
  sdl3-ttf,
  sdl3-mixer,
  glew,
  libGL,
  vulkan-headers,
  vulkan-loader,
  glslang,
  spirv-tools,
  shaderc,

  nexilis-src,
  spear-src,
  component ? "client",
}:

assert lib.assertOneOf "component" component [
  "server"
  "client"
];

let
  isClient = component == "client";
in
stdenv.mkDerivation (finalAttrs: {
  pname = "nx-3d-${component}";
  version = lib.fileContents ../VERSION.txt;

  # Only what CMake reads, so edits to docs or Nix files do not rebuild. The
  # root project always configures server/, and the tests use client headers.
  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions (
      [
        ../CMakeLists.txt
        ../cmake
        ../client
        ../server
        ../shared
        ../assets
      ]
      ++ lib.optional finalAttrs.doCheck ../tests
    );
  };

  postUnpack = ''
    cp -r --no-preserve=mode ${nexilis-src} "$sourceRoot/nexilis"
  ''
  + lib.optionalString isClient ''
    cp -r --no-preserve=mode ${spear-src} "$sourceRoot/spear"
  '';

  # Nexilis assumes any Nix build has its vendored Boost submodules, which a
  # GitHub source archive does not contain. Take its CI path instead, which
  # uses find_package(Boost) like the Ubuntu CI and Docker builds.
  postPatch = ''
    substituteInPlace nexilis/nexilis/CMakeLists.txt \
      --replace-fail 'if(DEFINED ENV{NIX_BUILD_CORES})' 'if(FALSE)'
  '';
  env.CI = "1";

  nativeBuildInputs = [
    cmake
    ninja
  ]
  ++ lib.optionals isClient [
    pkg-config
    makeWrapper
  ];

  buildInputs = [
    boost
    openssl
    bullet
    glm
  ]
  ++ lib.optionals isClient [
    sdl3
    sdl3-image
    sdl3-ttf
    sdl3-mixer
    glew
    libGL
    vulkan-headers
    vulkan-loader
    glslang
    spirv-tools
    shaderc
  ];

  cmakeFlags = [
    (lib.cmakeBool "NX3D_BUILD_CLIENT" isClient)
    (lib.cmakeFeature "NX3D_DATA_DIR" "${placeholder "out"}/share/nx-3D")
    # libnexilis.so is installed to $out/lib, which stdenv already adds to the
    # RPATH. A build-tree RPATH would point into the sandbox.
    (lib.cmakeBool "CMAKE_SKIP_BUILD_RPATH" true)
  ];

  # Root builds also configure the server; only the client is wanted here.
  ninjaFlags = lib.optional isClient "nx_3D_client";

  # The game-logic tests; the Spear ones need the client build.
  doCheck = !isClient;
  preCheck = ''
    export LD_LIBRARY_PATH="$PWD/nexilis''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  '';

  # The same layout as the release archives, under $out/share/nx-3D.
  installPhase = ''
    runHook preInstall

    install -Dm755 -t "$out/lib" nexilis/libnexilis.so
    data="$out/share/nx-3D"
  ''
  + (
    if isClient then
      ''
        install -Dm755 -t "$out/bin" client/nx_3D_client
        mkdir -p "$data/spear"
        cp -r ../assets "$data/assets"
        cp -r ../spear/engine/assets ../spear/engine/shaders "$data/spear/"

        # A host-wide LD_LIBRARY_PATH (some NixOS audio setups export one)
        # beats the RUNPATH and can load libraries built for a newer glibc.
        wrapProgram "$out/bin/nx_3D_client" --unset LD_LIBRARY_PATH
      ''
    else
      ''
        install -Dm755 -t "$out/bin" server/nx-3D-server
        install -Dm644 -t "$data/assets/de_dust2/source" ../assets/de_dust2/source/de_dust2.obj
      ''
  )
  + ''
    runHook postInstall
  '';

  meta = {
    description =
      if isClient then
        "Vulkan client for the nx-3D multiplayer FPS"
      else
        "Dedicated server for the nx-3D multiplayer FPS";
    homepage = "https://github.com/nexilistudios/nx-3D";
    platforms = lib.platforms.linux;
    mainProgram = if isClient then "nx_3D_client" else "nx-3D-server";
  };
})
