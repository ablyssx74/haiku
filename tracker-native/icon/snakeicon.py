#!/usr/bin/env python3
"""Draws the snaketracker icon (a smiling yellow snake, coiled, head up) as a list of filled polygons, and writes
it as an SVG preview and as a Haiku vector icon (HVIF)."""
import math, struct, sys

# ------------------------------------------------------------------ geometry
def ellipse(cx, cy, rx, ry, n=48, rot=0.0):
    pts = []
    c, s = math.cos(rot), math.sin(rot)
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = rx * math.cos(a), ry * math.sin(a)
        pts.append((cx + x * c - y * s, cy + x * s + y * c))
    return pts

def bezier(p0, p1, p2, p3, n=24):
    out = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        out.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                    u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    return out

def tube(center, widths, caps=True):
    """A closed outline around a polyline whose width varies along it (widths: list, one per point)."""
    left, right = [], []
    n = len(center)
    for i, (x, y) in enumerate(center):
        a = center[max(i - 1, 0)]
        b = center[min(i + 1, n - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        d = math.hypot(dx, dy) or 1
        nx, ny = -dy / d, dx / d
        w = widths[i] / 2
        left.append((x + nx * w, y + ny * w))
        right.append((x - nx * w, y - ny * w))
    outline = left + right[::-1]
    return outline

def lerp_widths(a, b, n):
    return [a + (b - a) * i / (n - 1) for i in range(n)]

def grow(pts, amount):
    """Offset a roughly convex polygon outwards from its centroid (for outlines)."""
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    out = []
    for x, y in pts:
        dx, dy = x - cx, y - cy
        d = math.hypot(dx, dy) or 1
        out.append((x + dx / d * amount, y + dy / d * amount))
    return out

# ------------------------------------------------------------------ drawing
shapes = []   # (name, points, style)   style: ('solid', (r,g,b,a)) or ('grad', (x0,y0), (x1,y1), [(t, (r,g,b,a)), ...])

OUTLINE = (92, 58, 4, 255)
def solid(r, g, b, a=255): return ('solid', (r, g, b, a))
def vgrad(y0, y1, stops): return ('grad', (0, y0), (0, y1), stops)

YELLOW_TOP = (255, 241, 120, 255)
YELLOW_MID = (252, 205, 40, 255)
YELLOW_BOT = (232, 150, 8, 255)

def body(name, pts, y0, y1, outline=1.1):
    shapes.append((name + '-line', grow(pts, outline), solid(*OUTLINE)))
    shapes.append((name, pts, vgrad(y0, y1, [(0, YELLOW_TOP), (0.45, YELLOW_MID), (1, YELLOW_BOT)])))

# soft shadow on the ground
shapes.append(('shadow', ellipse(32, 60.3, 27, 3.4), solid(40, 24, 0, 70)))

# tail tip, curling out at the right of the bottom coil
tail_center = bezier((46, 55), (56, 56), (62, 50), (58, 41), 20)
tail = tube(tail_center, lerp_widths(8.0, 1.2, len(tail_center)))
body('tail', tail, 40, 58)

# the three coils, bottom to top
coils = [(32, 52.2, 26.0, 8.6), (32, 44.6, 21.6, 7.8), (32, 37.4, 17.2, 6.8)]
for i, (cx, cy, rx, ry) in enumerate(coils):
    pts = ellipse(cx, cy, rx, ry)
    body('coil%d' % i, pts, cy - ry, cy + ry)
    # belly-side highlight along the front rim and a row of scale marks
    rim = []
    for k in range(0, 17):
        a = math.radians(18 + (144 * k / 16))
        rim.append((cx + (rx - 4.2) * math.cos(a), cy + (ry - 3.0) * math.sin(a)))
    for k, (x, y) in enumerate(rim):
        if k % 2 == 0:
            shapes.append(('scale%d_%d' % (i, k), ellipse(x, y, 1.9, 1.35, 12), solid(205, 120, 6, 200)))

# neck, rising from the top coil
neck_center = bezier((32, 41.0), (32.6, 34.0), (27.5, 29.5), (32, 22), 28)
neck = tube(neck_center, lerp_widths(10.6, 8.6, len(neck_center)))
neck_base = ellipse(32, 40.2, 7.2, 3.3, 28)
NECK_FILL = vgrad(22, 43.5, [(0, YELLOW_TOP), (0.45, YELLOW_MID), (1, YELLOW_BOT)])
shapes.append(('neck-line', grow(neck, 1.1), solid(*OUTLINE)))
shapes.append(('neck-base-line', grow(neck_base, 1.1), solid(*OUTLINE)))
shapes.append(('neck-base', neck_base, NECK_FILL))
shapes.append(('neck', neck, NECK_FILL))

# head
head = ellipse(32, 15.2, 11.6, 10.0, 56)
body('head', head, 5, 25.5, 1.2)
# cheeks (a touch of warm colour)
shapes.append(('cheek-l', ellipse(22.3, 19.6, 2.8, 1.8, 16), solid(255, 150, 90, 150)))
shapes.append(('cheek-r', ellipse(41.7, 19.6, 2.8, 1.8, 16), solid(255, 150, 90, 150)))

# eyes, looking out of the icon
for ex in (26.4, 37.6):
    shapes.append(('eye-line%d' % ex, ellipse(ex, 13.4, 4.5, 5.2, 28), solid(*OUTLINE)))
    shapes.append(('eye%d' % ex, ellipse(ex, 13.4, 3.6, 4.3, 28), solid(255, 255, 255)))
    shapes.append(('pupil%d' % ex, ellipse(ex, 13.9, 2.0, 2.6, 24), solid(28, 18, 4)))
    shapes.append(('glint%d' % ex, ellipse(ex - 0.7, 12.6, 0.8, 0.9, 10), solid(255, 255, 255)))

# nostrils
shapes.append(('nostril-l', ellipse(30.2, 19.0, 0.5, 0.7, 8), solid(120, 70, 4, 255)))
shapes.append(('nostril-r', ellipse(33.8, 19.0, 0.5, 0.7, 8), solid(120, 70, 4, 255)))

# smile
smile_center = bezier((24.6, 21.2), (28.0, 26.6), (36.0, 26.6), (39.4, 21.2), 22)
smile = tube(smile_center, [0.6 + 1.5 * math.sin(math.pi * i / (len(smile_center) - 1)) for i in range(len(smile_center))])
shapes.append(('smile', smile, solid(120, 36, 12, 255)))
# a soft highlight on the head
shapes.append(('head-shine', ellipse(27.4, 8.4, 5.2, 2.0, 20, -0.35), solid(255, 255, 255, 120)))

# ------------------------------------------------------------------ SVG
def svg():
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 64 64"><defs>']
    gid = 0
    ref = {}
    for name, pts, st in shapes:
        if st[0] == 'grad':
            gid += 1
            ref[name] = gid
            _, p0, p1, stops = st
            out.append('<linearGradient id="g%d" gradientUnits="userSpaceOnUse" x1="%g" y1="%g" x2="%g" y2="%g">' % (gid, p0[0], p0[1], p1[0], p1[1]))
            for t, c in stops:
                out.append('<stop offset="%g" stop-color="rgb(%d,%d,%d)" stop-opacity="%g"/>' % (t, c[0], c[1], c[2], c[3] / 255))
            out.append('</linearGradient>')
    out.append('</defs>')
    for name, pts, st in shapes:
        d = 'M' + ' L'.join('%.2f,%.2f' % p for p in pts) + ' Z'
        if st[0] == 'solid':
            c = st[1]
            out.append('<path d="%s" fill="rgb(%d,%d,%d)" fill-opacity="%g"/>' % (d, c[0], c[1], c[2], c[3] / 255))
        else:
            out.append('<path d="%s" fill="url(#g%d)"/>' % (d, ref[name]))
    out.append('</svg>')
    return '\n'.join(out)

# ------------------------------------------------------------------ HVIF
def write_coord(buf, v):
    v = max(-128.0, min(192.0, v))
    if int(v * 100) == int(v) * 100 and -32.0 <= v <= 95.0:
        buf.append(int(v + 32))
    else:
        value = int((v + 128.0) * 102.0) | 32768
        buf.append(value >> 8)
        buf.append(value & 255)

def write_float24(buf, v):
    if v == 0:
        buf.extend([0, 0, 0]); return
    i = struct.unpack('<I', struct.pack('<f', v))[0]
    sign = (i & 0x80000000) >> 31
    exponent = ((i & 0x7f800000) >> 23) - 127
    mantissa = i & 0x007fffff
    if exponent >= 32 or exponent < -32:
        buf.extend([0, 0, 0]); return
    short = (sign << 23) | ((exponent + 32) << 17) | (mantissa >> 6)
    buf.extend([(short >> 16) & 255, (short >> 8) & 255, short & 255])

def hvif():
    # styles
    styles, style_index = [], {}
    for name, pts, st in shapes:
        key = repr(st)
        if key not in style_index:
            style_index[key] = len(styles)
            styles.append(st)
    buf = bytearray(b'ncif')
    buf.append(len(styles))
    for st in styles:
        if st[0] == 'solid':
            r, g, b, a = st[1]
            if a == 255:
                buf.append(3); buf.extend([r, g, b])
            else:
                buf.append(1); buf.extend([r, g, b, a])
        else:
            _, p0, p1, stops = st
            # gradient space: x runs -64..64 along the gradient; map it onto p0 -> p1
            dx, dy = p1[0] - p0[0], p1[1] - p0[1]
            length = math.hypot(dx, dy)
            s = length / 128.0
            ang = math.atan2(dy, dx)
            mid = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)
            m = [s * math.cos(ang), s * math.sin(ang), -s * math.sin(ang), s * math.cos(ang), mid[0], mid[1]]
            has_alpha = any(c[3] != 255 for _, c in stops)
            flags = 2 | (0 if has_alpha else 4)
            buf.append(2); buf.append(0); buf.append(flags); buf.append(len(stops))
            for v in m: write_float24(buf, v)
            for t, c in stops:
                buf.append(int(round(t * 255)))
                buf.extend([c[0], c[1], c[2], c[3]] if has_alpha else [c[0], c[1], c[2]])
    # paths
    buf.append(len(shapes))
    for name, pts, st in shapes:
        if len(pts) > 255: raise SystemExit('too many points in ' + name)
        buf.append(2 | 8)          # closed, no curves
        buf.append(len(pts))
        for x, y in pts:
            write_coord(buf, x); write_coord(buf, y)
    # shapes
    buf.append(len(shapes))
    for i, (name, pts, st) in enumerate(shapes):
        buf.append(10)
        buf.append(style_index[repr(st)])
        buf.append(1)
        buf.append(i)
        buf.append(0)              # flags
    return bytes(buf)

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'snaketracker'
    open(out + '.svg', 'w').write(svg())
    data = hvif()
    open(out + '.hvif', 'wb').write(data)
    print(len(shapes), 'shapes,', len(data), 'bytes')
