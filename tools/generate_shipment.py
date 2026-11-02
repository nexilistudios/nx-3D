#!/usr/bin/env python3
"""Build a bright, editable container-yard arena inspired by Shipment."""
from __future__ import annotations

import argparse
import html
import json
import math
from pathlib import Path
import random

try:
    from .brush_map import Brush, ROOT, write_map
    from .generate_dust2 import png
except ImportError:
    from brush_map import Brush, ROOT, write_map
    from generate_dust2 import png

DEFAULT_MAP = ROOT / "assets/maps/shipment/map.brush"
PALETTE = {
    "paving": (224, 231, 222),
    "whitewash": (247, 244, 211),
    "blue": (42, 139, 232),
    "green": (49, 190, 111),
    "red": (239, 77, 66),
    "metal": (102, 125, 139),
    "dark": (47, 67, 82),
    "wood": (219, 165, 91),
    "site_a": (255, 199, 63),
}


class Yard:
    def __init__(self) -> None:
        self.brushes: list[Brush] = []

    def box(self, x: float, z: float, width: float, depth: float,
            bottom: float, top: float, material: str, group: str,
            yaw: float = 0) -> None:
        self.brushes.append(Brush(x, (bottom + top) / 2, z, width / 2,
                                  (top - bottom) / 2, depth / 2, material,
                                  0, 0, yaw, group))

    def container(self, name: str, x: float, z: float, color: str,
                  yaw: int = 0, open_ends: bool = False, stacked: bool = False) -> None:
        # 128 wide by 256 long. The grooves are individual shallow ribs, so
        # they remain editable and cast shadows in the 3D map editor.
        bottom = 168 if stacked else 0
        angle = math.radians(yaw)
        def local(dx: float, dz: float) -> tuple[float, float]:
            return x + math.cos(angle) * dx + math.sin(angle) * dz, \
                   z - math.sin(angle) * dx + math.cos(angle) * dz
        group = f"containers/{name}"
        for side in (-1, 1):
            wx, wz = local(side * 60, 0)
            self.box(wx, wz, 8, 256, bottom, bottom + 152, color, group, yaw)
            for rib in range(-112, 113, 32):
                rx, rz = local(side * 65, rib)
                self.box(rx, rz, 3, 4, bottom + 12, bottom + 140,
                         "metal", f"{group}/rib", yaw)
        self.box(x, z, 128, 256, bottom + 148, bottom + 160, color, f"{group}/roof", yaw)
        self.box(x, z, 128, 256, bottom, bottom + 8, "metal", f"{group}/floor", yaw)
        if not open_ends:
            for end in (-1, 1):
                wx, wz = local(0, end * 124)
                self.box(wx, wz, 128, 8, bottom, bottom + 152,
                         color, f"{group}/door", yaw)
                for line in (-36, 36):
                    rx, rz = local(line, end * 129)
                    self.box(rx, rz, 3, 3, bottom + 10, bottom + 143,
                             "dark", f"{group}/door_detail", yaw)
        for end in (-1, 1):
            for side in (-1, 1):
                wx, wz = local(side * 58, end * 123)
                self.box(wx, wz, 8, 9, bottom, bottom + 160,
                         "dark", f"{group}/corner", yaw)

    def pallet(self, name: str, x: float, z: float) -> None:
        group = f"cover/{name}"
        self.box(x, z, 108, 108, 0, 13, "wood", group)
        for offset in (-39, 0, 39):
            self.box(x + offset, z, 18, 108, 13, 58, "wood", group)
        self.box(x, z, 108, 108, 58, 65, "wood", group)

    def build(self) -> list[Brush]:
        # Four exposed lanes wrap a tight cross of containers. Outer blockers
        # keep players inside the yard; the pale floor and yellow markings make
        # the saturated container colors easy to read.
        self.box(0, 0, 1536, 1536, -48, 0, "paving", "ground/concrete")
        for coordinate in (-752, 752):
            self.box(coordinate, 0, 32, 1536, 0, 304, "whitewash", "boundary/wall")
            self.box(0, coordinate, 1536, 32, 0, 304, "whitewash", "boundary/wall")
            self.box(coordinate, 0, 36, 1536, 304, 320, "metal", "boundary/cap")
            self.box(0, coordinate, 1536, 36, 304, 320, "metal", "boundary/cap")
        for coordinate in (-512, 512):
            self.box(coordinate, 0, 6, 1360, 0.4, 1.4, "site_a", "markings/lane")
            self.box(0, coordinate, 1360, 6, 0.4, 1.4, "site_a", "markings/lane")

        # Perimeter groups and the familiar narrow middle lanes.
        placements = (
            ("northwest", -555, -555, "blue", 0, False, False),
            ("north", -175, -560, "red", 90, False, False),
            ("northeast", 555, -555, "green", 0, False, False),
            ("west", -550, -185, "green", 0, True, False),
            ("east", 550, -185, "blue", 0, True, False),
            ("southwest", -555, 555, "red", 0, False, False),
            ("south", 175, 560, "blue", 90, False, False),
            ("southeast", 555, 555, "red", 0, False, False),
            ("middle_west", -175, 0, "blue", 0, True, False),
            ("middle_east", 175, 0, "green", 0, True, False),
            ("upper_mid", 0, -340, "red", 90, False, False),
            ("lower_mid", 0, 340, "green", 90, False, False),
            ("stack", 555, -555, "blue", 0, False, True),
        )
        for name, x, z, color, yaw, open_ends, stacked in placements:
            self.container(name, x, z, color, yaw, open_ends, stacked)
        for name, x, z in (("north_cover", 325, -590), ("south_cover", -325, 590),
                           ("west_cover", -600, 315), ("east_cover", 600, 315),
                           ("center", 0, 0)):
            self.pallet(name, x, z)
        return self.brushes


