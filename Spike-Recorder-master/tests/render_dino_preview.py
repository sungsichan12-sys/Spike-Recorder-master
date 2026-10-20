"""Offline image of the actual scene polygons exported by dino_3d_tests.

Does not launch the recorder, create a window, or access sensor/audio devices.
"""
import json
from pathlib import Path
from scene_raster import render_scene
import sys

root = Path(__file__).resolve().parent
for name in ("dino-ground", "dino-jump", "dino-dragon", "dino-launch", "dino-ascent", "dino-sky", "dino-storm-ascent", "dino-storm", "dino-magic-seal", "dino-magic-beam", "dino-bestow", "dino-divine-ascent", "dino-demon", "dino-solar-seal", "dino-solar-laser", "dino-demon-detail", "dino-character-detail", "dino-dragon-detail", "dino-divine-detail"):
    if len(sys.argv)>1 and name not in sys.argv[1:]:
        continue
    data = json.loads((root / (name + ".json")).read_text())
    render_scene(data).save(root / (name + ".png"))
    print(name, flush=True)
