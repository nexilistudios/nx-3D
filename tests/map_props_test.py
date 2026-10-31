"""OBJ metadata survives a round trip through the existing top-down editor."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.brush_map import Brush, BrushMap, read_map, read_model_records, write_map

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "example.brush"
    record = '#   nx3d-model-v1 "props/example_crate.obj" 0 32 64 32 32 32 15 1'
    write_map(path, [Brush(0, -16, 0, 128, 16, 128)], [record])
    model = BrushMap(path)
    model.change([*model.brushes, Brush(0, 32, 0, 16, 16, 16)])
    model.save()
    if read_model_records(path) != [record] or len(read_map(path)) != 2:
        raise RuntimeError("Editing brushes lost OBJ records")
    for bad in ('# nx3d-model-v1 "../external.obj" 0 0 0 1 1 1 0 1',
                '# nx3d-model-v1 "props/crate.obj" 0 0 0 0 1 1 0 1'):
        path.write_text("nx3d-brush-map 2\n" + bad + "\n")
        try:
            BrushMap(path)
        except ValueError:
            pass
        else:
            raise RuntimeError("Invalid model metadata was accepted")
print("Top-down editor preserves and validates OBJ model records")
