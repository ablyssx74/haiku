#!/usr/bin/env python3
# Makes the xfwm4 themes "b6+", "b6+r" and "b5+": b6 (and b5) with the buttons of Haiku's own decorator in the tab (the close
# button at its left end, the zoom button at its right end) in place of the buttons of the bar. b6+ has square buttons, b6+r
# round ones, b5+ square ones with a zoom button of its own. The buttons are PNGs (with real transparency) over an empty XPM of
# the same name, which is how the decorator lays a picture with shading over a placeholder. The active pictures are drawn by
# hand (b6plus_art.py); the other states are made from them by recolour(). It also gives b5's frame the symbolic colours
# that make a theme's borders follow a dark desktop (annotate_b5()).
#   python3 make_b6plus.py ../ablyss/xfwm4-themes-4.10.0/themes
import os, re, shutil, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from b6plus_art import GLYPHS

H = 24                # the height of every button picture (the width is that of the button)
STATES = ('active', 'inactive', 'pressed', 'prelight', 'prelight-inactive')

# How much less dark the hover looks than the ramp's: a window's own buttons by 25 %, a window that is not the active one by 60 %
HOVER_ACTIVE_EASE = 0.25
HOVER_INACTIVE_EASE = 0.60

# The other states are the active picture recoloured along a ramp: the picture's darkest colour (the frame) becomes the first
# stop, its lightest one the last, and the colours between follow by their brightness.
# (b6+ and b6+r have the pale yellow of b6's inactive tab instead, which runs from #FFFFAB to #EDDD8F)
INACTIVE_YELLOW = [(200, 170, 90), (237, 221, 143), (255, 242, 141), (255, 255, 171)]

RAMPS = {
    # a window that is not the active one: pale, the frame grey-olive (b5+)
    'inactive': [(156, 154, 134), (200, 196, 170), (230, 226, 200), (250, 248, 236)],
    # under a click: grey-olive fills, the frame a shade darker
    'pressed': [(200, 150, 20), (176, 160, 96), (190, 188, 170), (205, 204, 195)],
    # under the pointer: darker gold
    'prelight': [(128, 88, 0), (201, 167, 30), (222, 190, 50), (240, 214, 96)],
}


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def brightness(rgb):
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def hex_to_rgb(text):
    return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))


