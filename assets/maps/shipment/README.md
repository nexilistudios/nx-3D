# Shipment container yard

A compact, bright Deathmatch arena inspired by Shipment. The playable square has
blue, green, and red corrugated containers, open lanes, stacked cover, pallets,
and pale concrete. All structures are editable cube brushes. The colors are
generated textures under `textures/`, and `overview.svg` shows the plan view.

From the repository root, regenerate or open the map with:

```sh
python3 tools/generate_shipment.py
./build/client/nx_3D_client --map-preview --map assets/maps/shipment/map.brush
./build/client/nx_3D_client --map-editor --map assets/maps/shipment/map.brush
```

For a match, start the server and client normally:

```sh
./build/server/nx-3D-server
./build/client/nx_3D_client
```

The first room, **Shipment DM**, loads this map and uses its north and south
lanes for spawns and buying. The second room, **Dust2 Bomb**, loads Dust II.
The server selects each room's map; the client loads it when you select the room.
