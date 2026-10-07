# snaketracker

Tracker, Deskbar and menus with the hDesktop "snake trail" look.

* Tracker and Deskbar menus: a rounded, beveled selector that stays joined through every open submenu.
* **SnakeControlLook** (Appearance > Control look): the same look for every program's menus, tray
  replicants (ProcessController and the like) and software installed later.
* Selector colour: hDesktop's Selector Color while hDesktop is running; otherwise Tracker preferences >
  Windows > "Menu selector color"; otherwise blue.

**Install it for the whole system** (the launch files point at `/system/apps`): double-click the `.hpkg` and choose "System", or copy it to `/boot/system/packages/`. A "home" install puts the files under `~/config` and the launch files then point at nothing.

The package installs to `/boot/system/apps/SnakeTracker` and a launch file in
`/boot/system/data/user_launch`, so the system's own Tracker and Deskbar are left alone. They are
replaced at the next start (log out and in, or restart). To go back, remove the package and pick the
default control look in Appearance.

Source: https://github.com/ablyssx74/haiku (`tracker-native/`, `src/add-ons/control_look/SnakeControlLook`).
