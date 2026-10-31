# nx-3D

nx-3D is a multiplayer first-person shooter with a dedicated server and a Vulkan client. The server offers a bomb match and team deathmatch on a generated CS2-style Dust II map. Players choose Counter Terrorist or Terrorist and spawn with a USP or Glock respectively. Weapons can be bought, dropped, and picked up from the map.

## Requirements

- Linux with a Vulkan-capable graphics driver and SDL3 audio support.
- CMake 3.30 or newer and a C++20 compiler.
- The public `spear` and `nexilis` Git submodules.
- The libraries used by `spear` and `nexilis`. The Nix development shell in `flake.nix` supplies these dependencies.

Clone with submodules:

```sh
git clone --recurse-submodules <repository-url>
cd nx-3D
```

## Build and run

On Nix-enabled Linux, enter the development shell with `nix develop`. With the dependencies available, build both programs:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 2
```

### Run the tests locally

From the repository root, run the test script in the Nix development shell:

```sh
nix develop -c ./scripts/run_tests.sh
```

The script configures and builds in `build-tests`, then runs all registered game tests, including the three that link against Spear. If you are already inside the Nix shell, run `./scripts/run_tests.sh` directly. You can pass CTest options to select a case, for example `./scripts/run_tests.sh -R '^bomb_mode$'`.

The **Tests** GitHub Actions workflow runs the full suite on pushes and pull requests. To start it manually, choose **Tests** in the Actions tab and select **Run workflow**, or run `gh workflow run tests.yml`. It uses the submodule revisions recorded in this repository.

The default build compiles Nexilis from the checked-out submodule in the same build as its consumers. Rebuild after updating submodules: newer Nexilis headers with an older `libnexilis.so` can corrupt the client event queues and crash on damage/death messages. Restart all running clients and servers after rebuilding.

Standalone `cmake -S client -B client/build` and `cmake -S server -B server/build` builds also compile their own matching Nexilis library. For a server-only root build, pass `-DNX3D_BUILD_CLIENT=OFF`; disable regression tests with `-DBUILD_TESTING=OFF` if GLM is unavailable. Packagers can opt into an external package with `-DNX3D_USE_SYSTEM_NEXILIS=ON -DNexilis_DIR=/path/to/package`, provided its headers and library match. Installed builds pass `-DNX3D_DATA_DIR=/path/to/data` to compile in the directory holding `assets/` instead of the source tree; for the client it also holds `spear/` with Spear's `assets/` and `shaders/`. The Nix packages do this and build Nexilis in-tree.

Run the server first, then the client in another terminal:

```sh
./build/server/nx-3D-server
./build/client/nx_3D_client
```

The default network ports remain TCP 54200 for authentication, TCP 54202 for the game session, and UDP 54201 for gameplay. To run a server on different ports, pass all three values:

```sh
./build/server/nx-3D-server \
  --tcp-port 55200 \
  --tcp-session-port 55202 \
  --udp-port 55201
```

Clients connecting to that server need its authentication TCP and UDP ports. The session TCP port does not need a client argument because the server sends it to the client during authentication:

```sh
./build/client/nx_3D_client \
  --server game.example.com \
  --username player \
  --tcp-port 55200 \
  --udp-port 55201
```

Each port option is optional. An omitted option keeps its default value, so the existing commands continue to work unchanged. Ports must be numbers from 1 through 65535. The single-dash forms (`-tcp-port`, `-tcp-session-port`, and `-udp-port`) are also accepted for consistency with the existing client arguments.

To run only the server with Docker on Linux, initialize the Nexilis submodule and start Compose:

```sh
git submodule update --init --recursive nexilis
docker compose up --build -d
```

Use `docker compose logs -f server` to view its output and `docker compose down` to stop it. Compose uses host networking on Linux. Remote clients need the configured authentication TCP, session TCP, and UDP ports. Allow these ports through the host firewall and forward them on the router when hosting behind NAT. The client accepts an IPv4 address or hostname in `-server`. The server currently has hardcoded credentials (`password` for players and `root` for administration); change them in `server/server.cc` before exposing it publicly.

The client initially asks for a server address and username. Enter an IPv4 address or domain name (for example, `game.example.com`); the default is `127.0.0.1`. You can also start it with `-server <address> -username <name>` to connect immediately. Select a room with the arrow keys and Enter, then click a team to join.

### Nix

The flake packages both programs. `nix run` starts the client and `.#server` the dedicated server, with or without a checkout:

