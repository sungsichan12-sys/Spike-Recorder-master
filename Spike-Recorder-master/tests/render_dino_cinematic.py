"""Render actual exported game polygons to an animation, without opening a window."""
import json
from pathlib import Path
from PIL import Image
from scene_raster import render_scene
root = Path(__file__).resolve().parent
frames = []
for path in sorted((root / 'dino-cinematic').glob('*.json')):
    data = json.loads(path.read_text(encoding='utf-8'))
    im = render_scene(data, scale=1)
    frames.append(im.resize((700, 476), Image.Resampling.LANCZOS))
if len(frames) != 72:
    raise RuntimeError(f'Expected 72 actual frames, found {len(frames)}')
# A single palette across the sequence prevents color flicker.
atlas = Image.new('RGB', (600, 408))
for i, im in enumerate(frames[::2]):
    atlas.paste(im.resize((100, 68)), ((i % 6) * 100, (i // 6) * 68))
palette = atlas.quantize(colors=256)
indexed = [im.quantize(palette=palette, dither=Image.Dither.NONE) for im in frames]
indexed[0].save(root / 'dino-cinematic.gif', save_all=True, append_images=indexed[1:],
                duration=[60, 70, 70] * 24, loop=0, disposal=2, optimize=False)
print('Rendered 72 frames / 4.8 seconds from the actual scene code')
