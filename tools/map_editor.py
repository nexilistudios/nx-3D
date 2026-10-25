#!/usr/bin/env python3
"""Top-down cube-brush editor for the Dust2 map (standard-library Tkinter)."""

from __future__ import annotations

import argparse
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import tkinter as tk
from tkinter import messagebox, ttk

try:
    from .brush_map import Brush, BrushMap, DEFAULT_MAP, ROOT, MATERIALS, COLORS
except ImportError:
    from brush_map import Brush, BrushMap, DEFAULT_MAP, ROOT, MATERIALS, COLORS


class Editor:
    def __init__(self, path: Path):
        self.model = BrushMap(path)
        self.saved_brushes = self.model.brushes.copy()
        metadata = path.parent / "layout.json"
        self.labels = json.loads(metadata.read_text()).get("labels", []) if metadata.exists() else []
        self.root = tk.Tk()
        self.root.title(f"nx-3D Cube Map Editor — {path.name}")
        self.root.geometry("1440x920")
        self.scale = 0.145
        self.origin_x = 450.0
        self.origin_y = 485.0
        self.grid = tk.IntVar(value=32)
        self.level = tk.StringVar(value="0")
        self.height = tk.StringVar(value="64")
        self.layers = tk.StringVar(value="All")
        self.mode = "select"
        self.selected: int | None = None
        self.drag_start: tuple[float, float] | None = None
        self.drag_original: Brush | None = None
        self.pan_start: tuple[float, float] | None = None
        self.preview: tuple[float, float, float, float] | None = None
        self.status = tk.StringVar(value=f"Generated Dust II: {len(self.model.brushes)} editable cubes and ramps.")
        self.fields: dict[str, tk.StringVar] = {}
        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.redraw()

    def _build_ui(self) -> None:
        bar = ttk.Frame(self.root, padding=5)
        bar.pack(fill="x")
        for label, command in (("Select", lambda: self.set_mode("select")),
                               ("Add brush", lambda: self.set_mode("add")),
                               ("Duplicate", self.duplicate), ("Delete", self.delete),
                               ("Undo", self.undo), ("Redo", self.redo),
                               ("Save", self.save), ("Fit map", self.fit), ("3D preview", self.preview_map)):
            ttk.Button(bar, text=label, command=command).pack(side="left", padx=2)
        layer = ttk.Combobox(bar, textvariable=self.layers, values=("All", "Floors", "Walls", "Cover", "Details"),
                             width=9, state="readonly")
        layer.pack(side="left", padx=5)
        layer.bind("<<ComboboxSelected>>", lambda _: self.redraw())
        ttk.Label(bar, text="Grid").pack(side="left", padx=(20, 3))
        ttk.Combobox(bar, textvariable=self.grid, values=(8, 16, 32, 64, 128),
                     width=5, state="readonly").pack(side="left")
        ttk.Label(bar, text="Bottom Y").pack(side="left", padx=(20, 3))
        ttk.Entry(bar, textvariable=self.level, width=7).pack(side="left")
        ttk.Label(bar, text="Height").pack(side="left", padx=(10, 3))
        ttk.Entry(bar, textvariable=self.height, width=7).pack(side="left")

        body = ttk.Frame(self.root)
        body.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(body, background="#18212a", highlightthickness=0)
        self.canvas.pack(side="left", fill="both", expand=True)
        inspector = ttk.Frame(body, padding=12, width=210)
        inspector.pack(side="right", fill="y")
        ttk.Label(inspector, text="Selected brush", font=("TkDefaultFont", 12, "bold")).pack(anchor="w")
        for key, label in (("x", "Center X"), ("y", "Center Y"), ("z", "Center Z"),
                           ("hx", "Half width X"), ("hy", "Half height Y"),
                           ("hz", "Half depth Z"), ("slope_x", "Ramp tilt X"),
                           ("slope_z", "Ramp tilt Z"), ("yaw", "Rotation Y (degrees)")):
            ttk.Label(inspector, text=label).pack(anchor="w", pady=(3, 0))
            variable = tk.StringVar()
            self.fields[key] = variable
            ttk.Entry(inspector, textvariable=variable, width=16).pack(anchor="w")
        for key, choices in (("material", MATERIALS), ("group", ())):
            ttk.Label(inspector, text=key.capitalize()).pack(anchor="w", pady=(3, 0))
            self.fields[key] = tk.StringVar()
            ttk.Combobox(inspector, textvariable=self.fields[key], values=choices, width=22).pack(anchor="w")
        ttk.Button(inspector, text="Apply dimensions", command=self.apply_fields).pack(anchor="w", pady=14)
        ttk.Label(inspector, text="Left drag: create or move\nMiddle drag: pan\nWheel: zoom\nDelete: remove\nCtrl+Z/Y: undo/redo\nCtrl+S: save", justify="left").pack(anchor="w")
        ttk.Label(self.root, textvariable=self.status, padding=5).pack(fill="x")

        self.canvas.bind("<Configure>", lambda _: self.redraw())
        self.canvas.bind("<ButtonPress-1>", self.mouse_down)
        self.canvas.bind("<B1-Motion>", self.mouse_drag)
        self.canvas.bind("<ButtonRelease-1>", self.mouse_up)
        self.canvas.bind("<ButtonPress-2>", self.pan_down)
        self.canvas.bind("<B2-Motion>", self.pan_drag)
        self.canvas.bind("<MouseWheel>", self.zoom)
        self.canvas.bind("<Button-4>", lambda e: self.zoom_at(e.x, e.y, 1.15))
        self.canvas.bind("<Button-5>", lambda e: self.zoom_at(e.x, e.y, 1/1.15))
        self.root.bind("<Delete>", lambda _: self.delete())
        self.root.bind("<Control-z>", lambda _: self.undo())
        self.root.bind("<Control-y>", lambda _: self.redo())
        self.root.bind("<Control-s>", lambda _: self.save())

    def set_mode(self, mode: str) -> None:
        self.mode = mode
        self.status.set("Drag on the map to create a brush." if mode == "add" else
                        "Click a brush to select it; drag to move it.")

    def world(self, sx: float, sy: float) -> tuple[float, float]:
        return (sx - self.origin_x) / self.scale, (sy - self.origin_y) / self.scale

    def screen(self, x: float, z: float) -> tuple[float, float]:
        return self.origin_x + x * self.scale, self.origin_y + z * self.scale

    def snap(self, value: float) -> float:
        step = self.grid.get()
        return round(value / step) * step

    def redraw(self) -> None:
        canvas = self.canvas
        canvas.delete("all")
        width, height = canvas.winfo_width(), canvas.winfo_height()
        step = self.grid.get() * self.scale
        if step >= 8:
            x = self.origin_x % step
            while x < width:
                canvas.create_line(x, 0, x, height, fill="#28333e")
                x += step
            y = self.origin_y % step
            while y < height:
                canvas.create_line(0, y, width, y, fill="#28333e")
                y += step
        for index, brush in enumerate(self.model.brushes):
            if not self.visible(brush): continue
            x1,z1,x2,z2 = brush.bounds
            a,b = self.screen(x1,z1),self.screen(x2,z2)
            if b[0]<0 or a[0]>width or b[1]<0 or a[1]>height: continue
            selected = index == self.selected
            points = [value for p in brush.footprint for value in self.screen(*p)]
            canvas.create_polygon(*points, outline="#fff28a" if selected else "#655f52",
                                  width=3 if selected else 1,
                                  fill=dict(zip(MATERIALS,COLORS))[brush.material])
        for label in self.labels:
            x,y = self.screen(label["x"],label["z"])
            canvas.create_text(x+1,y+1,text=label["name"],fill="black",font=("TkDefaultFont",9,"bold"))
            canvas.create_text(x,y,text=label["name"],fill="white",font=("TkDefaultFont",9,"bold"))
        if self.preview:
            x1, z1, x2, z2 = self.preview
            canvas.create_rectangle(*self.screen(x1, z1), *self.screen(x2, z2),
                                    outline="#91e0a0", width=2, dash=(5, 3))

    def visible(self, brush: Brush) -> bool:
        layer = self.layers.get()
        if layer == "Floors": return brush.group.endswith(("/floor","/bridge"))
        if layer == "Walls": return brush.group.endswith(("/wall","/trim"))
        if layer == "Cover": return any(tag in brush.group for tag in ("cover/","car","boost","blue"))
        if layer == "Details": return not brush.group.endswith(("/floor","/wall","/trim"))
        return True

    def fit(self) -> None:
        bounds = [b.bounds for b in self.model.brushes]
        if not bounds: return
        minx,minz = min(b[0] for b in bounds),min(b[1] for b in bounds)
        maxx,maxz = max(b[2] for b in bounds),max(b[3] for b in bounds)
        w,h = self.canvas.winfo_width(),self.canvas.winfo_height()
        self.scale = min((w-40)/max(1,maxx-minx),(h-40)/max(1,maxz-minz))
        self.origin_x,self.origin_y = w/2-(minx+maxx)*self.scale/2,h/2-(minz+maxz)*self.scale/2
        self.redraw()

    def preview_map(self) -> None:
        executable = ROOT/"build/client/nx_3D_client"
        if not executable.exists():
            messagebox.showerror("Preview unavailable","Build the client first: cmake --build build")
            return
        if self.model.path.resolve()!=DEFAULT_MAP.resolve():
            messagebox.showinfo("Preview", "3D preview loads assets/maps/dust2_cs2/map.brush. Save your map there first.")
            return
        self.save()
        if self.model.brushes != self.saved_brushes: return
        subprocess.Popen([str(executable),"--map-preview"],cwd=ROOT)

    def select(self, index: int | None) -> None:
        self.selected = index
        for value in self.fields.values():
            value.set("")
        if index is not None:
            brush = self.model.brushes[index]
            for key in self.fields:
                value = getattr(brush, key)
                self.fields[key].set(value if isinstance(value,str) else f"{value:g}")
        self.redraw()

    def mouse_down(self, event: tk.Event) -> None:
        x, z = self.world(event.x, event.y)
        self.drag_start = (x, z)
        self.drag_original = None
        if self.mode == "select":
            found = next((i for i in range(len(self.model.brushes)-1,-1,-1)
                          if self.visible(self.model.brushes[i]) and self.model.brushes[i].contains(x,z)),None)
            self.select(found)
            if found is not None:
                self.drag_original = self.model.brushes[found]

    def mouse_drag(self, event: tk.Event) -> None:
        if self.drag_start is None:
            return
        x, z = self.world(event.x, event.y)
        if self.mode == "add":
            x1, x2 = sorted((self.snap(self.drag_start[0]), self.snap(x)))
            z1, z2 = sorted((self.snap(self.drag_start[1]), self.snap(z)))
            self.preview = (x1, z1, x2, z2)
        elif self.selected is not None and self.drag_original is not None:
            dx, dz = x - self.drag_start[0], z - self.drag_start[1]
            brush = self.drag_original
            self.preview = (self.snap(brush.x + dx) - brush.hx,
                            self.snap(brush.z + dz) - brush.hz,
                            self.snap(brush.x + dx) + brush.hx,
                            self.snap(brush.z + dz) + brush.hz)
        self.redraw()

    def mouse_up(self, event: tk.Event) -> None:
        if self.drag_start is None:
            return
        self.mouse_drag(event)
        try:
            if self.preview and self.mode == "add":
                x1, z1, x2, z2 = self.preview
                if x2 > x1 and z2 > z1:
                    bottom, height = float(self.level.get()), float(self.height.get())
                    brush = Brush((x1 + x2) / 2, bottom + height / 2, (z1 + z2) / 2,
                                  (x2 - x1) / 2, height / 2, (z2 - z1) / 2)
                    self.model.change([*self.model.brushes, brush])
                    self.select(len(self.model.brushes) - 1)
            elif self.preview and self.selected is not None and self.drag_original is not None:
                x1, z1, x2, z2 = self.preview
                old = self.drag_original
                brush = replace(old, x=(x1+x2)/2, z=(z1+z2)/2)
                updated = self.model.brushes.copy()
                updated[self.selected] = brush
                self.model.change(updated)
                self.select(self.selected)
        except ValueError as error:
            messagebox.showerror("Invalid brush", str(error))
        self.drag_start = None
        self.drag_original = None
        self.preview = None
        self.redraw()

    def pan_down(self, event: tk.Event) -> None:
        self.pan_start = (event.x, event.y)

    def pan_drag(self, event: tk.Event) -> None:
        if self.pan_start:
            self.origin_x += event.x - self.pan_start[0]
            self.origin_y += event.y - self.pan_start[1]
            self.pan_start = (event.x, event.y)
            self.redraw()

    def zoom(self, event: tk.Event) -> None:
        self.zoom_at(event.x,event.y,1.15 if event.delta>0 else 1/1.15)

    def zoom_at(self, x, y, factor):
        wx,wz = self.world(x,y)
        self.scale = max(.025,min(2.0,self.scale*factor))
        self.origin_x,self.origin_y = x-wx*self.scale,y-wz*self.scale
        self.redraw()

    def apply_fields(self) -> None:
        if self.selected is None:
            return
        try:
            numeric = {key:float(self.fields[key].get()) for key in
                       ("x","y","z","hx","hy","hz","slope_x","slope_z","yaw")}
            brush = Brush(**numeric,material=self.fields["material"].get(),group=self.fields["group"].get())
        except ValueError as error:
            messagebox.showerror("Invalid brush", str(error))
            return
        updated = self.model.brushes.copy()
        updated[self.selected] = brush
        self.model.change(updated)
        self.redraw()

    def duplicate(self) -> None:
        if self.selected is not None:
            b = self.model.brushes[self.selected]
            try:
                self.model.change([*self.model.brushes, replace(b,x=b.x+self.grid.get())])
            except ValueError as error:
                messagebox.showerror("Map limit", str(error))
                return
            self.select(len(self.model.brushes) - 1)

    def delete(self) -> None:
        if self.selected is not None:
            updated = self.model.brushes.copy()
            updated.pop(self.selected)
            self.model.change(updated)
            self.select(None)

    def undo(self) -> None:
        self.model.undo()
        self.select(None)

    def redo(self) -> None:
        self.model.redo()
        self.select(None)

    def save(self) -> None:
        try:
            self.model.save()
            self.saved_brushes = self.model.brushes.copy()
            self.status.set(f"Saved {len(self.model.brushes)} brushes to {self.model.path}")
        except OSError as error:
            messagebox.showerror("Save failed", str(error))

    def close(self) -> None:
        if self.model.brushes != self.saved_brushes:
            choice = messagebox.askyesnocancel("Unsaved map", "Save brush changes before closing?")
            if choice is None:
                return
            if choice:
                self.save()
                if self.model.brushes != self.saved_brushes:
                    return
        self.root.destroy()

    def run(self) -> None:
        self.root.mainloop()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", nargs="?", type=Path, default=DEFAULT_MAP)
    parser.add_argument("--check", action="store_true", help="validate without opening a window")
    args = parser.parse_args()
    if args.check:
        model = BrushMap(args.map)
        print(f"{args.map}: {len(model.brushes)} valid brushes")
    else:
        Editor(args.map).run()


if __name__ == "__main__":
    main()