```sh
nix run github:nexilistudios/nx-3D -- -server 127.0.0.1 -username player
nix run github:nexilistudios/nx-3D#server -- --tcp-port 55200 --tcp-session-port 55202 --udp-port 55201
```

From a checkout, use `nix run` and `nix run .#server`. The packages build Nexilis and Spear in-tree like the CMake build above, but against the nixpkgs Boost, and install the assets under `share/nx-3D`. The client needs a Vulkan driver the Nix-built loader can find, such as NixOS with `hardware.graphics.enable`. `nix flake check` builds both packages, runs the game tests, and boots a NixOS VM with the server module.

GitHub source archives omit submodules, so the flake fetches Nexilis and Spear itself at the commits in its `inputs`. Keep them equal to the submodule commits: after bumping a submodule, put the same revision in the `nexilis` or `spear` URL in `flake.nix` and run `nix flake lock`. The **Nix** workflow fails when they differ. Uncommitted changes in the submodule checkouts do not reach the Nix packages; build those with CMake in `nix develop`.

On NixOS, import the module to run the server as a service. It accepts `tcpPort`, `tcpSessionPort`, and `udpPort`; `openFirewall` opens all three. `programs.nx3dClient.enable = true;` installs the client.

```nix
# Host flake inputs: nx-3D.url = "github:nexilistudios/nx-3D";
imports = [ inputs.nx-3D.nixosModules.default ];
services.nx3dServer = {
  enable = true;
  openFirewall = true;
};
```

## Cube map editor

Edit the map directly in the game’s 3D renderer, or use the top-down Tkinter tool:

```sh
./build/client/nx_3D_client --map-editor
./build/client/nx_3D_client --map-editor --map assets/maps/editor_example/map.brush
./build/client/nx_3D_client --map-editor --map /path/to/new-map.brush
python3 tools/map_editor.py
```

The 3D editor opens an existing map, or creates a floor for a new filename. Click
**Edit in 3D** or **New 3D map** in the top-down editor to launch it there. The
example map includes blocks, a ramp, and an original textured OBJ supply crate.

Click a block face or model to select it. Yellow edges outline the object; cyan
edges mark the face affected by the mouse wheel. **B** creates a grid-sized
block against the surface under the cursor; **O** places the model selected in
the palette. Move objects by dragging, or hold Shift while dragging to move
vertically. Arrow keys move X/Z, Page Up/Down move Y, and R rotates 15 degrees
(Shift reverses). The wheel pushes or pulls one face. M cycles block materials
and paints the selection. Minus/plus change the grid; brackets select OBJ models.
Ctrl+D duplicates, Delete removes, and Ctrl+Z/Y undo/redo. F focuses the selection.

The editor starts at ground level inside the map. Hold the right mouse button to
look around and walk with WASD; Space jumps. Tab keeps mouse look active while
you move. Press G for free flight (Space/C moves up/down, Shift speeds up) and G
again to return to walking. Click an object while looking around to select it,
then use the editing keys or mouse wheel without leaving that view. Escape releases mouse look, then closes the
editor with a save/discard prompt. **Ctrl+S saves**; the previous map is retained
as `.brush.bak`. Edits stay in memory until saved. The top-down editor reloads
changes when the 3D editor closes.

Put additional OBJ models, their MTL files, and referenced textures under
`assets/props/` before opening the editor; its model palette discovers subfolders.
OBJ models can be moved, resized, rotated, duplicated, and deleted like blocks.
They fit the selection box, which also defines their collision on both client and
server. C toggles model collision. See [the example asset](assets/props/README.md).

The complete gameplay map is `assets/maps/dust2_cs2/map.brush`. Both programs load
its brush geometry and OBJ placements; restart clients and the server after
saving edits, and distribute the same map and model assets to every participant.
Custom filenames are supported by the editor and `--map-preview --map <file>`.
The example map is an editing playground; match spawn positions still belong to
the Dust II game modes.