def textures(directory: Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for index, (material, base) in enumerate(PALETTE.items()):
        rng = random.Random(7341 + index)
        pixels = bytearray()
        for y in range(128):
            for x in range(128):
                grain = rng.randrange(-5, 6)
                groove = -18 if material in ("blue", "green", "red") and y % 16 < 2 else 0
                seam = -16 if material == "paving" and (x % 32 < 2 or y % 32 < 2) else 0
                stripe = -11 if material == "wood" and x % 20 < 2 else 0
                pixels.extend(max(0, min(255, value + grain + groove + seam + stripe))
                              for value in base)
        png(directory / f"{material}.png", 128, 128, bytes(pixels))


def overview(path: Path, brushes: list[Brush]) -> None:
    lines = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-800 -800 1600 1600">',
             '<rect x="-800" y="-800" width="1600" height="1600" fill="#e0e7de"/>']
    for brush in brushes:
        if brush.group.endswith("/rib") or brush.group.endswith("/floor"):
            continue
        points = " ".join(f"{x:.1f},{z:.1f}" for x, z in brush.footprint)
        color = PALETTE[brush.material]
        lines.append(f'<polygon points="{points}" fill="#{color[0]:02x}{color[1]:02x}{color[2]:02x}" '
                     'stroke="#34475a" stroke-width="2"/>')
    for label, x, z in (("NORTH LANE", 0, -690), ("CENTER", 0, 100),
                        ("WEST LANE", -675, 0), ("EAST LANE", 675, 0),
                        ("SOUTH LANE", 0, 690)):
        lines.append(f'<text x="{x}" y="{z}" text-anchor="middle" fill="#132b3b" '
                     f'font-size="32" font-family="sans-serif">{html.escape(label)}</text>')
    lines.append('</svg>')
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate(output: Path = DEFAULT_MAP) -> list[Brush]:
    brushes = Yard().build()
    write_map(output, brushes, description="Bright Shipment-inspired container yard")
    textures(output.parent / "textures")
    overview(output.parent / "overview.svg", brushes)
    metadata = {"name": "Shipment-inspired container yard", "generator": "tools/generate_shipment.py",
                "grid": 32, "labels": [{"name": name, "x": x, "z": z} for name, x, z in
                                      (("North Lane", 0, -690), ("Center", 0, 100),
                                       ("West Lane", -675, 0), ("East Lane", 675, 0),
                                       ("South Lane", 0, 690))]}
    (output.parent / "layout.json").write_text(json.dumps(metadata, indent=2) + "\n",
                                                     encoding="utf-8")
    print(f"Generated {len(brushes)} editable brushes: {output}")
    return brushes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_MAP)
    generate(parser.parse_args().output)
