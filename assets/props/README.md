# OBJ props for the 3D editor

`example_crate.obj` is an original low-poly supply crate with wooden panels,
metal corner bars, and a yellow label. Its material file and three tiny texture
swatches are included. Regenerate them with `python3 assets/props/generate_example.py`.

Open the example playground:

```sh
./build/client/nx_3D_client --map-editor --map assets/maps/editor_example/map.brush
```

Press **O** to place another crate at the cursor's surface. Select it, then drag,
rotate with **R**, resize a face with the mouse wheel, or duplicate with **Ctrl+D**.
**C** switches between box collision and a decorative model. **Ctrl+S** saves.

For another model, create a folder such as `assets/props/my_model/` and put its
OBJ, MTL, and textures there. The OBJ should name its MTL with `mtllib`; the MTL
should use relative `map_Kd` texture paths. Reopen the editor, choose the model
with **[ / ]**, and press **O**. Models need nonzero width, height, and depth.
Models without an MTL or diffuse textures use the engine's white fallback.

Placement dimensions are world-space box sizes, independent of the OBJ's source
units and origin. Nonuniform resizing stretches the mesh to that box. Collision
and picking use the rotated box rather than detailed OBJ triangles. Ship all
referenced assets together with the map.

Placements are stored in the same `.brush` file as versioned comment records:

```text
# nx3d-model-v1 "props/example_crate.obj" 0 32 64 32 32 32 15 1
```

The values are asset path, center XYZ, half extents XYZ, yaw in degrees, and solid
flag. Both native 3D and top-down editors preserve these records when saving.
