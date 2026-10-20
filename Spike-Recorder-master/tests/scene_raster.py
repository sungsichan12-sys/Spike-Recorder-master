"""Headless rasterizer for the same affine vertex colors sent to OpenGL."""
import numpy as np
from PIL import Image


class Raster:
    def __init__(self, width, height, scale=2):
        self.scale = scale
        self.width, self.height = width, height
        self.pixels = np.zeros((height * scale, width * scale, 3), dtype=np.uint8)
        self.depth = np.zeros((height * scale, width * scale), dtype=np.float32)

    def triangle(self, points, colors, depth=None):
        p = np.asarray(points, dtype=np.float32) * self.scale
        c = np.asarray(colors, dtype=np.float32)[:, :3]
        lo = np.maximum(np.floor(p.min(axis=0)).astype(int), 0)
        hi = np.minimum(np.ceil(p.max(axis=0)).astype(int), [self.pixels.shape[1], self.pixels.shape[0]])
        if np.any(hi <= lo):
            return
        a, b, d = p
        area = (b[1]-d[1])*(a[0]-d[0])+(d[0]-b[0])*(a[1]-d[1])
        if abs(area) < 1e-6:
            return
        yy, xx = np.ogrid[lo[1]:hi[1], lo[0]:hi[0]]
        xx, yy = xx+.5, yy+.5
        u = ((b[1]-d[1])*(xx-d[0])+(d[0]-b[0])*(yy-d[1]))/area
        v = ((d[1]-a[1])*(xx-d[0])+(a[0]-d[0])*(yy-d[1]))/area
        w = 1-u-v
        mask = (u >= -1e-5) & (v >= -1e-5) & (w >= -1e-5)
        if depth is not None and depth[0] > 0:
            values = u*depth[0]+v*depth[1]+w*depth[2]
            region = self.depth[lo[1]:hi[1], lo[0]:hi[0]]
            mask &= values >= region-1e-7
            region[mask] = values[mask]
        rgb = u[..., None]*c[0]+v[..., None]*c[1]+w[..., None]*c[2]
        self.pixels[lo[1]:hi[1], lo[0]:hi[0]][mask] = np.clip(rgb[mask], 0, 255).astype(np.uint8)

    def polygon(self, points, colors, depth=None):
        for i in range(1, len(points)-1):
            ids = (0, i, i+1)
            self.triangle([points[j] for j in ids], [colors[j] for j in ids], None if depth is None else [depth[j] for j in ids])

    def image(self):
        return Image.fromarray(self.pixels).resize((self.width, self.height), Image.Resampling.LANCZOS)


def render_scene(data, scale=2):
    canvas = Raster(data['width'], data['height'], scale)
    for face in data['faces']:
        canvas.polygon(face['points'], face.get('colors', [face['rgb']] * len(face['points'])),
                       face.get('depth') if face.get('layer') == 4 else None)
    return canvas.image()
