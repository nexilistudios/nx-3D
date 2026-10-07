# nx-3D

nx-3D is a multiplayer first-person shooter with a dedicated server and a Vulkan client. The current game mode is team deathmatch on de_dust2. Players join a room, choose Counter Terrorist or Terrorist, and spawn with an M4 or AK-47 respectively. Weapons can be dropped and picked up from the map.

## Requirements

- Linux with a Vulkan-capable graphics driver and SDL3 audio support.
- CMake 3.30 or newer and a C++20 compiler.
- The `spear` and `nexilis` Git submodules, including a built Nexilis CMake package. Nexilis is a private SSH dependency in this repository.
- The libraries used by `spear` and `nexilis`. The Nix development shell in `flake.nix` supplies these dependencies.

Clone with submodules:

```sh
git clone --recurse-submodules <repository-url>
cd nx-3D
```

## Build and run

On Nix-enabled Linux, enter the development shell and build the dependencies and both programs:

```sh
nix develop
cmake -S nexilis/nexilis -B nexilis/nexilis/build
cmake --build nexilis/nexilis/build
cmake -S server -B server/build
cmake --build server/build
cmake -S client -B client/build
cmake --build client/build
```

`server/CMakeLists.txt` and `client/CMakeLists.txt` look for the Nexilis package in `nexilis/nexilis/build`. If your Nexilis checkout has a different layout, set `CMAKE_PREFIX_PATH` in those files or provide the package location when configuring CMake. The client builds the `spear/engine` submodule as part of its CMake build.

Run the server first, then the client in another terminal:

```sh
./server/build/nx-3D-server
./client/build/nx_3D_client
```

The client initially asks for a server address and username. The default address is `127.0.0.1`. You can also start it with `-server <address> -username <name>` to connect immediately. Select a room with the arrow keys and Enter, then choose a team with `1` or `2`.

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
| `spear/`, `nexilis/` | Engine and networking submodules |

The server creates three deathmatch rooms. The client uses Nexilis for room and player synchronization, while `spear` supplies Vulkan rendering, physics, input, and audio. Client assets are resolved relative to the repository root compiled into `PROJECT_ROOT` by CMake.
