#!/usr/bin/env python3
"""The snaketracker_server icon: the same smiling yellow snake, now with human teeth, surfing on a red board in front
of a globe, its tail wrapped round the globe (after the WebPositive icon)."""
import math, sys
from iconlib import *

shapes = []
def solid(r, g, b, a=255): return ('solid', (r, g, b, a))
def vgrad(y0, y1, stops): return ('grad', (0, y0), (0, y1), stops)

OUTLINE = (92, 58, 4, 255)
YELLOW_TOP = (255, 241, 120, 255)
YELLOW_MID = (252, 205, 40, 255)
YELLOW_BOT = (232, 150, 8, 255)
SKIN = lambda y0, y1: vgrad(y0, y1, [(0, YELLOW_TOP), (0.45, YELLOW_MID), (1, YELLOW_BOT)])

def rounded_rect(x0, y0, x1, y1, r, n=5):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts

def tube_with_outline(name, center, widths, fill, outline, line=1.1):
    shapes.append((name + '-line', tube(center, [w + 2 * line for w in widths]), solid(*outline)))
    shapes.append((name, tube(center, widths), fill))

# ------------------------------------------------------------ the globe
GX, GY, GR = 31.0, 31.5, 23.0
shapes.append(('globe-shadow', ellipse(GX + 1.5, GY + 2.5, GR + 1, GR + 1, 56), solid(10, 30, 60, 70)))
shapes.append(('globe-line', ellipse(GX, GY, GR + 1.3, GR + 1.3, 64), solid(14, 40, 78, 255)))
shapes.append(('globe', ellipse(GX, GY, GR, GR, 64), vgrad(GY - GR, GY + GR, [(0, (140, 210, 250, 255)), (0.5, (60, 150, 230, 255)), (1, (24, 84, 184, 255))])))
LAND = vgrad(10, 52, [(0, (120, 214, 90, 255)), (1, (50, 150, 60, 255))])
for i, (cx, cy, rx, ry, rot) in enumerate(((21, 21, 7.5, 5.2, 0.5), (45, 40, 8.2, 5.5, -0.4), (20, 40, 5.5, 3.6, 0.3), (48, 22, 4.2, 3.0, 0.2))):
    shapes.append(('land-line%d' % i, ellipse(cx, cy, rx + 0.9, ry + 0.9, 28, rot), solid(26, 100, 44, 255)))
    shapes.append(('land%d' % i, ellipse(cx, cy, rx, ry, 28, rot), LAND))
shapes.append(('globe-shine', ellipse(22, 14.5, 7.5, 3.0, 24, -0.55), solid(255, 255, 255, 110)))

# ------------------------------------------------------------ the surfboard (red)
board_c = bezier((2.5, 59.5), (20, 57.0), (44, 55.0), (61.5, 49.0), 30)
bw = [0.8 + 10.4 * math.sin(math.pi * (i / (len(board_c) - 1)) ** 0.85) for i in range(len(board_c))]
tube_with_outline('board', board_c, bw, vgrad(48, 62, [(0, (255, 120, 100, 255)), (0.45, (226, 40, 40, 255)), (1, (150, 14, 24, 255))]), (82, 8, 18, 255), 1.0)
stripe_c = bezier((8, 58.4), (22, 56.4), (44, 54.4), (56.5, 50.4), 24)
shapes.append(('board-stripe', tube(stripe_c, [1.3 * math.sin(math.pi * i / (len(stripe_c) - 1)) + 0.2 for i in range(len(stripe_c))]), solid(255, 232, 200, 230)))
shapes.append(('board-shine', tube(bezier((9, 55.6), (24, 53.2), (42, 51.8), (54, 48.6), 20), [0.9] * 21), solid(255, 255, 255, 120)))

# ------------------------------------------------------------ the tail: out from the board, round the globe
tail_c = bezier((40, 50.0), (53, 55.0), (62, 46), (57.5, 35), 26) + bezier((57.5, 35), (54, 25.5), (49, 13.5), (43.5, 16.0), 24)[1:]
tail_w = lerp_widths(6.6, 1.0, len(tail_c))
tube_with_outline('tail', tail_c, tail_w, SKIN(12, 56), OUTLINE, 1.1)

# ------------------------------------------------------------ the snake, riding the board
def body(name, pts, y0, y1, outline=1.1):
    shapes.append((name + '-line', grow(pts, outline), solid(*OUTLINE)))
    shapes.append((name, pts, SKIN(y0, y1)))

