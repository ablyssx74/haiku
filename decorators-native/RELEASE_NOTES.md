# xfwm_decorators 1.0.15

Window decorators for Haiku made from xfwm4 themes. One decorator draws any xfwm4 theme, and the package carries 96 of them. Pick one in **Appearance > Decorator** (entries named "xfwm4: <theme>"), for example `xfwm4: b6`.

## Install
Two packages are attached: `xfwm_decorators-1.0.15-1-x86_64.hpkg` for 64-bit Haiku and `xfwm_decorators-1.0.15-1-x86_gcc2.hpkg` for 32-bit Haiku (gcc 2 / hybrid). Download the one for your system and install it **for the whole system**, not for "home". A home install is silently ignored, because app_server only loads decorators from the system location.

## Hover effect (on by default)
The close, zoom and minimize buttons light up under the pointer. app_server tells a decorator nothing while the pointer only moves over it, so the effect is done from outside and is still young: fast pointer movement over the buttons has occasionally been seen to misdraw on real hardware, and in one or two themes the minimize button stopped answering after its window had been minimized and brought back. If you see either, turn it off:

```
xfwm-hover off       turn it off
xfwm-hover on        turn it back on
xfwm-hover status
```

It takes effect within a second, with no restart. Themes that ship hover pictures (`default-4.6`, `default-4.8`) use them; for the others the button's own picture is tinted, and the tint follows the button's shape: a round button gets a round highlight and a square one a square highlight, while a button that is only a symbol on the title bar gets the whole box. A dark button gets lighter and a light one darker; the symbol keeps its colors. (`galaxy`'s round buttons still get a square highlight: their pictures have a shadow behind them that can't be told from the button.)

## Changes since 1.0.14
- **The hover effect is round on the buttons of a window that is not the active one, as it is on the active one.** A theme without hover pictures lights only the shape of a button, found by comparing its picture with the title bar behind it. A button picture of an inactive window is pale and close to the bar's color, so little of it was taken for the button and the whole rectangle was lit, as a square. The outline is now taken from the picture of the same button in the active window when both have the same size. In about 40 themes (`agua`, `agualemon`, `curve`, `gelly`, `gtk`, `iceg`, `pills`, `platinum`, `oroborus`, `quiet-purple`, `silverado` and others) the buttons of an inactive window light up round now. `wildbush` draws no buttons at all in a window that is not the active one, so it has nothing to light.
- **No stale pieces of the old title bar after a window leaves a stack of tabs.** The first move of a window dragged out of a group of tabs left vertical seams across its title (the ends of the tabs it had been grouped with) until the window or the desktop was clicked. app_server moves a window by copying what is on the screen, and what was there was the group's bar. The title bar is now drawn again once after the first move that follows a change of tabs or focus. The bars of the windows that stay in the stack, and of a window that joins one, are drawn again over what they covered before as well.

Changes in 1.0.14 (if you skipped it):
- **Bottom corners are right in 39 themes.** The bar along the bottom was drawn across a width taken from the border width, and before the corner pictures. In `adept`, `bbs`, `kde`, `next`, `metabox`, `totem`, `keramik`, `kindaker` and others that made the bar stick out past the side borders at both corners, and in themes with rounded corners (`b6`, `curve`, `default-4.0`, `blackwall`, `exocet` and more) the square end of the bar showed through the rounded corner as a lone pixel. The bar now runs between the two corner pictures, as in xfwm4, so the corners have the shape their themes give them. The top border of a modal window is laid out the same way.
- **Borders follow a dark or a light desktop in the themes with fixed colors.** 60 themes already took the colors of their borders from Appearance. The other 36 kept theirs, which left a light frame on a dark desktop. Their side and bottom pictures are now moved to the brightness of the panel color, with the highlights and shadows kept relative to it and the hue of the picture kept (a dark frame on a light desktop is lightened the same way). A strongly colored theme (`biz`, `gaudy`, `piranha`, `redmondxp` and the like) keeps its colors. The title bar and the buttons are not changed, and the theme is drawn again when the colors are changed in Appearance.
- **The resize knob in the corner of a document window matches the scroll bars.** It was filled with the color of the border's innermost pixel, which is black in a good many themes and stood out beside the scroll bars, most on a light desktop. When that color is nearly black it is the color of the scroll bars' track now (the panel color a little darker), and it follows Appearance.
- **The highlight, shadow and mid tones of the title bar come from its color.** In 56 themes the title bar is made of the colors that xfwm4 takes from the desktop theme's selection color: a base color, with a lighter highlight, a darker shadow and a mid tone made from it. The port kept the numbers written in the pictures, so `alternate` had a bare white line along the top of its blue tab and a fixed blue line at its right end. They are made from the title color now (a lightness and saturation shade of 1.3 and 0.7, as GTK 2 does), so the white line is a light tint of the blue. The title color itself is still the theme's own.

