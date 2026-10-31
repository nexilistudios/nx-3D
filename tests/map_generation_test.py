"""Generator output and route connectivity, without the original OBJ or a GUI."""
from collections import deque
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.brush_map import DEFAULT_MAP, Brush, BrushMap, read_map
from tools.generate_dust2 import generate, LABELS


with tempfile.TemporaryDirectory() as temporary:
    path = Path(temporary) / "map.brush"
    builder = generate(path)
    repeat = Path(temporary) / "repeat.brush"
    generate(repeat)
    if path.read_bytes() != repeat.read_bytes():
        raise RuntimeError("Map generation is not deterministic")
    brushes = read_map(path)
    if len(brushes) < 1000 or not any(b.slope_x or b.slope_z for b in brushes):
        raise RuntimeError("Incomplete generated map")
    edited_map = BrushMap(DEFAULT_MAP)
    if len(edited_map.brushes) < 1000 or not any(b.slope_x or b.slope_z for b in edited_map.brushes):
        raise RuntimeError("Incomplete editable Dust II map")
    model = BrushMap(path)
    original = model.brushes.copy()
    model.change([*original, Brush(0,32,0,16,32,16,"blue",0,0,45,"test")])
    model.save()
    if read_map(path) != model.brushes:
        raise RuntimeError("Edited geometry lost during save")
    model.undo()
    if model.brushes != original:
        raise RuntimeError("Undo did not restore generated geometry")
    model.redo()
    if len(model.brushes) != len(original)+1:
        raise RuntimeError("Redo did not restore added geometry")

    # Limit floor changes to a normal 24-unit step between adjacent cells.
    start = (56,146)
    seen = {start}
    queue = deque([start])
    while queue:
        point = queue.popleft()
        height = builder.cells[point].height
        for dx,dz in ((0,1),(0,-1),(1,0),(-1,0)):
            neighbor = point[0]+dx,point[1]+dz
            if neighbor in seen or neighbor not in builder.cells:
                continue
            if abs(builder.cells[neighbor].height-height) <= 24:
                seen.add(neighbor)
                queue.append(neighbor)
    for name,x,z in LABELS:
        if (int(x//10),int(z//10)) not in seen:
            raise RuntimeError(f"Disconnected map route: {name}")
    print(f"Generated map: {len(brushes)} brushes, all main routes connected")
