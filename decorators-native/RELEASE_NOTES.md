# xfwm_decorators 1.0.3

Window decorators for Haiku made from xfwm4 themes. One decorator draws any xfwm4 theme, and the package carries 93 of them. Pick one in **Appearance > Decorator** (entries named "xfwm4: <theme>"), for example `xfwm4: b6`.

## Install
Two packages are attached: `xfwm_decorators-1.0.3-1-x86_64.hpkg` for 64-bit Haiku and `xfwm_decorators-1.0.3-1-x86_gcc2.hpkg` for 32-bit Haiku (gcc 2 / hybrid). Download the one for your system and install it **for the whole system**, not for "home". A home install is silently ignored, because app_server only loads decorators from the system location.

## Changes since 1.0.2
- **Buttons light up under the pointer** on every theme. Themes that ship hover pictures (`default-4.6`, `default-4.8`) use them; for the others the button's own picture is tinted: its background gets lighter (darker on a light button) while the glyph keeps its colours.
- **Left border gaps fixed.** In 18 themes (for example `adept`, `r9x`, `next`, `kde`, `keramik`) a gap of 1 to 8 pixels showed between the left border and the window. The border is now placed by its inner edge.
- **Full width title bars.** The title bar is one tab across the whole window when the theme asks for it (`full_width_title`, on in most themes and xfwm4's default), instead of a short tab and a filler. This also fixes a hole in the title bar of `metabox`. `title_alignment` (left, center, right) is honoured.
- **The title goes between the buttons.** A theme with a button on the left (such as `adept`) no longer hides the first letters of the title under it.
- **Readable titles.** About 50 themes name no text colour; they get light text on a dark bar and dark text on a light one, instead of black.
- **The xfce default themes** (`default-4.4`, `4.6`, `4.8`) now show glyphs on their buttons and the shading of their bar: their real artwork is in PNG files next to placeholder XPMs, and the add-on lays the PNG over the placeholder. They still use their placeholder colors, since xfwm4 takes them from the GTK theme.
- A window that is not the active one keeps pale buttons when they are hovered.
- Tools for testing themes: `scan_themes.py` lists missing pictures and size mismatches; `edgetest.cpp` with `edge_measure.py` measures every edge of a window in every theme.

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
