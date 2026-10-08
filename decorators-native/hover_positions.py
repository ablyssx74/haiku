#!/usr/bin/env python3
"""Where the close button of each theme is in edgetest's window (content at (400,300)-(799,599), which can't be zoomed):
prints "theme x y" (the middle of the button) for every theme, for hover tests.

    hover_positions.py [themes-folder]
"""
import importlib.util
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("scan", os.path.join(here, "scan_themes.py"))
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)

FRAME_LEFT, FRAME_TOP, FRAME_RIGHT = 400, 300, 799


def positions(root):
    for name in sorted(os.listdir(root)):
        folder = os.path.join(root, name)
        if not os.path.isfile(os.path.join(folder, "themerc")):
            continue
        rc = scan.read_themerc(os.path.join(folder, "themerc"))
        layout = rc.get("button_layout", "O|HMC")
        offset = int(rc.get("button_offset", "0") or 0)
        spacing = int(rc.get("button_spacing", "0") or 0)
        close = scan.parse_xpm(os.path.join(folder, "close-active.xpm"))
        title = scan.parse_xpm(os.path.join(folder, "title-3-active.xpm"))
        if close is None or title is None or close[2] is None:
            continue
        width = close[0]
        divider = layout.find("|")
        if divider < 0:
            divider = len(layout)
        present = set("CH")          # the test window can't be zoomed
        left = [c for i, c in enumerate(layout) if i < divider and c in present]
        right = [c for i, c in enumerate(layout) if i > divider and c in present]
        if "C" in right:
            x = FRAME_RIGHT - offset
            for c in reversed(right):
                if c == "C":
                    break
                x -= width + spacing
            cx = x - width / 2.0 + 0.5
        elif "C" in left:
            x = FRAME_LEFT + offset
            for c in left:
                if c == "C":
                    break
                x += width + spacing
            cx = x + width / 2.0 - 0.5
        else:
            continue
        top = FRAME_TOP - title[1]
        cy = top + (close[2][1] + close[2][3]) / 2.0
        yield name, int(round(cx)), int(round(cy))


if __name__ == "__main__":
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "ablyss", "xfwm4-themes-4.10.0", "themes")
    for name, x, y in positions(root):
        print(name, x, y)