Dust II is generated from editable cubes, including floors, ramps, arches,
tunnels, doors, buildings, and cover. Run `python3 tools/generate_dust2.py` to
regenerate it; regeneration replaces edits in the default map. Use `--output`
for a separate project. The top-down tool requires Tkinter and provides exact
numeric fields for dimensions, rotation, ramp tilt, and materials.
`python3 tools/map_editor.py --check` validates the map, including OBJ records.
See `assets/maps/dust2_cs2/README.md` for the generated plan.

## Controls

| Action                               | Control                                                   |
| ------------------------------------ | --------------------------------------------------------- |
| Move and look                        | WASD and mouse                                            |
| Jump                                 | Space                                                     |
| Crouch slowly without footsteps      | Hold Ctrl                                                 |
| Walk slowly without footsteps        | Hold Shift                                                |
| Fire                                 | Hold left mouse button                                    |
| Reload                               | R                                                         |
| Equip primary, secondary, knife      | 1, 2, 3                                                   |
| Cycle owned utility (HE, smoke, kit) | 4; 5 while carrying the bomb                              |
| Equip carried bomb                   | 4                                                         |
| Throw equipped HE or smoke           | Left mouse button                                         |
| Drop equipped gun or bomb            | G                                                         |
| Pick up nearby gun or bomb           | F                                                         |
| Plant or defuse bomb (bomb match)    | Hold E                                                    |
| Throw held weapon onto the ground    | G                                                         |
| Pick up a nearby weapon              | F                                                         |
| Open buy menu at your team spawn     | B; Tab changes between list, radial, and CS2 grid layouts |
| Change team                          | Escape, then click CT or T                                |
| Leaderboard                          | Hold Tab                                                  |
| Chat                                 | Y, then Enter to send or Escape to cancel                 |
| Pause or resume                      | Escape                                                    |

Every game item is defined as an NXObject in `shared/nx_objects.hh`. An NXObject supplies its ID, kind, inventory slot, purchase price and team, separate world and first-person model paths, transforms, collision size, and combat data. The catalog includes all buy menu entries, the knife, both grenades, and the C4 bomb. `NXObjectInventory` holds owned grenade, smoke, bomb, and kit counts on the server and client; throw events carry the NXObject ID. Weapons, grenades, the bomb, and the defuse kit can be shown in the player's hands; ammo and health refills are applied immediately when bought. First-person placement and animation live in `client/gun/first_person_gun.cc`.

The AK-47, M16, Glock, USP, Desert Eagle, MP5, knife, and HE grenade have low-poly models in `assets/weapons/`. Run
`python3 assets/weapons/generate.py` to regenerate the firearm OBJ and MTL files; the knife and grenade meshes are hand-authored.
The AK-47 uses a black polymer and metal palette; the M16 uses charcoal and
gunmetal. Their materials use generated 1-pixel swatch textures.

Other players appear as custom low-poly OBJ figures: a blue armored Counter Terrorist and a tan Terrorist with a head wrap and utility vest. The figures and their texture atlases live in `assets/players/`; see its README for the generator and dimensions. Figures update when a player's team becomes known or changes. The server tests shots against a box spanning each figure from headgear to boots, shortened when crouching.

The rifles and MP5 fire automatically while held; pistols fire once per click. R reloads a partly empty magazine from a finite reserve of three magazines. Reloading stops when the reserve is empty, and the HUD shows magazine and reserve rounds. Pulling the trigger with an empty magazine plays a local dry-fire click. The server tracks ammunition for hit validation and preserves it in dropped guns; the HUD tracks a local prediction. Buying an ammo refill restores both equipped weapon slots. The server sends idle, running, crouching, and sneaking states from accepted movement input, plus reloading and shooting cues from accepted weapon actions. Remote players show their equipped gun and animate their arms, legs, and gun from those states; sneaking uses the running gait at half speed. Dead players see a red and black WASTED screen and cannot move or turn until respawning. Deathmatch respawns take 2 seconds; bomb rounds have a 4-second break. Fallen player models tumble under Bullet physics. Deathmatch respawns and bomb players eliminated in the previous round receive a fresh starter weapon and reserve.

