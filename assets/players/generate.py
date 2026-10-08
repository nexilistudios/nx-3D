"""Build the original low-poly T and CT OBJ characters and their color atlases.

Run from the repository root with ``python3 assets/players/generate.py``.
The generated meshes use the camera/remote-player position as eye height:
boots are 64 game units below it and the helmet is about 11 units above.
"""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path


DESTINATION = Path(__file__).resolve().parent

PALETTES = {
    "counter_terrorist": {
        "uniform": (69, 104, 139),
        "armor": (41, 61, 82),
        "helmet": (54, 79, 104),
        "visor": (15, 27, 35),
        "skin": (176, 140, 109),
        "boots": (21, 28, 34),
        "gloves": (28, 35, 41),
        "detail": (122, 157, 183),
        "belt": (25, 36, 47),
        "badge": (218, 202, 151),
    },
    "terrorist": {
        "uniform": (176, 145, 98),
        "armor": (93, 75, 53),
        "helmet": (126, 56, 47),  # Head wrap / scarf.
        "visor": (46, 39, 35),   # Face mask.
        "skin": (177, 123, 91),
        "boots": (44, 37, 32),
        "gloves": (67, 54, 43),
        "detail": (213, 186, 125),
        "belt": (67, 53, 41),
        "badge": (171, 73, 56),
    },
}


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    content = kind + payload
    return struct.pack(">I", len(payload)) + content + struct.pack(">I", zlib.crc32(content))


