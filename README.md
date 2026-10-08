# nx-3D

nx-3D is a multiplayer first-person shooter with a dedicated server and a Vulkan client. The current game mode is team deathmatch on de_dust2. Players join a room, choose Counter Terrorist or Terrorist, and spawn with an M4 or AK-47 respectively. Weapons can be dropped and picked up from the map.

## Requirements

- Linux with a Vulkan-capable graphics driver and SDL3 audio support.
- CMake 3.30 or newer and a C++20 compiler.
- The `spear` and `nexilis` Git submodules. Nexilis is a private SSH dependency in this repository.
- The libraries used by `spear` and `nexilis`. The Nix development shell in `flake.nix` supplies these dependencies.

Clone with submodules:

```sh
git clone --recurse-submodules <repository-url>
cd nx-3D
```

## Build and run

On Nix-enabled Linux, enter the development shell with `nix develop`. With the dependencies available, build both programs and run the regression checks:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The default build compiles Nexilis from the checked-out submodule in the same build as its consumers. Rebuild after updating submodules: newer Nexilis headers with an older `libnexilis.so` can corrupt the client event queues and crash on damage/death messages. Restart all running clients and servers after rebuilding.

Standalone `cmake -S client -B client/build` and `cmake -S server -B server/build` builds also compile their own matching Nexilis library. For a server-only root build, pass `-DNX3D_BUILD_CLIENT=OFF`; disable regression tests with `-DBUILD_TESTING=OFF` if GLM is unavailable. Packagers can opt into an external package with `-DNX3D_USE_SYSTEM_NEXILIS=ON -DNexilis_DIR=/path/to/package`, provided its headers and library match. The Nix packages use this explicit external-package mode.

Run the server first, then the client in another terminal:

```sh
./build/server/nx-3D-server
./build/client/nx_3D_client
```

The client initially asks for a server address and username. The default address is `127.0.0.1`. You can also start it with `-server <address> -username <name>` to connect immediately. Select a room with the arrow keys and Enter, then choose a team with `1` or `2`.

To reopen the most recent Codex conversation for this repository, run:

```sh
./resume-codex.sh
```

The script can be run from any directory. Extra arguments are passed to `codex resume`.

The flake also exposes packaged programs through `nix run .#server` and `nix run .#client`. Its Nexilis input uses SSH access to the private repository.

## Controls

| Action | Control |
| --- | --- |
| Move and look | WASD and mouse |
| Jump | Space |
| Fire | Hold left mouse button |
| Drop held weapon | G |
| Pick up a weapon | Walk near a dropped weapon |
| Leaderboard | Hold Tab |
| Chat | Y, then Enter to send or Escape to cancel |
| Pause or resume | Escape |

The equipped gun appears beside the camera and follows the view. It has walk bob and recoil. The gun mesh is selected from the team's weapon profile or the weapon picked up from the ground. First-person placement and animation live in `client/gun/first_person_gun.cc`; per-weapon mesh paths, scale, and center are in `client/gamemode/weapon_profiles.hh`.

Weapons render in camera space to avoid shaking from world-coordinate rounding. Their default pose is angled inward with a slight roll. `client/gun/weapon_motion.hh` controls the subtle walking cadence and time-based recoil recovery; vertical movement, small ground-contact jitter, and respawn teleports do not drive the bob animation.

## Verification and release status

The deathmatch regression runs the game's server death handler through Nexilis and delivers the resulting damage, respawn, and leaderboard messages to two client parsers for 100 consecutive kills. The animation regression checks idle settling, teleport/airborne suppression, and recoil recovery at 30, 60, and 144 FPS. These tests run without a window or live network sockets.

Before a public release, exercise multiplayer sessions on real Vulkan hardware, including disconnects during combat, repeated weapon drops/pickups, and extended matches. The current game still accepts client-reported hits/positions and uses hardcoded server credentials; server-authoritative hit validation and deployment configuration remain release work. Passing these regressions does not establish production readiness.

## Project layout

| Path | Purpose |
| --- | --- |
| `client/app/` | Client loop, rendering, networking, input, and UI state |
| `client/gamemode/` | Deathmatch rules, weapon handling, and weapon profiles |
| `client/gun/` | First-person weapon rendering and animation |
| `client/crosshair/`, `client/ui/` | Crosshair and menus/HUD |
| `server/` | Dedicated server, rooms, and gun logic |
| `shared/` | Types shared by client and server |
| `assets/` | Map, gun models, textures, fonts, and sounds |
| `tests/`, `cmake/` | Gameplay regression checks and shared dependency build configuration |
| `spear/`, `nexilis/` | Engine and networking submodules |

The server creates three deathmatch rooms. The client uses Nexilis for room and player synchronization, while `spear` supplies Vulkan rendering, physics, input, and audio. Client assets are resolved relative to the repository root compiled into `PROJECT_ROOT` by CMake.
