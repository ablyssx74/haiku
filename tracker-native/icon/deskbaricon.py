#!/usr/bin/env python3
"""The Deskbar icon: the same yellow snake peeking up from behind a wooden desk, thin round glasses, a grin, eyes half
open."""
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

WOOD_LINE = (66, 36, 12, 255)
def rounded_rect(x0, y0, x1, y1, r, n=6):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts

def circle_ring(cx, cy, r, width, n=40):
    centre = [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n + 1)]
    return tube(centre, [width] * len(centre))

# ------------------------------------------------------------ the snake (behind the desk)
# a tail tip waving up at the right of the desk
tail_c = bezier((49, 44), (55, 41), (60, 36), (57, 31), 20) + bezier((57, 31), (55, 27.5), (51.5, 28.5), (52.5, 31.5), 12)[1:]
tail = tube(tail_c, lerp_widths(6.0, 0.9, len(tail_c)))
shapes.append(('tail-line', grow(tail, 1.1), solid(*OUTLINE)))
shapes.append(('tail', tail, SKIN(28, 45)))

# neck
neck_c = bezier((32, 47), (32.6, 41), (31.4, 37), (32, 30), 20)
neck = tube(neck_c, lerp_widths(11.5, 9.6, len(neck_c)))
shapes.append(('neck-line', grow(neck, 1.1), solid(*OUTLINE)))
shapes.append(('neck', neck, SKIN(28, 47)))

# head
HC = (32, 21.6)
head = ellipse(HC[0], HC[1], 13.0, 11.2, 60)
shapes.append(('head-line', grow(head, 1.2), solid(*OUTLINE)))
shapes.append(('head', head, SKIN(10, 33)))
shapes.append(('cheek-l', ellipse(20.6, 27.0, 2.6, 1.7, 16), solid(255, 150, 90, 140)))
shapes.append(('cheek-r', ellipse(43.4, 27.0, 2.6, 1.7, 16), solid(255, 150, 90, 140)))

# eyes, half open: a white, a pupil looking out of the icon, and a heavy lid over the top half
EYES = (25.9, 38.1)
EY = 21.0
for ex in EYES:
    shapes.append(('eye%g' % ex, ellipse(ex, EY, 4.2, 4.5, 28), solid(255, 255, 255)))
    shapes.append(('pupil%g' % ex, ellipse(ex, EY + 1.5, 2.1, 2.3, 24), solid(28, 18, 4)))
    shapes.append(('glint%g' % ex, ellipse(ex - 0.8, EY + 0.9, 0.7, 0.8, 10), solid(255, 255, 255)))
    # the lid: the top of the eye in the head's own colour, with a dark line along its edge
    lid = [(ex + 4.9 * math.cos(math.radians(a)), EY + 5.2 * math.sin(math.radians(a))) for a in range(180, 361, 10)]
    lid += [(ex + 4.9, EY + 0.6), (ex - 4.9, EY + 0.6)]
    shapes.append(('lid%g' % ex, lid, SKIN(10, 33)))
    edge = tube([(ex - 4.4, EY + 0.9), (ex - 1.5, EY + 0.45), (ex + 1.5, EY + 0.45), (ex + 4.4, EY + 0.9)], [1.0] * 4)
    shapes.append(('lid-edge%g' % ex, edge, solid(*OUTLINE)))

# thin round glasses
RIM = (58, 36, 10, 255)
for ex in EYES:
    shapes.append(('lens%g' % ex, ellipse(ex, EY, 5.6, 5.6, 36), solid(210, 235, 255, 38)))
    shapes.append(('rim%g' % ex, circle_ring(ex, EY, 5.7, 0.9), solid(*RIM)))
    shapes.append(('glare%g' % ex, tube(bezier((ex - 3.6, EY - 1.6), (ex - 3.2, EY - 3.4), (ex - 2.0, EY - 4.2), (ex - 0.8, EY - 4.5), 8),
                                         [0.6, 0.9, 1.0, 1.0, 0.9, 0.8, 0.7, 0.6, 0.5]), solid(255, 255, 255, 190)))
shapes.append(('bridge', tube(bezier((30.7, EY - 0.8), (31.5, EY - 2.0), (32.5, EY - 2.0), (33.3, EY - 0.8), 6), [0.9] * 7), solid(*RIM)))
shapes.append(('arm-l', tube([(20.2, EY - 0.6), (18.8, EY - 0.9), (17.6, EY - 0.3)], [0.9] * 3), solid(*RIM)))
shapes.append(('arm-r', tube([(43.8, EY - 0.6), (45.2, EY - 0.9), (46.4, EY - 0.3)], [0.9] * 3), solid(*RIM)))

