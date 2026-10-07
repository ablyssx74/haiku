#!/usr/bin/env python3
"""Scans xfwm4 themes for what the Xfwm decorator needs: missing pictures, and sizes that disagree.

    scan_themes.py [themes-folder] [--json]

The decorator lays a frame out the way xfwm4 does: every picture of a side is anchored to the same outer edge
of the frame, so the opaque outer edge of left / top-left / bottom-left (and of right / top-right / bottom-right)
has to be the same distance from that edge, or the corners sit a pixel off the border. This reports, per theme:

  missing     a picture the decorator cannot do without (the theme is not offered), or an optional one
  left/right  the outer edge of the side's three pictures disagrees (the "pixel misalignment")
  bottom      the bottom edge of bottom / bottom-left / bottom-right disagrees
  title       title-1..5 differ in height, or the top-left / top-right corner is not as tall as the bar
  states      active and inactive pictures differ in size
  buttons     button pictures differ in size, or are taller than the bar
  themerc     settings the decorator does not honour
"""
import json
import os
import re
import sys

REQUIRED = ["top-left", "top-right", "title-3", "left", "right", "bottom", "bottom-left", "bottom-right"]
OPTIONAL = ["title-1", "title-2", "title-4", "title-5"]
BUTTONS = ["close", "maximize", "hide"]


def parse_xpm(path):
    """-> (width, height, opaque bounds (l, t, r, b) or None)"""
    try:
        text = open(path, encoding="latin-1").read()
    except OSError:
        return None
    strings = re.findall(r'"((?:[^"\\]|\\.)*)"', text)
    if not strings:
        return None
    head = strings[0].split()
    try:
        w, h, nc, cpp = (int(x) for x in head[:4])
    except (ValueError, IndexError):
        return None
    colors = {}
    for entry in strings[1:1 + nc]:
        key = entry[:cpp]
        rest = entry[cpp:].split()
        value = None
        for i, tok in enumerate(rest):
            if tok in ("c", "g", "g4", "m") and i + 1 < len(rest):
                value = rest[i + 1]
                break
        colors[key] = value
    transparent = {k for k, v in colors.items() if v is None or v.lower() == "none"}
    left = top = 10 ** 6
    right = bottom = -1
    rows = strings[1 + nc:1 + nc + h]
    for y, row in enumerate(rows):
        for x in range(0, min(len(row), w * cpp), cpp):
            if row[x:x + cpp] in transparent:
                continue
            xx = x // cpp
            left, right = min(left, xx), max(right, xx)
            top, bottom = min(top, y), max(bottom, y)
    if right < 0:
        return (w, h, None)
    return (w, h, (left, top, right, bottom))


def read_themerc(path):
    values = {}
    try:
        for line in open(path, encoding="latin-1"):
            if "=" in line:
                key, value = line.split("=", 1)
                values[key.strip()] = value.strip()
    except OSError:
        pass
    return values


