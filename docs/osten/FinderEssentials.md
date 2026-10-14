# Finder essentials: first public-preview pass

The OSTen Finder image now includes these ordinary file-management capabilities:

- Rename a single selected item via **File > Rename** or its context menu.
  Rename refuses invalid names, collisions, and writes in the locked System
  Folder. Folder contents refresh after a successful rename.
- **Get Info** displays name, kind, path, modified time, and byte size for
  regular files. Selection is required.
- **Special > Empty Trash** and the Trash context menu require confirmation
  before recursively deleting Trash contents. The OS Trash folder is preserved.
- Folder windows have vertical scrollbars and wheel scrolling for long lists.
- **View > Arrange by Name / Arrange by Kind / Clean Up**, plus background
  context-menu actions, keep icons on a grid; the name/kind preference is
  stored as a BFS folder attribute. Kind puts folders before other files.
- The desktop now shows ordinary items stored in /boot/home/Desktop alongside
  the volume and Trash icons. Opening, dragging, renaming, Get Info, and new
  desktop folders reuse Finder operations.

The standalone toolchain-free code tests include rename safety, collision
avoidance, and Trash removal, bringing native regression coverage to 21 cases.

Remaining separate features from the long-term product charter (not falsely
claimed by this milestone): user-defined free icon placement, aliases, seven
labels, spring-loaded folders, pop-up folders, Find File, and full OS-enforced
Owner permissions. Further QEMU UI interaction validation is required; the
successful compilation and boot of the milestone must be checked separately.


## October 14: local interactive QEMU feedback

- Finder's minimize/windowshade control collapses only the window client area,
  leaving its Platinum title bar visible; clicking again restores its height.
  An open shaded folder retains its full-size spatial window frame for the
  next launch.
- Finder clamps zoom and dragging to keep its decorative title below the
  permanent 26-pixel System menu bar.
- **OSTen > Restart... / Shut Down...** use confirmation and the native Haiku
  registrar shutdown sequence (BRoster::Private::ShutDown), not a forced quit.
- Corrected the 21-test QEMU smoke harness's stale 18-case final comparison.
  Run 38 successfully compiled, booted and produced all 21 PASS records,
  but that stale check marked the workflow failed.