Changes in 1.0.13 (if you skipped it):
- **`b5+`: the left button of a tab after the first is in its own tab.** It was drawn inside the tab before it, because the theme places it for the first tab's wide cap. It now sits 3 pixels in from the tab's left end, as in `b6+` and `b6+r`, and the tab is made wide enough that the title starts after the button and never runs over it.

Changes in 1.0.12 (if you skipped it):
- **Windows with several tabs look right in `b6`, `b6+` and `b6+r`.** The tabs of a stack have a rounded corner at each end, with a pixel of gap between them, and the bar after the last tab carries on in the colour of the title bar instead of showing a lighter block. When you switch tabs, no pieces of the old title bar are left behind in the title bar any more (this was the tearing a user reported). The left button of every tab after the first has 3 pixels more room from the tab's corner, and the title moves along with it. A theme asks for this with `haiku_stacked_tabs=true` in its `themerc`; the other themes are drawn as before.
- **`b5` and `b5+`: the tabs that are not in front end where the front one does.** Their bottom two rows were light grey, which made a tab behind look taller than the front one; they now continue the bar. The strip of the left edge under a first tab that is not in front has the colour of the frame's left edge, where it was a pale line on a dark desktop.

Changes in 1.0.11 (if you skipped it):
- **New buttons for `b6+` and `b6+r`.** The close and zoom buttons in the tab are drawn by hand now, with more detail (a lit edge, the diagonal hatching of Haiku's close button). `b6+` has bigger square ones; the buttons of `b6+r` are a pixel higher, and the zoom button of both sits 2 pixels further in. In a window that is not the active one they are the pale yellow of its title bar.
- **A new theme, `b5+`.** `b5` with the buttons of Haiku's own decorator in the tab: the close button at the left end and a zoom button of its own at the right end. Pick it in Appearance as **xfwm4: b5+**.
- **`b5` follows a dark desktop.** Its frame was drawn in fixed light greys, so the borders stayed light whatever the colours were. They are named after the GTK theme's colours now, as `b6`'s are, so the frame and the tab's outline take the dark colours of your Appearance settings. On a light desktop they are a little softer than before, as in the other themes. The yellow tab keeps its own colours.
- **The hover look of `b6+`, `b6+r` and `b5+` is softer.** A button under the pointer in the active window is 25 % less dark, and in a window that is not the active one 60 % less dark. A theme can now ship its own hover pictures for the inactive window (`close-prelight-inactive`, `maximize-prelight-inactive` and so on); the decorator used to make that look itself by darkening the pale button, and still does for every theme that has no such picture, so no other theme changes.

Changes in 1.0.10 (if you skipped it):
- **`b6+` and `b6+r`: the hover and click colors are swapped.** A button under the pointer is the darker gold that a pressed one used to be, and a button being clicked is the grey-olive one that was the hover color. The hover and click pictures are in the theme now (`close-prelight`, `maximize-prelight`, `close-pressed` and `maximize-pressed`), so it no longer depends on how the decorator brightens a button.

Changes in 1.0.9 (if you skipped it):
- **Two new themes, `b6+` and `b6+r`.** They are b6 with the buttons of Haiku's own decorator in the tab: the close button at the left end of the tab and the zoom button at its right end, in place of the three at the far right of the bar. The buttons are pale yellow with a golden frame, square in `b6+` and round in `b6+r`; a pressed button is darker, and a window that is not the active one has paler ones. Pick them in Appearance as **xfwm4: b6+** and **xfwm4: b6+r**.
- **Themes can put their buttons in the tab.** A theme's `themerc` can now say `buttons_in_tab=true`: the buttons on the left of the `|` in `button_layout` go at the left end of the front tab, those on its right at the tab's right end, and the tab is made a little wider so the title does not run up to them. `button_offset` is the room at the left end and the new `button_offset_right` the room at the right end. No other theme uses it, so they are drawn as before.

Changes in 1.0.8 (if you skipped it):
- **Title text shadows.** The 41 themes that ask for a shadow behind the title text now get one. `true` and `under` draw a copy of the title one pixel down and to the right, and `frame` draws one on every side of the letters, which reads as an outline. A theme can name its own shadow colour (six do); otherwise the shadow is black behind light text and white behind dark text. Themes that ask for no shadow are unchanged.
- **The decorator list in Appearance is in alphabetical order.** It used to come out Z to A, because a package's directory is listed in the reverse of the order its files were added. The package now adds the decorators from Z to A, which turns the list around. This relies on how packagefs lists a directory.
- **The Deskbar's top bar no longer grows with the theme.** The Deskbar makes its Be menu bar as high as a window's title tab and asks the decorator for that height when it starts, and a theme's title artwork can be much taller than a title (35 pixels in `b6`, 42 in `kde`). The decorator now reports the height the default decorator would have for the same font. The Deskbar only reads it when it starts, so restart it, or log out and in, after you change theme.
- **Softer frames on a light desktop.** The border colours that xfwm4 takes from the GTK theme now blend halfway toward your panel colour on a light desktop too, so frames that were stark white or cream are a little softer. Dark desktops keep the full change from 1.0.7.

Changes in 1.0.7 (if you skipped it):
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