Reload and planting sounds play locally and are relayed to nearby players through Nexilis positional audio. A planted bomb beeps near its position, faster as detonation approaches. A bomb explosion sound plays for everyone when the server declares an explosion win. Smoke makes a three-second positional hiss when it deploys; the cloud still lasts twelve seconds. The generated utility PCM assets are `assets/sounds/reload.wav`, `planting.wav`, `bomb_beep.wav`, `explosion.wav`, and `smoke.wav`; regenerate them with `python3 assets/sounds/generate.py`.

Each firearm has its own generated gunshot sound. The sound type travels with the positional audio event, so nearby players hear the correct weapon. Gunshots use a louder volume curve and a 2500-unit spatial range so distant firing remains audible across much of the map while fading with distance. The volume control still mutes all sounds at zero. Regenerate the six shot clips with `python3 assets/sounds/generate.py`.

Open **Escape → Settings → Crosshair** to customize your crosshair. Choose Classic,
Compact, Dot, T-style, or Dynamic presets, then adjust length, thickness, gap,
center dot, outline, RGB color, and opacity using the arrows or draggable sliders.
The live preview shows the actual size and a 3× view against Dust, Dark, or Light
backgrounds. Press Space or click the preview to test firing feedback with the
Dynamic style; that style also expands with movement and firing during play.
Changes apply immediately and save automatically. Escape returns to Settings;
Restore defaults resets only the crosshair.

The pause menu Settings screen has a Voicepack option. DEFAULT uses the original victory announcements and damage grunt; ALTERNATIVE uses a different voice and wording. The choice is saved with the other client settings, and changing it plays a preview. Each pack has `terrorists_win.wav`, `counter_terrorists_win.wav`, `hurt.wav`, `bomb_planted.wav`, and `bomb_defused.wav` in `assets/sounds/voicepacks/<pack id>/`. To add a pack, create those five files and add its id and display name to `client/audio/voice_pack.hh`. New voice cues can be added to that file and played through `ClientApp::playVoiceCue`.

Weapons render in camera space to avoid shaking from world-coordinate rounding. Their default pose sits toward the lower right at an angled FPS stance. `client/gun/weapon_motion.hh` controls the subtle walking cadence, time-based recoil recovery, and reload motion; vertical movement, small ground-contact jitter, and respawn teleports do not drive the bob animation. G drops the current weapon just ahead of the player. Press F near a dropped weapon to equip it; the server checks pickup distance and removes the item when equipped.

## Bomb match rules and queue

The Bomb Match room starts when at least one player has chosen each team. If either team is empty, every client sees which team is preventing the game from starting. Players may switch teams from the Escape menu, with no team size or balance restriction. A player who joins during a live round watches until the next one. The HUD shows queue position, score, round, time, life state, and bomb state. Deaths have no immediate respawn. Only living participants can shoot, plant, or defuse. Leaving the room removes a player from the match or queue.

The match uses MR12: twelve rounds per half, side swap after round 12, first to 13 rounds wins, and a 12–12 score after round 24 is a draw. A round lasts 115 seconds. The Terrorists win by eliminating all Counter Terrorists or by letting a planted bomb explode. Counter Terrorists win by eliminating all Terrorists before the bomb is planted, defusing it, or surviving until time expires before a plant. If all Terrorists die after planting, the bomb still needs to be defused. There are five seconds between rounds; a completed match restarts after ten seconds.

Each round assigns and equips the C4 NXObject to one Terrorist. The carrier can select it with 4 and drop it with G; a living Terrorist can pick it up with F. If the carrier dies before planting, the bomb drops at their position. Holding E at a bomb site equips the carried bomb and plants after three seconds. A Counter Terrorist within 120 game units of the planted bomb may hold E for five seconds to defuse, or 2.5 seconds with a defuse kit. Holding E equips an owned kit. Releasing E cancels the action. The planted bomb uses its world model and explodes 40 seconds after planting. The server checks ownership, team, life state, phase, defuse range, player movement, weapon ownership, shot hits, and map occlusion.

