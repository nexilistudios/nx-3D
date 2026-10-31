#!/usr/bin/env python3
"""Generate the editor's original supply crate OBJ and small material swatches."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parent
colors = {"wood": (142, 99, 54), "metal": (55, 65, 72), "label": (224, 180, 58)}
for name, rgb in colors.items():
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", 1, 1, 8, 2, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(bytes((0, *rgb)))) + chunk(b"IEND", b"")
    (root / f"crate_{name}.png").write_bytes(data)

lines = ["# Original nx-3D editor example: supply crate", "mtllib example_crate.mtl", "vt 0 0", "vt 1 0", "vt 1 1", "vt 0 1"]
vertices = 0

def box(center, half, material):
    global vertices
    cx, cy, cz = center
    hx, hy, hz = half
    for x, y, z in ((-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1),(-1,1,-1),(1,1,-1),(1,1,1),(-1,1,1)):
        lines.append(f"v {cx+x*hx:g} {cy+y*hy:g} {cz+z*hz:g}")
    lines.append(f"usemtl {material}")
    for face in ((0,1,2,3),(4,7,6,5),(0,4,5,1),(3,2,6,7),(0,3,7,4),(1,5,6,2)):
        # Triangulated faces also work with simple OBJ loaders.
        for tri in ((0,1,2),(0,2,3)):
            lines.append("f " + " ".join(f"{vertices+face[i]+1}/{i+1}" for i in tri))
    vertices += 8

box((0,0,0),(.94,.94,.94), "wood")
for axis in (0,1,2):
    other = [i for i in range(3) if i != axis]
    for a in (-.95,.95):
        for b in (-.95,.95):
            center = [0.,0.,0.]; center[other[0]]=a; center[other[1]]=b
            half = [.07,.07,.07]; half[axis]=1.02
            box(center, half, "metal")
box((0,0,-.953),(.28,.24,.025),"label")
(root/"example_crate.obj").write_text("\n".join(lines)+"\n")
(root/"example_crate.mtl").write_text("\n".join(
    f"newmtl {name}\nKd {r/255:.4f} {g/255:.4f} {b/255:.4f}\nmap_Kd crate_{name}.png\n"
    for name,(r,g,b) in colors.items()))