# nostrils
shapes.append(('nostril-l', ellipse(30.4, 26.0, 0.5, 0.65, 8), solid(120, 70, 4, 255)))
shapes.append(('nostril-r', ellipse(33.6, 26.0, 0.5, 0.65, 8), solid(120, 70, 4, 255)))

# a wide grin with a row of teeth
lower = bezier((23.0, 27.9), (27.2, 33.4), (36.8, 33.4), (41.0, 27.9), 22)
upper = bezier((41.0, 27.9), (37.0, 29.4), (27.0, 29.4), (23.0, 27.9), 22)
shapes.append(('mouth', lower + upper, solid(150, 34, 22)))
shapes.append(('teeth', tube(bezier((24.8, 28.5), (28.0, 30.1), (36.0, 30.1), (39.2, 28.5), 14), [1.5] * 15), solid(255, 255, 255)))
shapes.append(('lip', tube(lower, [0.9] * len(lower)), solid(110, 30, 12)))
# the corners of the grin
shapes.append(('dimple-l', tube(bezier((21.8, 26.2), (22.2, 27.4), (22.6, 28.0), (23.2, 28.3), 4), [0.8] * 5), solid(110, 30, 12)))
shapes.append(('dimple-r', tube(bezier((42.2, 26.2), (41.8, 27.4), (41.4, 28.0), (40.8, 28.3), 4), [0.8] * 5), solid(110, 30, 12)))

# a soft highlight on the head
shapes.append(('head-shine', ellipse(26.6, 12.0, 5.4, 1.9, 20, -0.3), solid(255, 255, 255, 110)))

# ------------------------------------------------------------ the desk
WOOD_TOP = vgrad(39.5, 46, [(0, (232, 170, 96, 255)), (1, (190, 120, 56, 255))])
WOOD_FRONT = vgrad(46, 61.5, [(0, (176, 108, 46, 255)), (1, (126, 72, 26, 255))])
WOOD_DARK = vgrad(46, 61.5, [(0, (146, 88, 36, 255)), (1, (104, 58, 20, 255))])

shapes.append(('desk-shadow', ellipse(32, 62.2, 29, 2.0), solid(30, 18, 0, 60)))
# the front: two side panels and a drawer between them
shapes.append(('front-line', grow(rounded_rect(8.0, 45.0, 56.0, 61.6, 1.6), 1.0), solid(*WOOD_LINE)))
shapes.append(('front', rounded_rect(8.0, 45.0, 56.0, 61.6, 1.6), WOOD_FRONT))
shapes.append(('leg-l', rounded_rect(8.0, 45.0, 14.0, 61.6, 1.2), WOOD_DARK))
shapes.append(('leg-r', rounded_rect(50.0, 45.0, 56.0, 61.6, 1.2), WOOD_DARK))
shapes.append(('drawer-line', grow(rounded_rect(18.0, 48.0, 46.0, 57.0, 1.4), 0.8), solid(*WOOD_LINE)))
shapes.append(('drawer', rounded_rect(18.0, 48.0, 46.0, 57.0, 1.4), vgrad(48, 57, [(0, (196, 128, 62, 255)), (1, (160, 96, 40, 255))])))
shapes.append(('knob-line', ellipse(32, 52.5, 2.5, 2.5, 16), solid(*WOOD_LINE)))
shapes.append(('knob', ellipse(32, 52.4, 1.8, 1.8, 16), vgrad(50.6, 54.2, [(0, (255, 226, 130, 255)), (1, (200, 140, 30, 255))])))
# the top, overhanging the front
shapes.append(('top-line', grow(rounded_rect(3.0, 39.5, 61.0, 46.0, 2.4), 1.0), solid(*WOOD_LINE)))
shapes.append(('top', rounded_rect(3.0, 39.5, 61.0, 46.0, 2.4), WOOD_TOP))
shapes.append(('top-shine', tube([(6.5, 41.4), (30, 41.2), (57.5, 41.4)], [1.2] * 3), solid(255, 235, 190, 150)))
for gx, gl in ((12, 12), (30, 16), (46, 9)):
    shapes.append(('grain%d' % gx, tube(bezier((gx, 43.9), (gx + gl / 3, 43.3), (gx + 2 * gl / 3, 44.4), (gx + gl, 43.9), 8), [0.55] * 9),
                   solid(150, 88, 34, 140)))

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'snakedeskbar'
    open(out + '.svg', 'w').write(svg(shapes))
    data = hvif(shapes)
    open(out + '.hvif', 'wb').write(data)
    print(len(shapes), 'shapes,', len(data), 'bytes')