Players start with $800 and can hold up to $16000. Each credited kill earns $300; bomb plants and defuses earn $800; round wins earn $3250 and losses earn $1400. Press B while alive in your team's spawn area to open the buy menu. Click an item to buy it, press Tab to cycle the list, radial, and CS2 grid layouts, and press B or Escape to close. The server checks purchases against balance, team, life state, and spawn position. The catalog includes the three pistols, MP5, the existing rifles, ammo refill, health refill, HE and smoke grenades, and a CT-only defuse kit for bomb matches. Left click throws the selected grenade; all clients see its 3D model in flight. HE deals blast damage after a two-second fuse. Smoke deploys Spear's soft particle cloud after two seconds, dissipates over its twelve-second effect, and deals no damage. Smoke detail controls the particle count. Bomb match survivors keep their primary, pistol, remaining ammunition, grenades, smoke, and defuse kit in the next round. Players who die, switch teams, or reach halftime receive a fresh loadout. Grenades currently use a simple arc without world collision; HE blast has no world occlusion.

## Manual releases

Commit and push the default branch, then run **Game release** from the GitHub Actions tab or use `gh workflow run release.yml`. Leave the version input blank to increment the patch version from `VERSION.txt`, or enter a newer version such as `0.2.0`. Watch the run with `gh run list --workflow release.yml`.

After all builds succeed, the workflow updates `VERSION.txt` (the Nix packages read their version from it), tags the release, and publishes five GitHub release assets: Linux and Windows archives for both server and client, plus a source archive. The source archive contains this repository's tracked files and submodule references; clone the tagged repository with submodules to build from source. Linux clients run `run.sh` from the extracted archive; Windows clients run `run.bat`. The release workflow has no schedule.

## Verification and release status

The deathmatch regression runs the game's server death handler through Nexilis and delivers the resulting damage, respawn, and leaderboard messages to two client parsers for 100 consecutive kills. The bomb regression covers admission from the queue, spectator combat restrictions, plant, defuse, elimination, and round score. The animation regression checks idle settling, teleport/airborne suppression, and recoil recovery at 30, 60, and 144 FPS. These tests run without a window or live network sockets.

Before a public release, exercise multiplayer sessions on real Vulkan hardware, including disconnects during combat, repeated weapon drops/pickups, and extended matches. The server uses hardcoded credentials and still trusts client aim direction; deployment configuration and cheat-resistant aiming remain release work. Passing these regressions does not establish production readiness.

## Project layout

| Path                              | Purpose                                                              |
| --------------------------------- | -------------------------------------------------------------------- |
| `client/app/`                     | Client loop, rendering, networking, input, and UI state              |
| `client/gamemode/`                | Bomb and deathmatch presentation, weapon handling, and profiles      |
| `client/gun/`                     | First-person weapon rendering and animation                          |
| `client/crosshair/`, `client/ui/` | Crosshair and menus/HUD                                              |
| `server/`                         | Dedicated server, rooms, and gun logic                               |
| `shared/`                         | Types shared by client and server                                    |
| `assets/`                         | Map, gun models, textures, fonts, and sounds                         |
| `tests/`, `cmake/`                | Gameplay regression checks and shared dependency build configuration |
| `spear/`, `nexilis/`              | Engine and networking submodules                                     |

The server creates one bomb room and one deathmatch room. The client uses Nexilis for room and player synchronization, while `spear` supplies Vulkan rendering, physics, input, and audio. Client assets are resolved relative to the repository root compiled into `PROJECT_ROOT` by CMake, or to `NX3D_DATA_DIR` when that is set.

### Sending input

Spear's `Input` interface accepts the SDL events the client uses. `ClientApp::input` is an `SDLInput` instance; both gameplay and menu loops poll it, so injected input follows the same path as physical input. For example, `app.input.sendKey(SDLK_W, true)` starts moving forward and `app.input.sendKey(SDLK_W, false)` stops. `sendMouseMotion`, `sendMouseButton`, and `sendText` cover camera movement, clicks, and text fields. Use `sendEvent` for other SDL events, such as a mouse wheel or window event. Events are handled on the next client loop iteration.
