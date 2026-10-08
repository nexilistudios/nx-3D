# Player figures

`counter_terrorist.obj` is a blue tactical officer with a helmet, goggles,
armor plates, knee pads, gloves, boots, and a small equipment pack.
`terrorist.obj` is a tan-clothed fighter with a head wrap, face covering,
utility vest, cargo pockets, gloves, and boots. Both are original low-poly
meshes built for this game.

The OBJ files use one material and a small PNG color atlas each. Spear's OBJ
renderer reads `map_Kd` textures; `Kd` colors alone would render white.
Run `python3 assets/players/generate.py` from any directory to regenerate the
OBJ, MTL, and PNG files. The script uses only Python's standard library.

Models face local -Z. Their origin is the networked player camera position
(eye height), not the feet. The boots reach 64 game units below the
origin, and the headgear extends about 11 units above it. This keeps the
figures aligned with the current camera-based player synchronization.