def write_png(path: Path, colors: list[tuple[int, int, int]]) -> None:
    """Write an RGB PNG atlas without external image dependencies."""
    swatch = 16
    width = 1 << math.ceil(math.log2(len(colors) * swatch))
    height = 16
    row = bytearray()
    for x in range(width):
        row.extend(colors[min(x // swatch, len(colors) - 1)])
    pixels = b"".join(b"\x00" + bytes(row) for _ in range(height))
    header = b"\x89PNG\r\n\x1a\n"
    header += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    header += png_chunk(b"IDAT", zlib.compress(pixels, 9))
    header += png_chunk(b"IEND", b"")
    path.write_bytes(header)


class Figure:
    def __init__(self, name: str, palette: dict[str, tuple[int, int, int]]) -> None:
        self.name = name
        self.palette = palette
        self.vertices: list[tuple[float, float, float]] = []
        self.faces: list[tuple[int, int, int, str]] = []
        self.used_colors: dict[str, tuple[int, int, int]] = {}

    def color(self, material: str, shade: str = "base") -> str:
        key = f"{material}_{shade}"
        if key not in self.used_colors:
            factor = {"base": 1.0, "light": 1.18, "dark": 0.72}[shade]
            self.used_colors[key] = tuple(min(255, round(c * factor)) for c in self.palette[material])
        return key

    def triangle(self, a: tuple[float, float, float], b: tuple[float, float, float],
                 c: tuple[float, float, float], color: str) -> None:
        start = len(self.vertices) + 1
        self.vertices.extend((a, b, c))
        self.faces.append((start, start + 1, start + 2, color))

    def quad(self, a: tuple[float, float, float], b: tuple[float, float, float],
             c: tuple[float, float, float], d: tuple[float, float, float], color: str) -> None:
        self.triangle(a, b, c, color)
        self.triangle(a, c, d, color)

    def tapered_box(self, material: str, x: float, z: float, y_bottom: float, y_top: float,
                    bottom_w: float, top_w: float, bottom_d: float, top_d: float) -> None:
        """A six-sided trunk/plate with independent top and bottom widths."""
        b, t = bottom_w / 2, top_w / 2
        bd, td = bottom_d / 2, top_d / 2
        p = [
            (x-b, y_bottom, z-bd), (x+b, y_bottom, z-bd),
            (x+b, y_bottom, z+bd), (x-b, y_bottom, z+bd),
            (x-t, y_top, z-td), (x+t, y_top, z-td),
            (x+t, y_top, z+td), (x-t, y_top, z+td),
        ]
        self.quad(p[0], p[4], p[5], p[1], self.color(material))       # front, -Z
        self.quad(p[2], p[6], p[7], p[3], self.color(material, "dark"))  # back
        self.quad(p[3], p[7], p[4], p[0], self.color(material, "dark"))  # left
        self.quad(p[1], p[5], p[6], p[2], self.color(material))          # right
        self.quad(p[4], p[7], p[6], p[5], self.color(material, "light"))  # top
        self.quad(p[3], p[0], p[1], p[2], self.color(material, "dark"))   # bottom

    def box(self, material: str, x: float, y: float, z: float,
            width: float, height: float, depth: float) -> None:
        self.tapered_box(material, x, z, y-height/2, y+height/2,
                         width, width, depth, depth)

    def limb(self, material: str, start: tuple[float, float, float],
             end: tuple[float, float, float], radius_start: float,
             radius_end: float, sides: int = 8) -> None:
        """A tapered low-poly cylinder between two joints."""
        direction = tuple(end[i] - start[i] for i in range(3))
        length = math.sqrt(sum(value * value for value in direction))
        axis = tuple(value / length for value in direction)
        reference = (0.0, 0.0, 1.0)
        ux = axis[1] * reference[2] - axis[2] * reference[1]
        uy = axis[2] * reference[0] - axis[0] * reference[2]
        uz = axis[0] * reference[1] - axis[1] * reference[0]
        ul = math.sqrt(ux*ux + uy*uy + uz*uz)
        u = (ux/ul, uy/ul, uz/ul)
        v = (axis[1]*u[2] - axis[2]*u[1],
             axis[2]*u[0] - axis[0]*u[2],
             axis[0]*u[1] - axis[1]*u[0])
        rings = []
        for center, radius in ((start, radius_start), (end, radius_end)):
            rings.append([
                tuple(center[j] + radius * (math.cos(i*math.tau/sides)*u[j] +
                                            math.sin(i*math.tau/sides)*v[j]) for j in range(3))
                for i in range(sides)
            ])
        for i in range(sides):
            nxt = (i + 1) % sides
            shade = "light" if i in (1, 2) else "dark" if i in (5, 6) else "base"
            self.quad(rings[0][i], rings[0][nxt], rings[1][nxt], rings[1][i],
                      self.color(material, shade))
            self.triangle(start, rings[0][nxt], rings[0][i], self.color(material, "dark"))
            self.triangle(end, rings[1][i], rings[1][nxt], self.color(material, "light"))

    def ellipsoid(self, material: str, center: tuple[float, float, float],
                  radii: tuple[float, float, float], upper_only: bool = False) -> None:
        rings = 3 if upper_only else 6
        columns = 12

        def point(theta: float, phi: float) -> tuple[float, float, float]:
            return (center[0] + radii[0] * math.sin(theta) * math.cos(phi),
                    center[1] + radii[1] * math.cos(theta),
                    center[2] + radii[2] * math.sin(theta) * math.sin(phi))

        max_theta = math.pi/2 if upper_only else math.pi
        for row in range(rings):
            theta0 = max_theta * row/rings
            theta1 = max_theta * (row+1)/rings
            for col in range(columns):
                phi0 = math.tau * col/columns
                phi1 = math.tau * (col+1)/columns
                a, b = point(theta0, phi0), point(theta0, phi1)
                c, d = point(theta1, phi1), point(theta1, phi0)
                shade = "light" if row == 0 else "dark" if row >= 4 else "base"
                self.quad(a, b, c, d, self.color(material, shade))

    def shared_body(self, terrorist: bool) -> None:
        trouser = "armor" if terrorist else "uniform"
        self.tapered_box("uniform", 0, 0, -30, -10, 15, 22, 9.5, 11)
        self.tapered_box(trouser, 0, 0, -34, -27, 17, 15, 10, 9)
        self.box("belt", 0, -28.5, -0.1, 17.5, 2.7, 10.5)
        self.box("detail", 0, -28.4, -5.5, 3.5, 2.7, 0.9)
        for side in (-1, 1):
            hip = (side * 4.9, -33, 0)
            knee = (side * 5.5, -43, -0.7)
            ankle = (side * 5.8, -58, 0.2)
            self.limb(trouser, hip, knee, 4.1, 3.6)
            self.limb(trouser, knee, ankle, 3.5, 2.6)
            self.box("armor" if not terrorist else "belt", side * 5.5, -43,
                     -3.1, 6.5, 4.8, 1.3)
            self.box("boots", side * 5.8, -60.0, -2.1, 7.1, 8.0, 12.0)
            shoulder = (side * 11.4, -12.4, 0)
            elbow = (side * 14.3, -22.2, -0.8)
            wrist = (side * 14.6, -31.1, -3.3)
            self.limb("uniform", shoulder, elbow, 4.0, 3.2)
            self.limb("skin" if terrorist else "uniform", elbow, wrist, 3.1, 2.3)
            self.box("gloves", wrist[0], -32.5, wrist[2], 4.8, 5.2, 5.0)
        self.limb("skin", (0, -10, 0), (0, -5, 0), 2.8, 2.5)
        self.ellipsoid("skin", (0, 0.5, -0.2), (5.1, 7.0, 4.8))

    def counter_terrorist(self) -> None:
        self.shared_body(False)
        self.tapered_box("armor", 0, -5.0, -26, -10.3, 17.3, 18.5, 3.2, 3.8)
        self.box("armor", 0, -19, 6.4, 13, 18, 4.0)  # Equipment pack.
        for side in (-1, 1):
            self.box("helmet", side*10.7, -12, -0.9, 5.8, 6.5, 9)
            self.box("armor", side*4.8, -23.2, -7.7, 4.8, 5.2, 2.0)
            self.box("visor", side*2.65, 0.7, -5.15, 4.2, 3.2, 1.1)
        self.box("belt", 0, -23.2, -7.7, 4.0, 5.2, 2.0)
        self.box("visor", 0, -4.0, -5.1, 9.0, 4.2, 1.1)
        self.ellipsoid("helmet", (0, 4.0, -0.1), (6.1, 7.0, 5.8), upper_only=True)
        self.box("helmet", 0, 3.6, -5.7, 12.3, 2.0, 1.7)  # Helmet rim.
        self.box("badge", 0, -14.0, -7.2, 2.8, 2.8, 0.5)
        self.limb("visor", (5.7, -8.0, 6.8), (6.0, 6.0, 6.8), 0.55, 0.35, 6)

    def terrorist(self) -> None:
        self.shared_body(True)
        self.tapered_box("armor", 0, -5.5, -25.5, -12, 16.5, 18.5, 3.3, 3.5)
        self.box("armor", 0, -20.0, 6.0, 11, 13, 3.1)  # Small pack.
        for side in (-1, 1):
            self.box("belt", side*6.2, -18.7, -7.0, 3.4, 14.0, 1.5)  # Vest straps.
            self.box("armor", side*5.0, -24.0, -7.4, 5.2, 4.7, 2.0)
            self.box("belt", side*8.4, -37.0, -3.9, 2.8, 7.0, 1.5)  # Cargo pockets.
        self.box("badge", 0, -20.0, -7.45, 5.8, 5.3, 1.8)
        self.ellipsoid("helmet", (0, 4.6, -0.1), (5.9, 6.4, 5.4), upper_only=True)
        self.box("helmet", 0, 3.3, -5.1, 11.0, 2.2, 1.5)  # Head-wrap band.
        self.box("visor", 0, -4.0, -5.15, 9.5, 5.3, 1.4)  # Face wrap.
        self.box("visor", 0, 0.8, -5.3, 6.5, 0.8, 0.8)   # Brow.

    def write(self) -> None:
        colors = list(self.used_colors.values())
        keys = list(self.used_colors)
        width = 1 << math.ceil(math.log2(len(colors) * 16))
        texture_coords = {key: ((index * 16 + 8) / width, 0.5)
                          for index, key in enumerate(keys)}
        lines = [
            "# Original low-poly nx-3D player model; regenerate with generate.py",
            f"mtllib {self.name}.mtl",
            f"o {self.name}",
        ]
        lines.extend(f"v {x:.4f} {y:.4f} {z:.4f}" for x, y, z in self.vertices)
        lines.extend(f"vt {u:.6f} {v:.6f}" for u, v in texture_coords.values())
        lines.append("usemtl atlas")
        uv_index = {key: index + 1 for index, key in enumerate(keys)}
        for a, b, c, color in self.faces:
            uv = uv_index[color]
            lines.append(f"f {a}/{uv} {b}/{uv} {c}/{uv}")
        (DESTINATION / f"{self.name}.obj").write_text("\n".join(lines) + "\n")
        (DESTINATION / f"{self.name}.mtl").write_text(
            f"newmtl atlas\nKd 1 1 1\nmap_Kd {self.name}.png\n"
        )
        write_png(DESTINATION / f"{self.name}.png", colors)


def main() -> None:
    for name, palette in PALETTES.items():
        figure = Figure(name, palette)
        if name == "counter_terrorist":
            figure.counter_terrorist()
        else:
            figure.terrorist()
        figure.write()
        print(f"{name}: {len(figure.faces)} triangles, {len(figure.used_colors)} colors")


if __name__ == "__main__":
    main()
