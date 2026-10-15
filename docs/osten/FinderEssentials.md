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


## October 14: local Run 39 follow-up

- Correct MacDecorator's minimize-button hit-testing: the base RegionAt()
  omitted the minimize rectangle, so the drawn WindowShade button did not
  deliver any action even though double-click shading worked.
- Track Finder's actual zoomed frame so pressing Zoom a second time restores
  its previous size despite reserving the permanent System menu bar.
- Follow symlinks when classifying and opening Finder directories, including
  the Home shortcut in the desktop directory.
- Allow the unclosable desktop window to accept system quit requests; its
  previous QuitRequested() refusal aborted native shutdown and restart.
- These changes need validation by a fresh OSTen preview build and local QEMU
  interaction. They are not confirmed merely by compiling.


## Run 40 local QEMU follow-up

- The shade button, zoom toggle and orderly Shut Down work interactively.
- The desktop Home folder is drawn but cannot be clicked or selected because
  the full-screen DesktopView is the first sibling in the hit-test order.
  Make FolderView a child of DesktopView so icon events reach it.
- Restart stops in a Haiku debugger reporting a stopped launch_daemon thread.
  Root cause is not yet established; obtain the thread list and backtrace
  before changing restart implementation.
- Home and restart remain unverified until the next interactive test.


## Run 40 restart stack recursion diagnostic

- A local restart produced a debugger backtrace with over 4,600 frames of
  LaunchDaemon::_LaunchJob, alternating between recursive launch paths.
- Track the active _LaunchJob call chain and reject a repeated job while
  printing OSTEN_LAUNCH_CYCLE and the job names. This prevents an unbounded
  stack overflow and provides evidence for the underlying launch graph bug.
- Keep the ordinary shutdown/restart API. Do not declare restart resolved
  until a guest-initiated reboot succeeds in QEMU and the subsequent startup
  confirms that all required OSTen services are running.


## Run 40: System Folder Access visibility

- Local alpha feedback: Unlock shows a confirmation but appears to do nothing.
- On a confirmed Unlock, Finder now verifies /boot/System Folder exists,
  changes its session-local edit state and opens the folder window.
- Lock displays explicit confirmation that the Finder protection is restored.
- This is currently Finder application-level write protection, not full
  OS-enforced filesystem permissions; retaining the original system directory
  and its startup contents is important.
- Validate in a new preview: Unlock should open the folder; New Folder should
  be permitted while unlocked, refused when locked, and Finder restart should
  restore locked status.