def darken_like_decorator(pixels, amount):
    """What the decorator makes of a pale picture when there is no hover picture: a pixel darkens in proportion to how close its
    brightness is to the typical one (see XfwmImage::LoadBrightened). Here by `amount` (0.4 is the decorator's own)."""
    values = sorted(brightness(p) for row in pixels for p in row if p)
    mean = values[len(values) // 2]
    out = []
    for row in pixels:
        line = []
        for p in row:
            if p is None:
                line.append(None)
                continue
            weight = 1.0 - min(1.0, abs(brightness(p) - mean) / 70.0)
            k = amount * weight
            line.append(tuple(c * (1.0 - k * 0.7) for c in p))
        out.append(line)
    return out


def recolour(rows, state, inactive_ramp=None):
    """The picture (rows of "RRGGBB" or None) in the given state: a list of rows of (r, g, b, a)."""
    pixels = [[hex_to_rgb(c) if c else None for c in row] for row in rows]
    if state == 'prelight-inactive':
        # the pale picture of an inactive window, darkened as the decorator would, but by 60 % less
        pale = recolour(rows, 'inactive', inactive_ramp)
        pale_pixels = [[(p[0], p[1], p[2]) if p[3] else None for p in row] for row in pale]
        dark = darken_like_decorator(pale_pixels, 0.4)
        out = []
        for pr, dr in zip(pale_pixels, dark):
            line = []
            for pp, dd in zip(pr, dr):
                if pp is None:
                    line.append((0, 0, 0, 0))
                else:
                    c = lerp(dd, pp, HOVER_INACTIVE_EASE)
                    line.append((c[0], c[1], c[2], 255))
            out.append(line)
        return out
    if state == 'prelight':
        # the hover look of an active window: the ramp's, but 25 % of the way back to the active picture
        hover = recolour(rows, 'prelight-ramp', inactive_ramp)
        active = recolour(rows, 'active', inactive_ramp)
        out = []
        for hr, ar in zip(hover, active):
            line = []
            for h_, a_ in zip(hr, ar):
                if h_[3] == 0:
                    line.append(h_)
                else:
                    c = lerp(h_[:3], a_[:3], HOVER_ACTIVE_EASE)
                    line.append((c[0], c[1], c[2], 255))
            out.append(line)
        return out
    if state == 'prelight-ramp':
        state = 'prelight'
    values = [brightness(p) for row in pixels for p in row if p]
    low, high = min(values), max(values)
    out = []
    for row in pixels:
        line = []
        for p in row:
            if p is None:
                line.append((0, 0, 0, 0))
            elif state == 'active':
                line.append((p[0], p[1], p[2], 255))
            else:
                t = (brightness(p) - low) / max(1.0, high - low)
                ramp = (inactive_ramp if state == 'inactive' and inactive_ramp else RAMPS[state])
                pos = t * (len(ramp) - 1)
                i = min(int(pos), len(ramp) - 2)
                c = lerp(ramp[i], ramp[i + 1], pos - i)
                line.append((c[0], c[1], c[2], 255))
        out.append(line)
    return out


def picture(glyph, top, state, inactive_ramp=None):
    """A button picture: the glyph at the given row of a canvas as wide as the glyph and H high; also its width."""
    width, height, rows = GLYPHS[glyph]
    glyph_pixels = recolour(rows, state, inactive_ramp)
    canvas = [[(0, 0, 0, 0)] * width for _ in range(H)]
    for y in range(height):
        canvas[top + y] = glyph_pixels[y]
    return width, canvas


def write_png(path, width, pix):
    raw = bytearray()
    for row in pix:
        raw.append(0)
        for (r, g, b, a) in row:
            raw += bytes((int(round(r)), int(round(g)), int(round(b)), int(round(a))))
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, len(pix), 8, 6, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def write_empty_xpm(path, name, width):
    lines = ['/* XPM */', 'static char * %s_xpm[] = {' % name, '"%d %d 1 1",' % (width, H), '" \tc None",']
    for _ in range(H):
        lines.append('"' + ' ' * width + '",')
    lines[-1] = lines[-1].rstrip(',')
    open(path, 'w').write('\n'.join(lines) + '\n};\n')


# b5's frame is drawn in fixed greys, so it stayed light on a dark desktop. Naming those greys after the colours of a GTK theme
# (as b6's pictures do) lets the decorator take them from the desktop's colours instead. The tab's own colours are left alone.
B5_SYMBOLS = {'FFFFFF': 'hilight_2', 'D8D8D8': 'color_2', '989898': 'mid_2', '888888': 'shadow_2', '606060': 'shadow_2'}


def annotate_b5(themes):
    folder = os.path.join(themes, 'b5')
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.xpm') or name.startswith('close'):
            continue
        state = 'inactive' if '-inactive' in name else 'active'
        path = os.path.join(folder, name)
        text = open(path).read()

        def symbol(match):
            if ' s ' in match.group(0):
                return match.group(0)
            colour = match.group(2).upper()
            if colour not in B5_SYMBOLS:
                return match.group(0)
            return '%s #%s s %s_%s"' % (match.group(1), match.group(2), state, B5_SYMBOLS[colour])

        new = re.sub(r'^("[^"]*?c) #([0-9A-Fa-f]{6})"', symbol, text, flags=re.M)
        if new != text:
            open(path, 'w').write(new)


THEMERC = """full_width_title=false
title_vertical_offset_active=%(voffset)d
title_vertical_offset_inactive=%(voffset)d
title_alignment=left
shadow_delta_x=%(shadow_x)d
shadow_delta_y=%(shadow_y)d
button_offset=%(left)d
button_offset_right=%(right)d
button_spacing=0
button_layout=C|M
buttons_in_tab=true
active_text_color=#000000
inactive_text_color=#000000
"""

README = """%s
--

%s with the buttons of Haiku's own decorator: the close button at the left end of the tab and the zoom button at its right
end (%s), in place of the buttons of the bar. Made by decorators-native/make_b6plus.py from %s.
"""

# Each theme: the theme it is made from, its buttons (glyph, row of the glyph's top in the 24 row picture) and the themerc
# values. The glyphs' bottoms are on the same row, as they were before the pictures had more detail.
VARIANTS = {
    'b6+': dict(base='b6', shape='square', inactive_ramp=INACTIVE_YELLOW, close=('close_square', 3), zoom=('zoom_square', 5),
                themerc=dict(voffset=-2, shadow_x=-10, shadow_y=-10, left=0, right=8)),
    # (the round close button is one pixel further in than the square one; both are a pixel higher than in b6+)
    'b6+r': dict(base='b6', shape='round', inactive_ramp=INACTIVE_YELLOW, close=('close_round', 4), zoom=('zoom_round', 4),
                 themerc=dict(voffset=-2, shadow_x=-10, shadow_y=-10, left=1, right=11)),
    # (b5's tab has a wide cap at its left end, which the buttons are placed after: the close button goes back into it)
    'b5+': dict(base='b5', shape='square', close=('close_square', 4), zoom=('zoom_b5', 5),
                themerc=dict(voffset=0, shadow_x=0, shadow_y=-18, left=-17, right=5)),
}


def make(themes, name):
    v = VARIANTS[name]
    src = os.path.join(themes, v['base'])
    dst = os.path.join(themes, name)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    open(os.path.join(dst, 'themerc'), 'w').write(THEMERC % v['themerc'])
    open(os.path.join(dst, 'README'), 'w').write(README % (name, v['base'], v['shape'], v['base']))
    for button, key in (('close', 'close'), ('maximize', 'zoom')):
        glyph, top = v[key]
        for state in STATES:
            base = '%s-%s' % (button, state)
            width, canvas = picture(glyph, top, state, v.get('inactive_ramp'))
            write_empty_xpm(os.path.join(dst, base + '.xpm'), base.replace('-', '_'), width)
            write_png(os.path.join(dst, base + '.png'), width, canvas)
    for junk in ('Makefile.am', 'Makefile.in'):
        p = os.path.join(dst, junk)
        if os.path.exists(p):
            os.remove(p)
    print('made', dst)


if __name__ == '__main__':
    themes = sys.argv[1]
    annotate_b5(themes)
    for name in ('b6+', 'b6+r', 'b5+'):
        make(themes, name)
