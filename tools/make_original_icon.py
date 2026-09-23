"""Draws the app icon from scratch (no game artwork): dusk sky, island, tower, metal rim.
Output: packaging/Eternam.icns (+ packaging/icon_1024.png). Requires Pillow and macOS iconutil."""
import os, shutil, subprocess, random, math
from PIL import Image, ImageDraw, ImageFilter, ImageChops

SS = 2; S = 1024 * SS                       # supersampled canvas
def sc(*v): return tuple(int(x * SS) for x in v)
body = sc(100, 100, 924, 924); R = 185 * SS

def rrect(box, r):
    m = Image.new('L', (S, S), 0); ImageDraw.Draw(m).rounded_rectangle(box, r, fill=255); return m

def vgrad(box, stops):
    x0, y0, x1, y1 = box; im = Image.new('RGBA', (S, S)); d = ImageDraw.Draw(im)
    for y in range(y0, y1):
        t = (y - y0) / max(1, y1 - y0 - 1)
        for i in range(len(stops) - 1):
            if stops[i][0] <= t <= stops[i + 1][0]:
                a, b = stops[i], stops[i + 1]; u = (t - a[0]) / (b[0] - a[0] or 1)
                c = tuple(int(a[1][k] + (b[1][k] - a[1][k]) * u) for k in range(3)); break
        d.line([(x0, y), (x1, y)], fill=c + (255,))
    return im

art = Image.new('RGBA', (S, S), (0, 0, 0, 0))
horizon = 640 * SS
art.alpha_composite(vgrad((0, body[1], S, horizon), [(0, (24, 20, 72)), (0.45, (92, 48, 128)), (0.8, (236, 120, 96)), (1, (255, 196, 120))]))
art.alpha_composite(vgrad((0, horizon, S, body[3]), [(0, (40, 92, 140)), (1, (10, 28, 60))]))
d = ImageDraw.Draw(art)
rnd = random.Random(7)
for _ in range(70):                                       # stars
    x, y = rnd.randint(body[0], body[2]), rnd.randint(body[1], body[1] + 330 * SS)
    r = rnd.choice((2, 2, 3, 4)) * SS / 2
    d.ellipse((x - r, y - r, x + r, y + r), fill=(255, 250, 230, rnd.randint(140, 255)))