for i, (cx, cy, rx, ry) in enumerate(((31.5, 49.6, 13.0, 5.0), (31.5, 45.0, 10.6, 4.4))):
    pts = ellipse(cx, cy, rx, ry)
    body('coil%d' % i, pts, cy - ry, cy + ry)
    for k in range(0, 13, 2):
        a = math.radians(20 + 140 * k / 12)
        shapes.append(('scale%d_%d' % (i, k), ellipse(cx + (rx - 3.2) * math.cos(a), cy + (ry - 2.2) * math.sin(a), 1.5, 1.0, 10), solid(205, 120, 6, 200)))

neck_base = ellipse(31.5, 43.6, 6.4, 2.6, 28)
shapes.append(('neck-base-line', grow(neck_base, 1.1), solid(*OUTLINE)))
shapes.append(('neck-base', neck_base, SKIN(40, 47)))
neck_c = bezier((31.5, 43.4), (32.2, 38.5), (28.6, 35.0), (32, 29.5), 26)
neck_w = lerp_widths(9.2, 7.8, len(neck_c))
shapes.append(('neck-line', tube(neck_c, [w + 2.2 for w in neck_w]), solid(*OUTLINE)))
shapes.append(('neck', tube(neck_c, neck_w), SKIN(29, 45)))

head = ellipse(32, 21.0, 10.8, 9.2, 56)
body('head', head, 12, 30.5, 1.2)
shapes.append(('cheek-l', ellipse(23.4, 24.6, 2.3, 1.5, 16), solid(255, 150, 90, 150)))
shapes.append(('cheek-r', ellipse(40.6, 24.6, 2.3, 1.5, 16), solid(255, 150, 90, 150)))
for ex in (27.4, 36.6):
    shapes.append(('eye-line%d' % ex, ellipse(ex, 19.0, 4.1, 4.8, 28), solid(*OUTLINE)))
    shapes.append(('eye%d' % ex, ellipse(ex, 19.0, 3.3, 4.0, 28), solid(255, 255, 255)))
    shapes.append(('pupil%d' % ex, ellipse(ex + 0.2, 19.5, 1.8, 2.4, 24), solid(28, 18, 4)))
    shapes.append(('glint%d' % ex, ellipse(ex - 0.5, 18.2, 0.7, 0.8, 10), solid(255, 255, 255)))
shapes.append(('nostril-l', ellipse(30.4, 24.0, 0.45, 0.6, 8), solid(120, 70, 4, 255)))
shapes.append(('nostril-r', ellipse(33.6, 24.0, 0.45, 0.6, 8), solid(120, 70, 4, 255)))

# a wide open grin with human teeth: a flat upper lip, a deep curve below, a row of square teeth under the lip
lip_y = 25.8
curve = bezier((24.6, lip_y), (27.4, 33.0), (36.6, 33.0), (39.4, lip_y), 24)
shapes.append(('mouth-line', curve + [(39.4 + 0.9, lip_y - 0.9), (24.6 - 0.9, lip_y - 0.9)], solid(88, 20, 10, 255)))
shapes.append(('mouth', curve, vgrad(lip_y, 31.5, [(0, (150, 34, 22, 255)), (1, (96, 14, 10, 255))])))
tongue = ellipse(32, 30.0, 3.6, 1.5, 20)
shapes.append(('tongue', tongue, solid(226, 96, 96, 255)))
# upper teeth: six, slightly gapped, rounded at the bottom
tw, gap = 2.15, 0.25
n = 6
x0 = 32 - (n * tw + (n - 1) * gap) / 2
for k in range(n):
    x = x0 + k * (tw + gap)
    # the teeth follow the curve of the mouth a little (outer ones shorter)
    h = 3.0 - 0.45 * abs(k - (n - 1) / 2) * 0.8
    shapes.append(('tooth-line%d' % k, rounded_rect(x - 0.35, lip_y - 0.1, x + tw + 0.35, lip_y + h + 0.35, 0.9), solid(120, 70, 60, 255)))
    shapes.append(('tooth%d' % k, rounded_rect(x, lip_y, x + tw, lip_y + h, 0.7), vgrad(lip_y, lip_y + h, [(0, (255, 255, 252, 255)), (1, (232, 226, 214, 255))])))
# a few lower teeth
for k in range(4):
    x = 32 - 4 * 1.0 + k * 2.1 - 0.1
    shapes.append(('ltooth%d' % k, rounded_rect(x, 30.6 - 1.1 - abs(k - 1.5) * 0.45, x + 1.8, 31.0 + 0.0 - abs(k - 1.5) * 0.45, 0.5), solid(244, 238, 226, 255)))
shapes.append(('head-shine', ellipse(27, 14.0, 4.8, 1.8, 20, -0.35), solid(255, 255, 255, 120)))

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'snakeserver'
    open(out + '.svg', 'w').write(svg(shapes))
    data = hvif(shapes)
    open(out + '.hvif', 'wb').write(data)
    print(len(shapes), 'shapes,', len(data), 'bytes')
