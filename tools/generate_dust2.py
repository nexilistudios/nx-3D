#!/usr/bin/env python3
"""Generate a self-contained CS2-style Dust II from editable cube geometry.

The plan is hand-authored from the CS2 radar. No imported map mesh or texture is
read. Coordinates in PLAN use a convenient diagram space, converted to engine
X/Z; height is engine Y. A 32-unit grid bounds the playable footprint.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass
import html
import json
import math
from pathlib import Path
import random
import struct
import zlib

try:
    from .brush_map import Brush, DEFAULT_MAP, MATERIALS, COLORS, write_map
except ImportError:
    from brush_map import Brush, DEFAULT_MAP, MATERIALS, COLORS, write_map

SCALE = 3.2
STEP = 10  # 32 engine units


def world(px: float, py: float) -> tuple[float, float]:
    return (px-795)*SCALE, (py-1160)*SCALE


def rect(x1, y1, x2, y2):
    return [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]


# Areas overlap at their connections. Later areas own the overlapping cells.
PLAN = [
    ("T_Spawn", [(75,1410),(130,1360),(130,1240),(220,1240),(220,1280),(350,1280),
                 (350,1310),(430,1310),(430,1250),(495,1250),(495,1380),(705,1380),
                 (705,1550),(215,1550),(215,1540),(75,1540)]),
    ("Outside_Tunnels", [(130,995),(175,995),(175,965),(240,965),(240,850),
                        (285,850),(285,965),(350,965),(350,995),(385,995),
                        (400,1180),(350,1180),(350,1260),(220,1260),(220,1395),
                        (125,1395),(125,1230),(135,1230)]),
    ("T_to_Long", [(640,1405),(640,1090),(710,1060),(970,1040),(970,1095),
                   (1070,1095),(1070,1330),(940,1330),(940,1400),(965,1400),
                   (965,1530),(875,1530),(875,1585),(705,1585),(705,1540)]),
    ("Suicide", rect(640,1090,695,1410)),
    ("Top_Mid", [(565,1030),(605,1030),(605,960),(640,950),(640,905),(630,905),
                 (630,825),(770,825),(770,1015),(795,1035),(975,1035),
                 (975,1105),(705,1105),(650,1135),(585,1135)]),
    ("Mid", [(640,650),(705,650),(705,690),(765,690),(765,1035),
             (640,1035),(640,910),(630,910),(630,825),(640,825)]),
    ("Catwalk", rect(735,745,775,1020)),
    ("A_Short", [(740,695),(765,695),(765,670),(835,670),(835,690),(895,690),
                 (895,570),(980,570),(980,755),(905,755),(905,765),(790,765),
                 (770,785),(740,785)]),
    ("A_Plat", [(895,280),(1145,280),(1145,180),(1335,180),(1335,280),
                (1350,280),(1350,440),(1230,440),(1230,400),(980,400),
                (980,620),(895,620)]),
    ("Long_Doors", [(980,945),(1055,945),(1065,980),(1065,1095),(1050,1115),
                    (980,1115)]),
    ("Long_A", [(975,810),(1030,795),(1215,795),(1215,605),(1205,605),
                (1205,520),(1220,520),(1220,440),(1400,440),(1405,590),
                (1365,590),(1340,640),(1340,850),(1400,860),(1400,1100),
                (1335,1100),(1335,1145),(1225,1145),(1225,940),(1120,940),
                (1120,1135),(1060,1135),(1060,955),(1025,940),(975,940)]),
    ("A_Ramp", rect(1230,280,1350,450)),
    ("CT_Mid", [(385,350),(705,350),(705,435),(800,435),(825,410),
                (825,355),(890,355),(890,520),(845,520),(815,540),
                (705,540),(705,655),(615,655),(615,515),(570,515),
                (570,525),(525,525),(525,515),(385,515)]),
    ("CT_Spawn", rect(825,350,980,510)),
    ("CT_Underpass", rect(875,440,1220,520)),
    ("B_Site", [(90,435),(90,395),(110,395),(110,155),(190,155),(190,245),
                (310,245),(365,280),(365,435),(350,435),(350,545),
                (285,620),(245,665),(240,605),(90,605)]),
    ("B_Doors", rect(345,440,430,510)),
    ("B_Window", rect(350,310,445,345)),
    ("B_Tunnel", rect(135,595,185,755)),
    ("Upper_Tunnels", [(90,735),(265,735),(265,775),(400,775),(435,755),
                       (475,765),(475,810),(440,850),(400,865),(305,865),
                       (305,880),(200,880),(200,860),(90,860)]),
    ("Tunnel_Stairs", [(395,750),(445,730),(480,750),(480,800),(440,845),(395,845)]),
    ("Lower_Tunnels", rect(410,690,645,760)),
]

LABELS = [("T SPAWN",565,1470), ("OUTSIDE TUNNELS",255,1100), ("UPPER TUNNELS",245,815),
          ("LOWER TUNNELS",525,722), ("B",300,325), ("B DOORS",375,475),
          ("CT MID",575,455), ("CT SPAWN",860,415), ("MID",680,860),
          ("TOP MID",785,1075), ("CATWALK",758,865), ("A SHORT",865,725),
          ("A",1180,375), ("GOOSE",1220,220), ("LONG A",1280,695),
          ("PIT",1280,1045), ("LONG DOORS",1020,1040)]


def inside(x, y, polygon):
    result = False
    for (ax, ay), (bx, by) in zip(polygon, polygon[1:]+polygon[:1]):
        if (ay > y) != (by > y) and x < (bx-ax)*(y-ay)/(by-ay)+ax:
            result = not result
    return result


def clamp(value, lo=0.0, hi=1.0):
    return max(lo, min(hi, value))


def elevation(x: float, y: float, area: str) -> float:
    if area in ("T_Spawn", "T_to_Long", "Suicide", "Top_Mid"):
        return 128
    if area == "Outside_Tunnels":
        return 128 * clamp((y-860)/400)
    if area == "Mid":
        return -112 + 240*clamp((y-700)/300)
    if area == "Catwalk":
        return 128*clamp((y-780)/240)
    if area == "A_Short":
        return 96*clamp((690-y)/70)
    if area == "A_Plat":
        return 96
    if area == "A_Ramp":
        return 96*clamp((450-y)/170)
    if area == "Long_Doors":
        return 128*clamp((y-945)/150)
    if area == "Long_A":
        if 1225 <= x <= 1335 and y >= 945:
            return -96*clamp((y-945)/65)
        if y < 450:
            return 96*clamp((450-y)/170)
        return 0
    if area in ("CT_Mid", "CT_Spawn", "Lower_Tunnels"):
        return -112
    if area == "CT_Underpass":
        return -112 + 112*clamp((x-1130)/90)
    if area == "B_Doors":
        return -112*clamp((x-365)/65)
    if area == "B_Window":
        return 56
    if area == "Tunnel_Stairs":
        return -112*clamp((800-y)/50)
    return 0


def ground_material(area):
    return "paving" if area in ("A_Plat", "A_Short", "Catwalk", "T_Spawn") else \
           "road" if area in ("Long_A", "CT_Underpass", "Long_Doors") else "sand"


@dataclass
class Cell:
    area: str
    height: float
    ax: float
    az: float
    intercept: float


class Builder:
    def __init__(self):
        self.brushes: list[Brush] = []
        self.cells: dict[tuple[int,int], Cell] = {}

    def box(self, x, z, width, depth, bottom, top, material="stone", group="detail", yaw=0,
            slope_x=0, slope_z=0):
        self.brushes.append(Brush(x, (bottom+top)/2, z, width/2, (top-bottom)/2, depth/2,
                                  material, slope_x, slope_z, yaw, group))

    def plan_box(self, x1, y1, x2, y2, bottom, top, material, group):
        x, z = world((x1+x2)/2, (y1+y2)/2)
        self.box(x,z,(x2-x1)*SCALE,(y2-y1)*SCALE,bottom,top,material,group)

    def terrain(self):
        for i in range(5,142):
            for j in range(14,160):
                px, py = i*STEP+STEP/2, j*STEP+STEP/2
                area = next((name for name, polygon in reversed(PLAN) if inside(px,py,polygon)), None)
                if inside(px,py,[(710,1120),(825,1120),(850,1140),(850,1320),
                                 (825,1345),(710,1345),(690,1320),(690,1140)]):
                    area = None
                if area is None:
                    continue
                h = elevation(px,py,area)
                ax = (elevation(px+5,py,area)-elevation(px-5,py,area))/32
                az = (elevation(px,py+5,area)-elevation(px,py-5,area))/32
                x,z = world(px,py)
                self.cells[i,j] = Cell(area,h,ax,az,h-ax*x-az*z)
        # Greedy rectangles retain region boundaries and every ramp plane.
        keys = {p:(c.area, round(c.ax,6), round(c.az,6), round(c.intercept,4))
                for p,c in self.cells.items()}
        remaining = set(keys)
        for i,j in sorted(keys):
            if (i,j) not in remaining: continue
            key = keys[i,j]
            w = 1
            while (i+w,j) in remaining and keys[i+w,j] == key: w += 1
            d = 1
            while all((i+k,j+d) in remaining and keys[i+k,j+d] == key for k in range(w)): d += 1
            remaining.difference_update((i+k,j+n) for k in range(w) for n in range(d))
            x,z = world((i+w/2)*STEP,(j+d/2)*STEP)
            cell = self.cells[i,j]
            h = cell.ax*x+cell.az*z+cell.intercept
            sx,sz = cell.ax*w*16,cell.az*d*16
            self.box(x,z,w*32,d*32,h-abs(sx)-abs(sz)-64,h,
                     ground_material(cell.area),cell.area+"/floor",slope_x=sx,slope_z=sz)

    def boundaries(self):
        for (i,j),cell in sorted(self.cells.items()):
            for di,dj in ((-1,0),(1,0),(0,-1),(0,1)):
                if (i+di,j+dj) in self.cells: continue
                px,py = (i+.5)*STEP,(j+.5)*STEP
                x,z = world(px+di*STEP/2,py+dj*STEP/2)
                x += di*32
                z += dj*32
                # Keep the covered CT route open below the elevated A Short slab.
                h = cell.height
                wall_height = 288 if "Tunnel" in cell.area else 352
                material = "stone" if cell.area.startswith("B") or "Tunnel" in cell.area else \
                           "whitewash" if cell.area in ("Long_A","A_Plat","Long_Doors") else "plaster"
                width,depth = (64,32) if di else (32,64)
                group = cell.area+"/wall"
                self.box(x,z,width,depth,h-64,h+wall_height,material,group)
                self.box(x,z,width+4,depth+4,h+wall_height-12,h+wall_height+4,"stone",cell.area+"/trim")
                self.box(x,z,width+2,depth+2,h,h+24,"stone",cell.area+"/trim")
                # Repeated wall piers and shuttered windows give the long lanes a facade.
                if (i if dj else j) % 8 == 0 and "Tunnel" not in cell.area:
                    self.box(x-di*34,z-dj*34,44 if di else 60,60 if di else 44,
                             h+128,h+218,"blue" if material=="whitewash" else "wood",cell.area+"/window")

    def crate(self, px,py,width=64,height=64,bottom=0,depth=None,yaw=0,group="cover"):
        x,z=world(px,py)
        depth=depth or width
        self.box(x,z,width,depth,bottom,bottom+height,"crate",group,yaw)
        for offset in (-height/2+8,height/2-8):
            self.box(x,z,width+2,depth+2,bottom+height/2+offset-3,bottom+height/2+offset+3,"metal",group,yaw)

    def arch(self, px,py,width=192,height=192,bottom=0,axis="z",group="gate",doors=False):
        x,z=world(px,py)
        def piece(offset,w,lo,hi,mat):
            self.box(x+(offset if axis=="z" else 0),z+(offset if axis=="x" else 0),
                     w if axis=="z" else 32,32 if axis=="z" else w,lo,hi,mat,group)
        for side in (-1,1):
            piece(side*(width/2+16),32,bottom,bottom+height+48,"stone")
        for part in range(12):
            offset=(part+.5)*width/12-width/2
            rise=math.sqrt(max(0,(width/2)**2-offset**2))*.48
            piece(offset,width/12+.1,bottom+height-width*.24+rise,bottom+height+48,"stone")
        if doors:
            for side in (-1,1):
                # Open leaves sit along the passage; the center remains traversable.
                dx,dz=side*(width/2-16),side*32
                self.box(x+(dx if axis=="z" else dz),z+(dz if axis=="z" else dx),
                         width*.43,10,bottom,bottom+height-20,"wood",group,
                         yaw=(side*72 if axis=="z" else 90+side*72))

    def car(self,px,py,bottom=0,yaw=0,group="car",color="whitewash"):
        x,z=world(px,py)
        self.box(x,z,160,76,bottom+12,bottom+44,color,group,yaw)
        self.box(x,z,86,64,bottom+44,bottom+68,"dark",group,yaw)
        self.box(x,z,70,64,bottom+68,bottom+74,color,group,yaw)
        angle=math.radians(yaw)
        for dx in (-50,50):
            for dz in (-36,36):
                wx,wz=x+math.cos(angle)*dx+math.sin(angle)*dz,z-math.sin(angle)*dx+math.cos(angle)*dz
                self.box(wx,wz,28,12,bottom,bottom+28,"dark",group,yaw)

    def details(self):
        # Elevated A Short crosses over CT spawn. Its underside is 80 above
        # ground level, leaving 192 units of clear height along CT underpass.
        self.plan_box(900,350,980,620,80,96,"paving","A_Short/bridge")
        for x in (900,980):
            self.plan_box(x-2,400,x+2,590,96,128,"stone","A_Short/rail")
        # Roofed tunnels, open skylight breaks, and internal supporting arches.
        for x1,x2 in ((90,185),(205,295),(320,395)):
            self.plan_box(x1,735,x2,860,208,232,"stone","Upper_Tunnels/ceiling")
        self.plan_box(135,610,185,735,208,232,"stone","B_Tunnel/ceiling")
        self.plan_box(470,690,625,760,112,144,"stone","Lower_Tunnels/ceiling")
        for px in (195,310,390):
            self.arch(px,810,224,192,axis="x",group="Upper_Tunnels/arch")
        self.arch(160,605,128,192,group="B_Tunnel/exit")
        self.arch(672,650,160,192,-112,group="Mid/doors",doors=True)
        self.arch(375,475,176,192,-16,axis="x",group="B_Doors/gate",doors=True)
        self.arch(373,328,96,128,56,axis="x",group="B_Window/frame")
        self.arch(1020,1090,176,192,124,group="Long_Doors/outer",doors=True)
        self.arch(1020,950,176,192,4,group="Long_Doors/inner",doors=True)
        # CS2-style recognizable cover: Xbox, A default, Goose, B double stack,
        # B window boost, Blue dumpster and the parked cars.
        for args in ((710,725,72,64,-92),(1155,370,80,104,96),
                     (1170,345,80,104,96),(1020,292,64,64,96),
                     (1040,292,64,64,96),(1260,285,48,72,96),
                     (285,350,96,112,0),(285,350,80,80,112),
                     (345,345,64,64,0),(215,410,96,96,0),
                     (132,390,80,64,0),(168,175,64,64,0),
                     (425,440,72,64,-112),(750,455,72,64,-112),
                     (815,450,64,64,-112),(440,705,64,64,-112),
                     (115,750,80,64,0),(220,805,48,64,0)):
            self.crate(*args,group="cover/crate")
        self.crate(424,335,64,64,-112,group="B_Window/boost")
        self.crate(397,330,64,64,-48,group="B_Window/boost")
        self.crate(328,350,64,32,0,group="B_Window/boost_step")
        # The outer window approach climbs from CT mid to its raised sill.
        x,z=world(423,385)
        self.box(x,z,80,256,-176,-28,"stone","B_Window/ramp",slope_z=-84)
        x,z=world(1075,825)
        self.box(x,z,112,80,0,88,"blue","Long_A/blue")
        self.box(x,z,120,88,88,96,"metal","Long_A/blue")
        self.car(1365,520,0,-18,"Long_A/car")
        self.car(285,575,0,-32,"B_Site/car", "blue")
        # Back site platform and an accessible shallow step.
        self.plan_box(110,155,190,245,0,32,"stone","B_Site/back_plat")
        self.plan_box(110,245,190,255,0,16,"stone","B_Site/back_step")
        # Buildings fill non-playable islands and create the Moroccan skyline.
        for x1,y1,x2,y2,top,mat in ((790,790,875,1010,384,"plaster"),
                                   (1000,550,1190,775,416,"whitewash"),
                                   (460,535,600,665,352,"plaster"),
                                   (425,900,590,1005,384,"whitewash"),
                                   (405,1155,595,1235,416,"plaster"),
                                   (710,1145,825,1335,384,"plaster")):
            self.plan_box(x1,y1,x2,y2,-176,top,mat,"skyline/building")
            self.plan_box(x1-3,y1-3,x2+3,y2+3,top,top+12,"roof","skyline/cornice")
            self.plan_box(x1+20,y1+20,x1+45,y1+45,top+12,top+72,"whitewash","skyline/tank")
        # Painted plant-zone outline and letters are geometry, not imported decals.
        for letter,px,py,h in (("a",1180,395,96),("b",300,325,0)):
            x,z=world(px,py)
            mat="site_"+letter
            for dx,dz,w,d in ((0,-112,224,5),(0,112,224,5),(-112,0,5,224),(112,0,5,224)):
                self.box(x+dx,z+dz,w,d,h+.15,h+.45,mat,"site/paint")
            if letter=="a":
                for side in (-1,1):
                    self.box(x+side*22,z,8,92,h+.2,h+.55,mat,"site/letter",yaw=side*24)
                self.box(x,z+5,55,8,h+.2,h+.55,mat,"site/letter")
            else:
                for dx,dz,w,d in ((-30,0,9,84),(0,-40,58,8),(0,0,58,8),(0,40,58,8),(30,-20,8,40),(30,20,8,40)):
                    self.box(x+dx,z+dz,w,d,h+.2,h+.55,mat,"site/letter")

    def build(self):
        # A sandy foundation hides the empty space between building islands.
        # It is below the fall-death plane, so escaping the playable floor is fatal.
        self.box(-240,-960,4800,5120,-448,-384,"sand","world/foundation")
        self.terrain()
        self.boundaries()
        self.details()
        return self.brushes


def png(path: Path, width: int, height: int, pixels: bytes):
    def chunk(name,data):
        return struct.pack(">I",len(data))+name+data+struct.pack(">I",zlib.crc32(name+data)&0xffffffff)
    rows=b"".join(b"\0"+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,2,0,0,0))+
                     chunk(b"IDAT",zlib.compress(rows,9))+chunk(b"IEND",b""))


def textures(directory: Path):
    directory.mkdir(parents=True,exist_ok=True)
    for index,(material,color) in enumerate(zip(MATERIALS,COLORS)):
        base=tuple(int(color[n:n+2],16) for n in (1,3,5))
        rng=random.Random(2800+index)
        pixels=bytearray()
        for y in range(128):
            for x in range(128):
                value=rng.uniform(-6,6)+3*math.sin(x*.13)*math.cos(y*.17)
                if material in ("stone","paving"):
                    if y%32<2 or (x+(16 if (y//32)%2 else 0))%64<2: value-=24
                if material in ("wood","crate"):
                    value+=4*math.sin(x*2.2+math.sin(y*.08))
                    if x%24<2 or y in (3,4,123,124): value-=28
                if material=="blue" and y%16<2: value-=14
                if material=="roof" and (y%16<2 or (x+(8 if y//16%2 else 0))%32<2): value-=22
                if material=="whitewash" and y>115: value-=4
                if material=="metal" and x%64<2: value-=12
                pixels.extend(max(0,min(255,round(c+value))) for c in base)
        png(directory/f"{material}.png",128,128,bytes(pixels))


def overview(path: Path, brushes: list[Brush]):
    colors=dict(zip(MATERIALS,COLORS))
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-2464 -3360 4544 4928">',
         '<rect x="-2464" y="-3360" width="4544" height="4928" fill="#19242c"/>']
    for b in brushes:
        if b.group.endswith("/ceiling") or b.group.startswith("skyline/"): continue
        points=" ".join(f"{x:.2f},{z:.2f}" for x,z in b.footprint)
        svg.append(f'<polygon points="{points}" fill="{colors[b.material]}" stroke="#655f52" stroke-width="2"/>')
    for label,px,py in LABELS:
        x,z=world(px,py)
        svg.append(f'<text x="{x}" y="{z}" fill="white" stroke="#1a252a" stroke-width="3" '
                   f'paint-order="stroke" text-anchor="middle" font-family="sans-serif" font-size="36">{html.escape(label)}</text>')
    svg.append('</svg>')
    path.write_text("\n".join(svg)+"\n")


def generate(output: Path = DEFAULT_MAP):
    builder=Builder()
    brushes=builder.build()
    write_map(output,brushes)
    textures(output.parent/"textures")
    overview(output.parent/"overview.svg",brushes)
    metadata={"name":"Generated Dust II (CS2 style)","generator":"tools/generate_dust2.py",
              "reference":"https://csnades.gg/dust2/callouts", "grid":32,
              "labels":[{"name":n,"x":world(x,y)[0],"z":world(x,y)[1]} for n,x,y in LABELS]}
    (output.parent/"layout.json").write_text(json.dumps(metadata,indent=2)+"\n")
    print(f"Generated {len(brushes)} editable cubes, {len(builder.cells)} floor cells and {len(MATERIALS)} procedural textures: {output}")
    return builder


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,default=DEFAULT_MAP)
    generate(parser.parse_args().output)
