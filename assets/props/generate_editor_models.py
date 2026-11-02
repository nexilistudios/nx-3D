"""Generate the built-in OBJ models offered by the map editor library."""
from pathlib import Path
import math
import struct
import zlib

ROOT = Path(__file__).resolve().parent
COLORS = {
    "steel": (49, 129, 195),
    "dark": (51, 65, 76),
    "concrete": (185, 197, 199),
    "wood": (179, 121, 65),
    "orange": (244, 124, 40),
    "white": (242, 236, 214),
}


def pixel(rgb: tuple[int, int, int]) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes((0, *rgb)))) + chunk(b"IEND", b""))


class Model:
    def __init__(self, name: str) -> None:
        self.name = name
        self.vertices: list[tuple[float, float, float]] = []
        self.lines = [f"# Built-in map editor {name}", f"mtllib {name}.mtl",
                      "vt 0 0", "vt 1 0", "vt 1 1", "vt 0 1"]

    def mesh(self, vertices: list[tuple[float, float, float]], faces: list[tuple[int, int, int]],
             material: str) -> None:
        start = len(self.vertices)
        self.vertices.extend(vertices)
        self.lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices)
        self.lines.append(f"usemtl {material}")
        for a, b, c in faces:
            self.lines.append(f"f {start+a+1}/1 {start+b+1}/2 {start+c+1}/3")

    def box(self, center: tuple[float, float, float], half: tuple[float, float, float],
            material: str) -> None:
        x, y, z = center
        hx, hy, hz = half
        vertices = [(x + sx * hx, y + sy * hy, z + sz * hz)
                    for sy, sz, sx in ((-1,-1,-1),(-1,-1,1),(-1,1,-1),(-1,1,1),
                                       (1,-1,-1),(1,-1,1),(1,1,-1),(1,1,1))]
        faces = [(0,2,3),(0,3,1),(4,5,7),(4,7,6),
                 (0,1,5),(0,5,4),(2,6,7),(2,7,3),
                 (0,4,6),(0,6,2),(1,3,7),(1,7,5)]
        self.mesh(vertices, faces, material)

    def cylinder(self, radius: float, bottom: float, top: float, material: str,
                 segments: int = 12, top_radius: float | None = None) -> None:
        if top_radius is None:
            top_radius = radius
        vertices = []
        for y, r in ((bottom, radius), (top, top_radius)):
            vertices.extend((math.cos(i * 2 * math.pi / segments) * r, y,
                             math.sin(i * 2 * math.pi / segments) * r)
                            for i in range(segments))
        faces = []
        for i in range(1, segments - 1):
            faces.extend(((0, i + 1, i), (segments, segments + i, segments + i + 1)))
        for i in range(segments):
            j = (i + 1) % segments
            faces.extend(((i, j, segments + j), (i, segments + j, segments + i)))
        self.mesh(vertices, faces, material)

    def save(self) -> None:
        (ROOT / f"{self.name}.obj").write_text("\n".join(self.lines) + "\n")
        (ROOT / f"{self.name}.mtl").write_text("\n".join(
            f"newmtl {name}\nKd {r/255:.4f} {g/255:.4f} {b/255:.4f}\n"
            f"map_Kd editor_{name}.png\n"
            for name, (r, g, b) in COLORS.items()))


def generate() -> None:
    for name, color in COLORS.items():
        (ROOT / f"editor_{name}.png").write_bytes(pixel(color))

    barrel = Model("editor_barrel")
    barrel.cylinder(.70, -1, 1, "steel")
    for y in (-.80, .0, .80):
        barrel.cylinder(.74, y - .05, y + .05, "dark")
    barrel.save()

    barrier = Model("editor_barrier")
    barrier.box((0, -.65, 0), (1, .30, .58), "concrete")
    barrier.box((0, .12, 0), (1, .48, .31), "concrete")
    for x in (-.65, .65):
        barrier.box((x, .62, -.315), (.12, .15, .02), "orange")
    barrier.save()

    pallet = Model("editor_pallet")
    for z in (-.72, 0, .72):
        pallet.box((0, -.75, z), (1, .12, .16), "dark")
    for x in (-.7, -.35, 0, .35, .7):
        pallet.box((x, -.48, 0), (.13, .12, 1), "wood")
    for z in (-.65, 0, .65):
        pallet.box((0, .05, z), (.85, .40, .18), "wood")
    pallet.save()

    pillar = Model("editor_pillar")
    pillar.box((0, -.88, 0), (.75, .12, .75), "concrete")
    pillar.cylinder(.43, -1, .85, "white", 12)
    pillar.box((0, .88, 0), (.65, .12, .65), "concrete")
    pillar.save()

    cone = Model("editor_cone")
    cone.box((0, -.91, 0), (.72, .09, .72), "dark")
    cone.cylinder(.49, -.83, .90, "orange", 12, .07)
    cone.cylinder(.27, -.05, .10, "white", 12, .22)
    cone.save()


if __name__ == "__main__":
    generate()
