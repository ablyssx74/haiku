#!/usr/bin/env python3
# Makes the xfwm4 themes "b6+" and "b6+r" from b6: the same tab and frame, with the buttons of Haiku's own decorator in the
# tab (the close button at its left end, the zoom button at its right end) in place of the three at the far right of the bar.
# b6+ has square buttons, b6+r round ones. The buttons are PNGs (with real transparency) over an empty XPM of the same name,
# which is how the decorator lays a picture with shading over a placeholder.
#   python3 make_b6plus.py ../ablyss/xfwm4-themes-4.10.0/themes
import math, os, shutil, struct, sys, zlib

SS = 8   # samples per pixel along each side
W, H = 12, 24
BOX_Y = 5   # the 12 x 12 box of a button, in the 12 x 24 picture

STATES = ('active', 'inactive', 'pressed')

# For each button and state: the frame colour along the top and left, the one along the bottom and right, the fill of each shape
# (top to bottom), and how much of an edge inside the frame along the top and left is lit.
SPEC = {
    ('close', 'active'): dict(tl=(224, 171, 31), br=(241, 190, 45), fills=[((254, 253, 237), (244, 225, 128))], light=0.6),
    ('close', 'inactive'): dict(tl=(156, 154, 134), br=(156, 154, 134), fills=[((246, 244, 226), (222, 217, 184))], light=0.4),
    ('close', 'pressed'): dict(tl=(128, 88, 0), br=(128, 88, 0), fills=[((232, 200, 64), (201, 167, 30))], light=0.0),
    # (the same pale yellow as the close button's, the back one a little paler; the frame has its own two colors)
    ('zoom', 'active'): dict(tl=(190, 146, 36), br=(176, 124, 0),
                             fills=[((255, 254, 244), (250, 238, 170)), ((254, 253, 237), (244, 225, 128))], light=0.5),
    ('zoom', 'inactive'): dict(tl=(156, 154, 134), br=(156, 154, 134),
                               fills=[((250, 249, 240), (226, 222, 196)), ((246, 244, 226), (222, 217, 184))], light=0.4),
    ('zoom', 'pressed'): dict(tl=(128, 88, 0), br=(128, 88, 0),
                              fills=[((240, 214, 96), (213, 184, 50)), ((232, 200, 64), (201, 167, 30))], light=0.0),
}


def rrect(x0, y0, x1, y1, r):
    def inside(px, py, inset=0.0):
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        hx, hy = (x1 - x0) / 2 - inset, (y1 - y0) / 2 - inset
        rr = max(0.0, r - inset)
        qx = abs(px - cx) - hx + rr
        qy = abs(py - cy) - hy + rr
        d = min(max(qx, qy), 0.0) + math.hypot(max(qx, 0.0), max(qy, 0.0)) - rr
        return d <= 0.0
    inside.box = (x0, y0, x1, y1)
    return inside


def circle(cx, cy, r):
    def inside(px, py, inset=0.0):
        return math.hypot(px - cx, py - cy) <= r - inset
    inside.box = (cx - r, cy - r, cx + r, cy + r)
    return inside


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def shapes(kind, round_):
    if kind == 'close':
        return [circle(6, 6, 6) if round_ else rrect(0, 0, 12, 12, 1.5)]
    # zoom: two overlapping ones, the back one first
    if round_:
        return [circle(4, 4, 4), circle(8, 8, 4)]
    return [rrect(0, 0, 8, 8, 1.0), rrect(4, 4, 12, 12, 1.0)]


def render(kind, state, round_):
    st = SPEC[(kind, state)]
    layers = shapes(kind, round_)
    pix = [[(0.0, 0.0, 0.0, 0.0) for _ in range(W)] for _ in range(H)]
    for y in range(H):
        for x in range(W):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SS):
                for sx in range(SS):
                    px = x + (sx + 0.5) / SS
                    py = y - BOX_Y + (sy + 0.5) / SS
                    col = None
                    for index, inside in enumerate(layers):
                        if not inside(px, py):
                            continue
                        x0, y0, x1, y1 = inside.box
                        if not inside(px, py, 1.0):
                            # the frame: one colour along the top and the left, another along the bottom and the right
                            col = st['tl'] if (px - (x0 + x1) / 2) + (py - (y0 + y1) / 2) <= 0 else st['br']
                        else:
                            top, bottom = st['fills'][min(index, len(st['fills']) - 1)]
                            t = (py - y0) / max(1.0, (y1 - y0))
                            c = lerp(top, bottom, t)
                            # a pale edge inside, along the top and the left: lit from the top left
                            if st['light'] > 0 and not inside(px - 1.0, py - 1.0, 1.0):
                                c = lerp(c, (255, 255, 255), st['light'])
                            col = c
                    if col is not None:
                        acc[0] += col[0]; acc[1] += col[1]; acc[2] += col[2]; acc[3] += 1.0
            n = SS * SS
            if acc[3] > 0:
                pix[y][x] = (acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3], acc[3] / n)
    return pix


def write_png(path, pix):
    raw = bytearray()
    for row in pix:
        raw.append(0)
        for (r, g, b, a) in row:
            raw += bytes((int(round(r)), int(round(g)), int(round(b)), int(round(a * 255))))
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def write_empty_xpm(path, name):
    lines = ['/* XPM */', 'static char * %s_xpm[] = {' % name, '"%d %d 1 1",' % (W, H), '" \tc None",']
    for _ in range(H):
        lines.append('"' + ' ' * W + '",')
    lines[-1] = lines[-1].rstrip(',')
    open(path, 'w').write('\n'.join(lines) + '\n};\n')


THEMERC = """full_width_title=false
title_vertical_offset_active=-2
title_vertical_offset_inactive=-2
title_alignment=left
shadow_delta_x=-10
shadow_delta_y=-10
button_offset=%d
button_offset_right=9
button_spacing=0
button_layout=C|M
buttons_in_tab=true
active_text_color=#000000
inactive_text_color=#000000
"""

README = """%s
--

b6 with the buttons of Haiku's own decorator: the close button at the left end of the tab and the zoom button at its right
end (%s), in place of the three at the far right of the bar. Made by decorators-native/make_b6plus.py from b6.
"""


def make(themes, name, round_):
    src = os.path.join(themes, 'b6')
    dst = os.path.join(themes, name)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    # (the round close button is one pixel further in than the square one)
    open(os.path.join(dst, 'themerc'), 'w').write(THEMERC % (1 if round_ else 0))
    open(os.path.join(dst, 'README'), 'w').write(README % (name, 'round' if round_ else 'square'))
    for kind, button in (('close', 'close'), ('zoom', 'maximize')):
        for state in STATES:
            base = '%s-%s' % (button, state)
            write_empty_xpm(os.path.join(dst, base + '.xpm'), base.replace('-', '_'))
            write_png(os.path.join(dst, base + '.png'), render(kind, state, round_))
    for junk in ('Makefile.am', 'Makefile.in'):
        p = os.path.join(dst, junk)
        if os.path.exists(p):
            os.remove(p)
    print('made', dst)


if __name__ == '__main__':
    themes = sys.argv[1]
    make(themes, 'b6+', False)
    make(themes, 'b6+r', True)
