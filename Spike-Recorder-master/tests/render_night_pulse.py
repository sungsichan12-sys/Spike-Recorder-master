"""Draw the actual plugin render callbacks without opening a window or audio."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from scene_raster import Raster
import numpy as np
import sys
root = Path(__file__).resolve().parent
scale = 2
font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 15*scale)
def rgba(n):
    n = int(n)
    return ((n >> 24) & 255, (n >> 16) & 255, (n >> 8) & 255, n & 255)
for path in root.glob('night-pulse-*.tsv'):
    if len(sys.argv)>1 and path.stem not in sys.argv[1:]:
        continue
    rows = [line.split('\t') for line in path.read_text(encoding='utf-8').splitlines()]
    w,h = map(int, rows[0][1:])
    canvas = Raster(w,h,scale)
    for row in rows[1:]:
        if row[0] == 'R':
            x,y,rw,rh = map(int,row[1:5])
            color = rgba(row[5]); alpha=color[3]/255
            region=canvas.pixels[max(0,y*scale):max(0,(y+rh)*scale),max(0,x*scale):max(0,(x+rw)*scale)]
            region[:] = np.asarray(color[:3])*alpha+region*(1-alpha)
        elif row[0] in ('P','G'):
            xy = list(map(float,row[1:7]))
            colors = [rgba(v)[:3] for v in row[7:10]] if row[0]=='G' else [rgba(row[7])[:3]]*3
            canvas.triangle([(xy[i],xy[i+1]) for i in (0,2,4)],colors, list(map(float,row[10:13])) if len(row)>=13 else None)
        elif row[0] == 'T':
            image = Image.fromarray(canvas.pixels)
            draw = ImageDraw.Draw(image, 'RGBA')
            x,y,c,alignment = map(int,row[1:5])
            label = row[5]
            length=draw.textlength(label,font=font)
            x=x*scale-(length/2 if alignment==1 else length if alignment==2 else 0)
            draw.text((x,y*scale),label,font=font,fill=rgba(c),anchor='lt')
            canvas.pixels = np.array(image)
    canvas.image().save(path.with_suffix('.png'))
