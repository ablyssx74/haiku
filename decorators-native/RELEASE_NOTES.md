# xfwm_decorators 1.0.7

Window decorators for Haiku made from xfwm4 themes. One decorator draws any xfwm4 theme, and the package carries 93 of them. Pick one in **Appearance > Decorator** (entries named "xfwm4: <theme>"), for example `xfwm4: b6`.

## Install
Two packages are attached: `xfwm_decorators-1.0.7-1-x86_64.hpkg` for 64-bit Haiku and `xfwm_decorators-1.0.7-1-x86_gcc2.hpkg` for 32-bit Haiku (gcc 2 / hybrid). Download the one for your system and install it **for the whole system**, not for "home". A home install is silently ignored, because app_server only loads decorators from the system location.

## Hover effect (on by default)
The close, zoom and minimize buttons light up under the pointer. app_server tells a decorator nothing while the pointer only moves over it, so the effect is done from outside and is still young: fast pointer movement over the buttons has occasionally been seen to misdraw on real hardware, and in one or two themes the minimize button stopped answering after its window had been minimized and brought back. If you see either, turn it off:

```
xfwm-hover off       turn it off
xfwm-hover on        turn it back on
xfwm-hover status
```

It takes effect within a second, with no restart. Themes that ship hover pictures (`default-4.6`, `default-4.8`) use them; for the others the button's own picture is tinted, and the tint follows the button's shape: a round button gets a round highlight and a square one a square highlight, while a button that is only a symbol on the title bar gets the whole box. A dark button gets lighter and a light one darker; the symbol keeps its colors. (`galaxy`'s round buttons still get a square highlight: their pictures have a shadow behind them that can't be told from the button.)

## Changes since 1.0.6
- **Dark desktops get dark borders.** In 65 of the themes the border colours are xfwm4's "symbolic" colours (the ones xfwm4 takes from the GTK theme), which this decorator used to draw in the theme's own light colour. When your Appearance panel colour is dark, the border colours now come from the panel colour instead, with the theme's light and dark bevel shades kept. The window frame and the grab corner at the bottom right follow, so they no longer show up stark white. Title bars keep each theme's own colours, and nothing changes on a light desktop. The decorator reloads when you switch between light and dark. Themes that draw their borders in fixed colours (about 30) keep them. Whether the desktop counts as dark is a plain brightness test on the panel colour.
- **Title position follows the theme.** `title_horizontal_offset` (set by 42 themes) is now honoured: a left aligned title starts that many pixels in from the edge, and a right aligned one ends that many in. Centred titles are unchanged.
- **Button glyph colours follow the theme's text colour.** Pictures that say "use the title text colour" (16 themes, such as `defcon-IV`, whose glyphs were black although the theme asks for white text) now use the colour the theme's themerc names. When the themerc names none, the picture's own colour stays.

Earlier (1.0.3 to 1.0.6):
- **Ghost pieces of the title bar fixed.** After a window was made narrower, a stale copy of the right end of its title bar could stay on the desktop, up to 70 pixels past the window (seen on `retro`, and cleared only by closing the window). The area a decorator claims was larger than the bar it draws. It is now exactly the bar, and what the bar covered is remembered across moves of the window.
- **Left border gaps fixed.** In 18 themes (for example `adept`, `r9x`, `next`, `kde`, `keramik`) a gap of 1 to 8 pixels showed between the left border and the window. The border is now placed by its inner edge.
- **Full width title bars.** The title bar is one tab across the whole window when the theme asks for it (`full_width_title`, on in most themes and xfwm4's default), instead of a short tab and a filler. This also fixes a hole in the title bar of `metabox`. `title_alignment` (left, center, right) is honoured.
- **The title goes between the buttons.** A theme with a button on the left (such as `adept`) no longer hides the first letters of the title under it.
- **Readable titles.** About 50 themes name no text color; they get light text on a dark bar and dark text on a light one, instead of black.
- **The xfce default themes** (`default-4.4`, `4.6`, `4.8`) now show glyphs on their buttons and the shading of their bar: their real artwork is in PNG files next to placeholder XPMs, and the add-on lays the PNG over the placeholder. They still use their placeholder colors, since xfwm4 takes them from the GTK theme.
- Tools for testing themes (in the source): `scan_themes.py` lists missing pictures and size mismatches; `edgetest.cpp` with `edge_measure.py` measures every edge of a window in every theme; `opstest.cpp` resizes, moves, retitles and minimizes a window and looks for pieces left behind.

## What you get
- The theme's own artwork for the title bar, borders and corners, including extended title bars and bottom-right grab bars.
- No sliding title tab: the tab is part of the artwork and stays put.
- Stacked windows: one tab per window in the same title bar, the front tab highlighted. Tabs can't be dragged into a new order.
- Close, zoom and minimize buttons placed as the theme's `button_layout` says, including themes with buttons on the left.
- Titled, document, floating, modal and bordered windows. Resizing from the borders, corners and the document knob works.

## Limits
- The shade, stick and menu buttons of xfwm4 are not drawn or handled.
- The left-titled look falls back to the default decorator.
- Modal and bordered windows look thin or faint in a few themes, because those themes don't ship artwork for them. `b5` has no maximize or hide buttons of its own.
- The xfce default themes show their placeholder colors, not your desktop's.
- Only PNG and XPM pictures are read; SVG is not.
- Decorators run inside app_server and are built against its revision. After a Haiku update that changes app_server, the add-ons may need a rebuilt package; if the desktop misbehaves, run `setdecor Default` or choose the default decorator in Appearance.

## Credits
The themes belong to their authors; see `AUTHORS` and `COPYING` in the package's documentation folder.