def scan_theme(folder):
    name = os.path.basename(folder)
    info = {"theme": name, "missing_required": [], "missing_optional": [], "problems": {}}
    img = {}
    for state in ("active", "inactive"):
        for base in REQUIRED + OPTIONAL:
            img[(base, state)] = parse_xpm(os.path.join(folder, "%s-%s.xpm" % (base, state)))
    for state in ("active", "inactive"):
        for button in BUTTONS + ["shade"]:
            img[(button, state)] = parse_xpm(os.path.join(folder, "%s-%s.xpm" % (button, state)))
        img[("close", "pressed")] = parse_xpm(os.path.join(folder, "close-pressed.xpm"))

    for base in REQUIRED:
        if img[(base, "active")] is None:
            info["missing_required"].append(base)
    for base in OPTIONAL:
        if img[(base, "active")] is None:
            info["missing_optional"].append(base)
    missing_buttons = [b for b in BUTTONS if img[(b, "active")] is None]
    if missing_buttons:
        info["missing_optional"].extend(b + " button" for b in missing_buttons)
    no_inactive = [b for b in REQUIRED + OPTIONAL if img[(b, "active")] and img[(b, "inactive")] is None]
    if no_inactive:
        info["no_inactive"] = no_inactive

    if info["missing_required"]:
        return info

    def dims(base, state="active"):
        v = img[(base, state)]
        return None if v is None else (v[0], v[1])

    def bounds(base, state="active"):
        v = img[(base, state)]
        return None if v is None else v[2]

    problems = info["problems"]

    for state in ("active", "inactive"):
        tl, bl, lf = bounds("top-left", state), bounds("bottom-left", state), bounds("left", state)
        tr, br, rt = bounds("top-right", state), bounds("bottom-right", state), bounds("right", state)
        if None in (tl, bl, lf, tr, br, rt):
            continue
        # distance of the outer edge from the frame's left edge: opaque.left (all three are drawn from x0)
        lefts = {"top-left": tl[0], "left": lf[0], "bottom-left": bl[0]}
        if len(set(lefts.values())) > 1:
            problems.setdefault("left", []).append("%s: %s" % (state, lefts))
        # from the right edge: width - 1 - opaque.right
        rights = {"top-right": img[("top-right", state)][0] - 1 - tr[2],
                  "right": img[("right", state)][0] - 1 - rt[2],
                  "bottom-right": img[("bottom-right", state)][0] - 1 - br[2]}
        if len(set(rights.values())) > 1:
            problems.setdefault("right", []).append("%s: %s" % (state, rights))
        bt = bounds("bottom", state)
        if bt is not None:
            bottoms = {"bottom": img[("bottom", state)][1] - 1 - bt[3],
                       "bottom-left": img[("bottom-left", state)][1] - 1 - bl[3],
                       "bottom-right": img[("bottom-right", state)][1] - 1 - br[3]}
            if len(set(bottoms.values())) > 1:
                problems.setdefault("bottom", []).append("%s: %s" % (state, bottoms))
        # the left border's width is also what the bottom-left corner is anchored by
        lw = dims("left", state)[0]
        blw = dims("bottom-left", state)[0]
        if blw < lw:
            problems.setdefault("left", []).append("%s: bottom-left (%d) is narrower than left (%d)" % (state, blw, lw))

    heights = {base: dims(base)[1] for base in ["title-1", "title-2", "title-3", "title-4", "title-5"]
               if dims(base) is not None}
    if len(set(heights.values())) > 1:
        problems.setdefault("title", []).append("title pieces differ in height: %s" % heights)
    bar = dims("title-3")[1]
    for corner in ("top-left", "top-right"):
        ch = dims(corner)[1]
        if ch != bar:
            problems.setdefault("title", []).append("%s is %d tall, the bar %d" % (corner, ch, bar))

    for base in REQUIRED + OPTIONAL:
        a, i = dims(base, "active"), dims(base, "inactive")
        if a and i and a != i:
            problems.setdefault("states", []).append("%s: active %dx%d, inactive %dx%d" % ((base,) + a + i))

    sizes = {b: dims(b) for b in BUTTONS if dims(b)}
    if len(set(sizes.values())) > 1:
        problems.setdefault("buttons", []).append("buttons differ in size: %s" % sizes)
    for b, s in sizes.items():
        if s[1] > bar:
            problems.setdefault("buttons", []).append("%s button is %d tall, the bar %d" % (b, s[1], bar))

    rc = read_themerc(os.path.join(folder, "themerc"))
    layout = rc.get("button_layout", "")
    unsupported = sorted(set(layout) - set("|CMHSOT") if False else set(c for c in layout if c in "OST"))
    notes = []
    if unsupported:
        notes.append("button_layout uses %s (not drawn)" % "".join(unsupported))
    if rc.get("full_width_title", "").lower() == "true":
        notes.append("full_width_title=true")
    for key in ("title_shadow_active", "title_shadow_inactive"):
        if rc.get(key, "false") not in ("false", ""):
            notes.append("%s=%s" % (key, rc[key]))
    if notes:
        problems["themerc"] = notes
    return info


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    root = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ablyss",
                                             "xfwm4-themes-4.10.0", "themes")
    results = []
    for name in sorted(os.listdir(root)):
        folder = os.path.join(root, name)
        if os.path.isfile(os.path.join(folder, "themerc")):
            results.append(scan_theme(folder))
    if "--json" in sys.argv:
        print(json.dumps(results, indent=1))
        return

    print("%d themes\n" % len(results))
    bad = [r for r in results if r["missing_required"]]
    print("== not usable (a required picture is missing): %d" % len(bad))
    for r in bad:
        print("  %-14s %s" % (r["theme"], ", ".join(r["missing_required"])))
    usable = [r for r in results if not r["missing_required"]]
    print("\n== usable but missing optional pieces: %d" % sum(1 for r in usable if r["missing_optional"]))
    for r in usable:
        if r["missing_optional"]:
            print("  %-14s %s" % (r["theme"], ", ".join(r["missing_optional"])))
    print("\n== themes with no inactive pictures (the active ones are reused): %d"
          % sum(1 for r in usable if r.get("no_inactive")))
    for kind in ("left", "right", "bottom", "title", "states", "buttons", "themerc"):
        names = [r for r in usable if kind in r["problems"]]
        print("\n== %s: %d themes" % (kind, len(names)))
        for r in names:
            for line in r["problems"][kind]:
                print("  %-14s %s" % (r["theme"], line))
    clean = [r["theme"] for r in usable if not r["problems"] and not r["missing_optional"]]
    print("\n== nothing to report: %d themes\n  %s" % (len(clean), " ".join(clean)))


if __name__ == "__main__":
    main()
