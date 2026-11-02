"""Generate the built-in low-poly sphere for the 3D map editor."""
from pathlib import Path
import math
import struct
import zlib


def png_pixel(red: int, green: int, blue: int) -> bytes:
    def chunk(name: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + name + data +
                struct.pack(">I", zlib.crc32(name + data) & 0xFFFFFFFF))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes((0, red, green, blue)))) +
            chunk(b"IEND", b""))


def generate(directory: Path) -> None:
    segments = 16
    rings = 8
    vertices = [(0.0, 1.0, 0.0)]
    for latitude in range(1, rings):
        angle = math.pi * latitude / rings
        for longitude in range(segments):
            azimuth = 2 * math.pi * longitude / segments
            vertices.append((math.sin(angle) * math.cos(azimuth),
                             math.cos(angle),
                             math.sin(angle) * math.sin(azimuth)))
    vertices.append((0.0, -1.0, 0.0))
    lines = ["# Built-in low-poly map editor sphere", "mtllib editor_sphere.mtl"]
    for x, y, z in vertices:
        lines.append(f"v {x:.8f} {y:.8f} {z:.8f}")
    for x, y, z in vertices:
        lines.append(f"vt {(math.atan2(z, x) / (2 * math.pi) + 1) % 1:.8f} "
                     f"{(y + 1) * .5:.8f}")
    lines.append("usemtl sphere")
    def face(a: int, b: int, c: int) -> None:
        lines.append(f"f {a}/{a} {b}/{b} {c}/{c}")
    south = len(vertices)
    for longitude in range(segments):
        next_longitude = (longitude + 1) % segments
        face(1, 2 + next_longitude, 2 + longitude)
        for latitude in range(rings - 2):
            a = 2 + latitude * segments + longitude
            b = 2 + latitude * segments + next_longitude
            c = a + segments
            d = b + segments
            face(a, b, c)
            face(b, d, c)
        face(south, 2 + (rings - 2) * segments + longitude,
             2 + (rings - 2) * segments + next_longitude)
    (directory / "editor_sphere.obj").write_text("\n".join(lines) + "\n")
    (directory / "editor_sphere.mtl").write_text(
        "newmtl sphere\nKd 0.36 0.72 0.94\nmap_Kd editor_sphere.png\n")
    (directory / "editor_sphere.png").write_bytes(png_pixel(92, 184, 240))


if __name__ == "__main__":
    generate(Path(__file__).resolve().parent)
