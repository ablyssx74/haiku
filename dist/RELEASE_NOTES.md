# snaketracker 1.0.0

Tracker, Deskbar and menus with the hDesktop "snake trail" look, as an installable Haiku package.

## What you get
- **Tracker and Deskbar menus**: a rounded, beveled selector that stays joined through every open submenu
  (the "snake trail"). The Desktop right-click menu, New, Mount, Add-ons, the Be menu, Recent
  documents/folders/applications, and so on.
- **SnakeControlLook** (optional, Appearance > Control look): the same look for *every* program's menus,
  including tray replicants such as ProcessController and software you install later.
- **Selector color**: follows hDesktop's Selector Color while hDesktop is running; otherwise Tracker
  preferences > Windows > "Menu selector color" (and "Snake trail in folder menus" to turn the trail off);
  otherwise blue.

## Install
Download `snaketracker-1.0.0-1-x86_64.hpkg` and copy it to `/boot/home/config/packages/`
(or double-click it in Tracker). Then **log out and in or restart**: the launch daemon starts the new
Tracker and Deskbar at the next boot. To get the look in every program, open Appearance and choose
**SnakeControlLook** as the control look, then restart the programs you want it in.

## Remove
Delete the package from `/boot/home/config/packages/` (or `pkgman uninstall snaketracker`), choose the
default control look in Appearance, and restart. The system's own Tracker and Deskbar come back.

## Notes
- It installs next to the system's Tracker and Deskbar (`/boot/system/apps/SnakeTracker`) instead of
  replacing them, plus a launch file `data/user_launch/x-snake-tracker`. Removing the package removes
  all of it.
- Built from the sources in this repository (`tracker-native/`, `src/add-ons/control_look/SnakeControlLook`),
  which track upstream Haiku master as of 2026-10-05, and tested on Haiku R1/beta6 (hrev60200), x86_64.
  Other revisions may work but are untested.
- Not covered: the Deskbar's expanding app list and team menus keep the stock look.
