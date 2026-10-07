# xfwm_decorators 1.0.2

Window decorators for Haiku made from xfwm4 themes. One decorator draws any xfwm4 theme, and the package carries 93 of them. Pick one in **Appearance > Decorator** (entries named "xfwm4: <theme>"), for example `xfwm4: b6`.

## Install
Two packages are attached: `xfwm_decorators-1.0.2-1-x86_64.hpkg` for 64-bit Haiku and `xfwm_decorators-1.0.2-1-x86_gcc2.hpkg` for 32-bit Haiku (gcc 2 / hybrid). Download the one for your system and install it **for the whole system**, not for "home". A home install is silently ignored, because app_server only loads decorators from the system location.

## Changes since 1.0.0
- Hardened the ghost-image fix: the old title bar area is now always redrawn, even after other layout changes (such as a focus change) happened in between.
- Fixed a ghost image left on the desktop when a window's title got shorter and the title bar shrank.

## What you get
- The theme's own artwork for the title bar, borders and corners, including extended title bars and bottom-right grab bars.
- No sliding title tab: the tab is part of the artwork and stays put.
- Stacked windows: one tab per window in the same title bar, the front tab highlighted. Tabs can't be dragged into a new order.
- Close, zoom and minimize buttons placed as the theme's `button_layout` says, including themes with buttons on the left.
- Titled, document, floating, modal and bordered windows. Resizing from the borders, corners and the document knob works.

## Limits
- The shade, stick and menu buttons of xfwm4 are not drawn or handled.
- The left-titled look falls back to the default decorator.
- Modal and bordered windows look thin or faint in a few themes (for example `default-4.8`), because those themes don't ship artwork for them.
- Decorators run inside app_server and are built against its revision. After a Haiku update that changes app_server, the add-ons may need a rebuilt package; if the desktop misbehaves, run `setdecor Default` or choose the default decorator in Appearance.

## Credits
The themes belong to their authors; see `AUTHORS` and `COPYING` in the package's documentation folder.
