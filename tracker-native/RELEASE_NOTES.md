# snaketracker 1.0.18

Tracker, Deskbar and menus with the hDesktop "snake trail" look.

## What's new
- **The selected row bulges out of the menu.** The row you are on now sticks out a few pixels past the menu's outer edge, as a small outlined tab, and joins the row so it reads as one shape. It is on the edge away from the neighbouring menu, so it flips sides when a submenu opens on the other side of the screen.
- **Dark outline.** The selector has a one pixel dark outline that follows the whole trail: the rows, the elbow bar down a submenu's edge and the fillets that join them. It looks right on dark and light menu themes alike.
- **Flat selector.** The selector is a single flat colour inside the outline. The light top edge and dark bottom edge are still available: turn off **Tracker preferences > Windows > Flat menu selector** to bring them back. The setting applies to Tracker and Deskbar menus, to every program's menus when SnakeControlLook is the control look, to menu bar titles and to the seam between submenu windows. hDesktop can override it with its `nav_snake_flat` setting while it runs, as it does for the selector colour and the trail.

## Install
Two packages are attached: `snaketracker-1.0.18-1-x86_64.hpkg` for 64-bit Haiku and `snaketracker-1.0.18-1-x86_gcc2.hpkg` for 32-bit Haiku (gcc 2 / hybrid). Download the one for your system and install it **for the whole system**, not for "home". A home install is silently ignored, because the file that tells Haiku to start the new Tracker and Deskbar is only read from the system location.
