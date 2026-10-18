"""Generate small OBJ props and solid-color textures for utility NXObjects."""

from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parent


def png(path: Path, color: tuple[int, int, int]) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(
            ">I", zlib.crc32(kind + data) & 0xFFFFFFFF
        )

    raw = b"\x00" + bytes(color) * 2
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 1, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


def box(out: list[str], material: str, center: tuple[float, float, float],
        size: tuple[float, float, float], start: int) -> int:
    x, y, z = center
    w, h, d = (s / 2 for s in size)
    for vx, vy, vz in (
        (x-w,y-h,z-d), (x+w,y-h,z-d), (x+w,y+h,z-d), (x-w,y+h,z-d),
        (x-w,y-h,z+d), (x+w,y-h,z+d), (x+w,y+h,z+d), (x-w,y+h,z+d),
    ):
        out.append(f"v {vx:.3f} {vy:.3f} {vz:.3f}")
    out.append(f"usemtl {material}")
    for face in ((1,2,3,4), (5,8,7,6), (1,5,6,2),
                 (2,6,7,3), (3,7,8,4), (4,8,5,1)):
        out.append("f " + " ".join(str(start + i - 1) for i in face))
    return start + 8


def model(name: str, colors: dict[str, tuple[int, int, int]],
          parts: list[tuple[str, tuple[float, float, float], tuple[float, float, float]]]) -> None:
    mtl = []
    for material, color in colors.items():
        png(ROOT / f"{name}_{material}.png", color)
        mtl.extend((f"newmtl {material}", f"map_Kd {name}_{material}.png"))
    (ROOT / f"{name}.mtl").write_text("\n".join(mtl) + "\n")
    obj = [f"mtllib {name}.mtl"]
    index = 1
    for material, center, size in parts:
        index = box(obj, material, center, size, index)
    (ROOT / f"{name}.obj").write_text("\n".join(obj) + "\n")


model("defuse_kit", {"case": (35, 66, 58), "handle": (24, 32, 35),
                     "latch": (178, 190, 105)}, [
    ("case", (0, 0, 0), (.8, .55, .3)),
    ("handle", (0, .34, 0), (.35, .15, .12)),
    ("latch", (0, .04, .16), (.15, .12, .03)),
])
model("ammo_refill", {"case": (86, 72, 43), "band": (174, 146, 76),
                     "round": (193, 153, 56)}, [
    ("case", (0, -.05, 0), (.75, .4, .55)),
    ("band", (0, .17, 0), (.79, .08, .59)),
    ("round", (-.19, .30, 0), (.12, .18, .12)),
    ("round", (0, .30, 0), (.12, .18, .12)),
    ("round", (.19, .30, 0), (.12, .18, .12)),
])
model("health_refill", {"case": (226, 227, 215), "cross": (184, 39, 42),
                       "handle": (65, 70, 68)}, [
    ("case", (0, 0, 0), (.7, .6, .27)),
    ("cross", (0, .05, .15), (.13, .37, .03)),
    ("cross", (0, .05, .17), (.37, .13, .03)),
    ("handle", (0, .37, 0), (.30, .14, .10)),
])
