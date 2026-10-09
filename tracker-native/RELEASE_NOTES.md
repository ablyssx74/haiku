# snaketracker 1.0.23

Tracker, Deskbar and menus with the hDesktop "snake trail" look.

## What's new
- **The scroll bar thumb sits on the track without a box around it.** The thumb was drawn on a patch of a slightly different grey from the track beside it, which showed as a lighter rectangle around the pill. The patch now uses the same colour as the track.

Since 1.0.22 (if you skipped it):
- **Scroll bars with nothing to scroll no longer fall back to the stock look.** When a window's content fits, its scroll bar now gets the same flat track as the others, with no thumb, so it reads as an empty trough. The arrow buttons keep their stock disabled look.

Since 1.0.21 (if you skipped it):
- **The snake trail starts at the Deskbar.** The Deskbar's own menu (the leaf menu) now carries the trail from the Deskbar to the row you are on. When the menu opens beside the Deskbar (vertical, expanded), a strip runs down the edge facing the Deskbar from the top of the menu to your row, and the window border between the two is covered so the strip runs into the Deskbar. When it opens below the leaf (a top bar, or a vertical bar that is not expanded) the strip runs down the side the leaf is on, from the top of the menu. When it opens above the leaf (a bottom bar) it runs from your row to the bottom of the menu.
- **Scroll bars match the selector.** The scroll bar thumb is a rounded pill in the selector colour, lighter down its middle, on a flat track (SnakeControlLook only). In a window that is not active the pill stays, washed out towards grey. The colour is the same one the menus use, so it follows hDesktop's Selector Color and not the window decorator's theme.

Since 1.0.20 (if you skipped it):
- **The vertical part of the trail bulges too.** Where a submenu's selected row is not level with the row that opened it, the strip joining the two now hangs a few pixels into the parent menu, rounded at its free end and outlined on its inner side, so the whole trail reads as one shape. Where the submenu's row is lower (or higher) than the parent menu reaches, the strip carries on past the parent's edge over the desktop, so it no longer disappears as you move down a long menu. When the rows are level there is no vertical part and nothing changes.

Since 1.0.17 (if you skipped it):
- **The selected row bulges out of the menu.** The row you are on sticks out a few pixels past the menu's outer edge, as a small outlined tab. It is on the edge away from the neighbouring menu, so it flips sides when a submenu opens on the other side of the screen.
- **Dark outline.** The selector has a one pixel dark outline that follows the whole trail.
- **Flat selector.** The selector is a single flat colour inside the outline. The light top edge and dark bottom edge are still available: turn off **Tracker preferences > Windows > Flat menu selector** to bring them back. The setting applies to Tracker and Deskbar menus, to every program's menus when SnakeControlLook is the control look, to menu bar titles and to the seam between submenu windows. hDesktop can override it with its `nav_snake_flat` setting while it runs.

## Install
Two packages are attached: `snaketracker-1.0.23-1-x86_64.hpkg` for 64-bit Haiku and `snaketracker-1.0.23-1-x86_gcc2.hpkg` for 32-bit Haiku (gcc 2 / hybrid). Download the one for your system and install it **for the whole system**, not for "home". A home install is silently ignored, because the file that tells Haiku to start the new Tracker and Deskbar is only read from the system location.
