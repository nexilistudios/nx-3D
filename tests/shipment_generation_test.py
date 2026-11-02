"""Shipment's generated arena and shipped copy stay valid and reproducible."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.brush_map import read_map
from tools.generate_shipment import DEFAULT_MAP, generate


with tempfile.TemporaryDirectory() as directory:
    first = Path(directory) / "first" / "map.brush"
    second = Path(directory) / "second" / "map.brush"
    generate(first)
    generate(second)
    if first.read_bytes() != second.read_bytes():
        raise RuntimeError("Shipment geometry is not deterministic")
    brushes = read_map(first)
    if brushes != read_map(DEFAULT_MAP):
        raise RuntimeError("Shipped map differs from the generator")
    for material in ("blue", "green", "red"):
        if not any(b.material == material and b.group.startswith("containers/") for b in brushes):
            raise RuntimeError(f"Missing {material} shipping container")
        if not (first.parent / "textures" / f"{material}.png").is_file():
            raise RuntimeError(f"Missing {material} container texture")
    for x, z in ((-390, -680), (-220, -680), (0, -680), (200, -680), (370, -680),
                 (-390, 680), (-220, 680), (0, 680), (200, 680), (370, 680)):
        if any(b.contains(x, z) and b.y + b.hy > 1 and b.y - b.hy < 128
               for b in brushes):
            raise RuntimeError(f"Spawn at ({x}, {z}) is blocked")
print(f"Shipment: {len(brushes)} brushes, three container colors, clear spawns")
