#!/usr/bin/env python3
"""Measures, from screenshots made by edgetest.cpp (a magenta window at (400,300)-(799,599) on a green backdrop),
where each theme's frame edges fall: how far the content (magenta) starts from where the window's frame is,
and whether desktop (green) shows between the frame art and the content.

    edge_measure.py <folder-of-screenshots>

Per theme: L R T B = content offset at the left / right / top / bottom edge (0 is right; positive: the
frame art stops short of the content, negative: it covers some of it), gap = pixels of backdrop seen between
the art and the content. Medians over many rows or columns.
"""
import os
import statistics
import subprocess
import sys

CX, CY, CW, CH = 340, 150, 560, 520          # the part of the screenshot that is read
FRAME = (400, 300, 799, 599)                   # left, top, right, bottom of the content
GREEN = (0, 255, 0)
MAGENTA = (255, 0, 255)


def load(path):
    out = subprocess.run(["magick", path, "-crop", "%dx%d+%d+%d" % (CW, CH, CX, CY), "+repage", "-depth", "8",
                          "rgb:-"], capture_output=True, check=True).stdout
    return out


def pixel(data, x, y):
    i = (y - CY) * CW * 3 + (x - CX) * 3
    return (data[i], data[i + 1], data[i + 2])


def scan(data, points):
    """points: list of (x, y) from the outside toward the content. -> (offset of first magenta, gap, art extent)"""
    first_art = None
    gap = 0
    for n, (x, y) in enumerate(points):
        p = pixel(data, x, y)
        if p == MAGENTA:
            return n, gap, first_art
        if p == GREEN:
            if first_art is not None:
                gap += 1
        elif first_art is None:
            first_art = n
    return None, gap, first_art


def measure(path):
    data = load(path)
    left, top, right, bottom = FRAME
    res = {}
    # left: scan x from left-60 .. toward the content, rows inside the content
    rows = range(top + 40, bottom - 40, 8)
    cols = range(left + 40, right - 40, 12)
    for name, sets, expected in (
        ("L", [[(x, y) for x in range(left - 60, left + 12)] for y in rows], 60),
        ("R", [[(x, y) for x in range(right + 60, right - 12, -1)] for y in rows], 60),
        ("T", [[(x, y) for y in range(top - 70, top + 12)] for x in cols], 70),
        ("B", [[(x, y) for y in range(bottom + 60, bottom - 12, -1)] for x in cols], 60),
    ):
        offs, gaps, arts = [], [], []
        for points in sets:
            n, gap, first_art = scan(data, points)
            if n is None:
                continue
            offs.append(n - expected)
            gaps.append(gap)
            if first_art is not None:
                arts.append(expected - first_art)
        if not offs:
            res[name] = None
            continue
        res[name] = (statistics.median(offs), statistics.median(gaps), statistics.median(arts) if arts else 0,
                     min(offs), max(offs))
    return res


def main():
    folder = sys.argv[1]
    print("%-14s %s" % ("theme", "off (art stops short / covers)   gap in backdrop px   art reach beyond content"))
    odd = 0
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".png"):
            continue
        r = measure(os.path.join(folder, name))
        cells, bad = [], False
        for k in "LRTB":
            v = r.get(k)
            if v is None:
                cells.append("%s: ?" % k)
                bad = True
                continue
            off, gap, art, lo, hi = v
            flag = off != 0 or gap != 0 or lo != hi
            bad = bad or flag
            cells.append("%s: off %+d gap %d art %d%s" % (k, off, gap, art, " (varies %+d..%+d)" % (lo, hi) if lo != hi else ""))
        odd += bad
        print("%-14s %s %s" % (name[:-4], "!!" if bad else "  ", "   ".join(cells)))
    print("\n%d themes with something off" % odd)


if __name__ == "__main__":
    main()
