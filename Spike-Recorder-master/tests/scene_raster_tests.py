"""Order-independent opaque depth and interpolated color reference checks."""
import numpy as np
from scene_raster import Raster

points = [(2, 2), (30, 2), (2, 30)]
red, blue = [[255, 0, 0]] * 3, [[0, 0, 255]] * 3
a, b = Raster(32, 32, 1), Raster(32, 32, 1)
a.triangle(points, red, [.2, .2, .2])
a.triangle(points, blue, [.1, .1, .1])
b.triangle(points, blue, [.1, .1, .1])
b.triangle(points, red, [.2, .2, .2])
assert np.array_equal(a.pixels, b.pixels), 'Opaque occlusion depends on face order'
assert tuple(a.pixels[5, 5]) == (255, 0, 0), 'Far surface overwrites near surface'

c = Raster(32, 32, 1)
c.triangle(points, [[255, 0, 0], [0, 255, 0], [0, 0, 255]], [.2, .2, .2])
pixel = c.pixels[10, 10]
assert (pixel > 40).all() and (pixel < 150).all(), 'Vertex colors are not interpolated'
c.triangle(points, blue)  # HUD overlays deliberately bypass depth.
assert tuple(c.pixels[5, 5]) == (0, 0, 255), 'Overlay inherited world depth'
print('PASS depth order, occlusion, interpolated color, overlay isolation (4 checks)')