moon = Image.new('L', (S, S), 0); md = ImageDraw.Draw(moon)             # crescent moon
md.ellipse(sc(640, 190, 740, 290), fill=255)
md.ellipse(sc(608, 170, 708, 270), fill=0)
glow = moon.filter(ImageFilter.GaussianBlur(18 * SS)).point(lambda v: v * 0.6)
art.paste(Image.new('RGBA', (S, S), (255, 220, 170, 255)), (0, 0), glow)
art.paste(Image.new('RGBA', (S, S), (255, 240, 210, 255)), (0, 0), moon)
d = ImageDraw.Draw(art)
for i in range(9):                                        # sun glitter on the sea
    y = horizon + (18 + i * 30) * SS; w = (150 - i * 12) * SS
    d.rectangle((S // 2 - w // 2, y, S // 2 + w // 2, y + 5 * SS), fill=(255, 190, 130, 120 - i * 10))
# island
d.polygon([sc(170, 648), sc(290, 590), sc(420, 575), sc(610, 580), sc(760, 600), sc(860, 650)], fill=(44, 110, 62, 255))
d.polygon([sc(170, 648), sc(860, 650), sc(820, 668), sc(210, 668)], fill=(120, 96, 60, 255))
for cx, cy, r in ((300, 575, 34), (345, 560, 40), (700, 575, 38), (745, 590, 30)):     # trees
    d.rectangle(sc(cx - 5, cy + r - 10, cx + 5, cy + r + 22), fill=(80, 52, 30, 255))
    d.ellipse(sc(cx - r, cy - r, cx + r, cy + r), fill=(30, 96, 54, 255))
    d.ellipse(sc(cx - r * 0.6, cy - r * 0.8, cx + r * 0.4, cy + r * 0.2), fill=(58, 140, 76, 255))
# tower: tapered body, crenellated gallery, pyramid roof, lantern
tx = 512
d.polygon([sc(tx - 88, 585), sc(tx + 88, 585), sc(tx + 76, 400), sc(tx - 76, 400)], fill=(222, 128, 36, 255))
d.polygon([sc(tx - 88, 585), sc(tx - 30, 585), sc(tx - 26, 400), sc(tx - 76, 400)], fill=(190, 100, 26, 255))
d.rectangle(sc(tx - 100, 360, tx + 100, 400), fill=(236, 146, 48, 255))
for k in range(-4, 5):
    d.rectangle(sc(tx + k * 22 - 6, 368, tx + k * 22 + 6, 394), fill=(150, 72, 20, 255))
d.polygon([sc(tx - 118, 362), sc(tx + 118, 362), sc(tx, 255)], fill=(30, 92, 96, 255))
d.polygon([sc(tx - 118, 362), sc(tx, 362), sc(tx, 255)], fill=(22, 72, 78, 255))
d.rectangle(sc(tx - 26, 212, tx + 26, 262), fill=(236, 146, 48, 255))
d.polygon([sc(tx - 36, 214), sc(tx + 36, 214), sc(tx, 170)], fill=(30, 92, 96, 255))
d.rectangle(sc(tx - 7, 226, tx + 7, 248), fill=(255, 226, 120, 255))
d.rounded_rectangle(sc(tx - 24, 505, tx + 24, 585), 22 * SS, fill=(110, 50, 16, 255))
for wx in (-44, 44):
    d.rounded_rectangle(sc(tx + wx - 10, 430, tx + wx + 10, 470), 9 * SS, fill=(255, 214, 110, 255))

canvas = Image.new('RGBA', (S, S), (0, 0, 0, 0))
shadow = rrect((body[0], body[1] + 14 * SS, body[2], body[3] + 14 * SS), R).filter(ImageFilter.GaussianBlur(22 * SS))
canvas.paste(Image.new('RGBA', (S, S), (0, 0, 0, 110)), (0, 0), shadow)
canvas.paste(art, (0, 0), ImageChops.multiply(rrect(body, R), art.split()[3]))
rim = 26 * SS
inner = rrect((body[0] + rim, body[1] + rim, body[2] - rim, body[3] - rim), R - rim)
ring = ImageChops.subtract(rrect(body, R), inner)
canvas.paste(vgrad((0, body[1], S, body[3]), [(0, (222, 226, 236)), (0.5, (150, 156, 170)), (1, (92, 96, 110))]), (0, 0), ring)
hl = ImageChops.subtract(rrect((body[0] + 2 * SS, body[1] + 2 * SS, body[2] - 2 * SS, body[3] - 2 * SS), R - 2 * SS),
                         rrect((body[0] + 6 * SS, body[1] + 6 * SS, body[2] - 6 * SS, body[3] - 6 * SS), R - 6 * SS))
canvas.paste(Image.new('RGBA', (S, S), (255, 255, 255, 150)), (0, 0), hl)
edge = ImageChops.subtract(inner, rrect((body[0] + rim + 5 * SS, body[1] + rim + 5 * SS, body[2] - rim - 5 * SS, body[3] - rim - 5 * SS), R - rim - 5 * SS))
canvas.paste(Image.new('RGBA', (S, S), (20, 25, 35, 200)), (0, 0), edge)

icon = canvas.resize((1024, 1024), Image.LANCZOS)
os.makedirs('packaging', exist_ok=True)
icon.save('packaging/icon_1024.png')
iconset = 'build/Eternam.iconset'
shutil.rmtree(iconset, ignore_errors=True); os.makedirs(iconset)
for s in (16, 32, 128, 256, 512):
    icon.resize((s, s), Image.LANCZOS).save(f'{iconset}/icon_{s}x{s}.png')
    icon.resize((s * 2, s * 2), Image.LANCZOS).save(f'{iconset}/icon_{s}x{s}@2x.png')
subprocess.check_call(['iconutil', '-c', 'icns', iconset, '-o', 'packaging/Eternam.icns'])
print('packaging/Eternam.icns written')
