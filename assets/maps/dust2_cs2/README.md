# Generated Dust II

This is a procedural, editable reconstruction of the CS2 Dust II layout. The
generator reads no imported map geometry or textures. All floors, sloped ramps,
walls, facades, tunnel roofs, arches, open doors, crates and cars are deformed
cubes. The result is a stylized recreation, not an exact copy of Valve's artwork.

The hand-authored plan uses the [CS2 Dust II overview](https://csnades.gg/dust2/callouts)
as a layout reference. That image is not bundled or read by the generator.

From the repository root:

```sh
python3 tools/generate_dust2.py
python3 tools/map_editor.py
./build/client/nx_3D_client --map-preview
```

`map.brush` is the complete map loaded by both client and server. `textures/`
contains generated, deterministic materials. `overview.svg` is a plan view of
the actual generated geometry, and `layout.json` supplies the editor's labels.
The runtime builds one render batch per material and one static collision mesh
from the same compiled cube surfaces. Hidden and overlapping faces are removed
before drawing and collision are built, and the map uses fixed daylight.

The editor can move, resize, duplicate or delete all generated geometry, change
materials and rotation, and tilt cube tops to create ramps. Filter by Floors,
Walls, Cover or Details to reach overlapping geometry. Save before opening 3D
preview. Preview has free flight with WASD, Space/C and Shift; Escape exits.
Restart clients and the server after edits, and distribute the same map to all
participants. Regenerating replaces edits in the default map; use `--output`
to generate a separate project when needed.
