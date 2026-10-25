"""Editable deformed cubes shared by the map generator and editor."""
from __future__ import annotations

from dataclasses import dataclass
import math
import os
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MAP = ROOT / "assets/maps/dust2_cs2/map.brush"
HEADER = "nx3d-brush-map 2"
MAX_BRUSHES = 32768
MATERIALS = ("sand", "paving", "plaster", "whitewash", "stone", "blue", "wood",
             "crate", "metal", "roof", "dark", "site_a", "site_b", "road")
COLORS = ("#c8b897", "#c7bda8", "#d0b98e", "#e3dfcd", "#a7977c", "#608e9e",
          "#806443", "#a78b58", "#6e7777", "#ac7964", "#434b4b", "#c97546", "#b76440", "#979187")


@dataclass(frozen=True)
class Brush:
    x: float
    y: float
    z: float
    hx: float
    hy: float
    hz: float
    material: str = "stone"
    slope_x: float = 0
    slope_z: float = 0
    yaw: float = 0
    group: str = "custom"

    def __post_init__(self) -> None:
        values = (self.x, self.y, self.z, self.hx, self.hy, self.hz,
                  self.slope_x, self.slope_z, self.yaw)
        if not all(math.isfinite(v) and abs(v) <= 100000 for v in values):
            raise ValueError("Brush values must be finite and within 100000 units")
        if min(self.hx, self.hy, self.hz) <= 0:
            raise ValueError("Brush dimensions must be positive")
        if abs(self.slope_x) + abs(self.slope_z) >= 2 * self.hy:
            raise ValueError("The ramp top must stay above the cube bottom")
        if self.material not in MATERIALS:
            raise ValueError(f"Unknown material: {self.material}")
        if not self.group or any(c.isspace() for c in self.group):
            raise ValueError("Group names cannot contain whitespace")

    @property
    def footprint(self) -> list[tuple[float, float]]:
        angle = math.radians(self.yaw)
        c, s = math.cos(angle), math.sin(angle)
        return [(self.x + c*x + s*z, self.z - s*x + c*z) for x, z in
                ((-self.hx, -self.hz), (self.hx, -self.hz),
                 (self.hx, self.hz), (-self.hx, self.hz))]

    @property
    def bounds(self) -> tuple[float, float, float, float]:
        points = self.footprint
        return min(p[0] for p in points), min(p[1] for p in points), \
               max(p[0] for p in points), max(p[1] for p in points)

    def contains(self, x: float, z: float) -> bool:
        angle = math.radians(self.yaw)
        c, s = math.cos(angle), math.sin(angle)
        dx, dz = x-self.x, z-self.z
        return abs(c*dx-s*dz) <= self.hx and abs(s*dx+c*dz) <= self.hz


def read_map(path: Path) -> list[Brush]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] not in (HEADER, "nx3d-brush-map 1"):
        raise ValueError(f"{path}: invalid brush map header")
    version2 = lines[0] == HEADER
    result = []
    for number, line in enumerate(lines[1:], 2):
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        try:
            if parts[0] != "brush" or len(parts) != (12 if version2 else 7):
                raise ValueError("Invalid brush record")
            values = [float(value) for value in parts[1:7]]
            result.append(Brush(*values, parts[7], *map(float, parts[8:11]), parts[11])
                          if version2 else Brush(*values))
        except ValueError as error:
            raise ValueError(f"{path}:{number}: {error}") from error
    if len(result) > MAX_BRUSHES:
        raise ValueError(f"{path}: map exceeds {MAX_BRUSHES} brushes")
    return result


def write_map(path: Path, brushes: list[Brush]) -> None:
    if len(brushes) > MAX_BRUSHES:
        raise ValueError(f"Map exceeds {MAX_BRUSHES} brushes")
    lines = [HEADER, "# Generated CS2-style Dust II. All world geometry is editable.",
             "# brush x y z half_x half_y half_z material slope_x slope_z yaw group"]
    for b in brushes:
        geometry = " ".join(f"{value:.9g}" for value in (b.x, b.y, b.z, b.hx, b.hy, b.hz))
        shape = " ".join(f"{value:.9g}" for value in (b.slope_x, b.slope_z, b.yaw))
        lines.append(f"brush {geometry} {b.material} {shape} {b.group}")
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix=".brushes-", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            output.write("\n".join(lines) + "\n")
        os.chmod(name, path.stat().st_mode & 0o777 if path.exists() else 0o644)
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


class BrushMap:
    def __init__(self, path: Path):
        self.path = path
        self.brushes = read_map(path)
        self.undo_stack: list[list[Brush]] = []
        self.redo_stack: list[list[Brush]] = []

    def change(self, brushes: list[Brush]) -> None:
        if brushes == self.brushes:
            return
        if len(brushes) > MAX_BRUSHES:
            raise ValueError(f"Map exceeds {MAX_BRUSHES} brushes")
        self.undo_stack.append(self.brushes.copy())
        self.redo_stack.clear()
        self.brushes = brushes

    def undo(self) -> None:
        if self.undo_stack:
            self.redo_stack.append(self.brushes.copy())
            self.brushes = self.undo_stack.pop()

    def redo(self) -> None:
        if self.redo_stack:
            self.undo_stack.append(self.brushes.copy())
            self.brushes = self.redo_stack.pop()

    def save(self) -> None:
        write_map(self.path, self.brushes)
