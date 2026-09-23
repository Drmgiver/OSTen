# OSTen project charter

OSTen asks what Classic Macintosh system software might have become if its
design had evolved beyond System 8. System 8 is the behavioral and visual
starting point. Later releases can grow outward, but the first release should
feel immediately familiar to someone who used System 8.

This document records product decisions. It does **not** describe features
already implemented in the fork. The repository currently contains the Haiku
base while the OSTen user-facing system is developed.

## Architecture

- Retain Haiku's kernel, drivers, network stack, filesystem foundation, app
  server, and other low-level components where they serve OSTen's needs.
- Build an OSTen System and a new Finder. Haiku's Tracker and Deskbar are
  temporary baseline components, not the desired desktop.
- Wrap the underlying Haiku facilities in a native OSTen Toolbox for apps.
  The Haiku implementation is predominantly C++; OSTen can use C++ for direct
  integration, with stable C interfaces and selective Rust components where
  appropriate. Rust is not a prerequisite for the initial build.
- The user-facing system model is Classic Mac, not Unix. Low-level reuse does
  not define the desktop's navigation, application model, or vocabulary.

## Desktop and files

- Finder owns the spatial desktop and windows. Use a global menu bar, an
  Application menu, explicit Quit, and no Dock.
- Treat a modern two-button mouse and scroll wheel as standard. Left-click
  performs ordinary selection and direct manipulation; right-click opens a
  contextual menu without requiring a keyboard modifier. Contextual menus may
  accelerate work, but essential commands must remain available elsewhere.
- Use direct file manipulation: dragging moves an item, Option-dragging copies
  it, and dragging to Trash discards it. File replacement happens visibly at
  the destination without an installer or a separate conflict-choice mode.
- Show mounted disks on the desktop; eject disks by dragging to the Trash.
  Include aliases, seven labels, desktop printer icons, Find File, WindowShade,
  spring-loaded folders, and pop-up folders.
- Keep a visible, blessed System Folder with System, Finder, Control Panels,
  Extensions, Fonts, and Startup Items. Use one Owner who can explicitly
  unlock and change system files; do not expose a root-user workflow.
- Keep applications self-contained in their own folders, including their
  Preferences folder. Software arrives as a standard ZIP archive, is expanded,
  and is manually dragged into place. Never require an installer or silently
  register software. System updates likewise use visible file replacement.
- Use OSTen Extended storage built upon BFS, with Mac-style file semantics.
  Foreign-disk support belongs in visible extensions; avoid invisible sidecar
  files. Define an OSTen ZIP convention for metadata without creating a new
  archive format.
- Backups remain ordinary visible files and folders. A copied System Folder
  on a suitable bootable volume should remain a viable recovery path.

## System behavior

- Redraw the Platinum visual language for modern displays, including HiDPI.
  Appearance changes and Control Strip modules should take effect when added
  or removed, without a restart.
- Expose networking through a Chooser and Network Browser, while using modern
  protocols underneath. Keep printing visible through desktop printer icons.
- Use dynamic memory allocation, explicit document saving, and robust crash
  recovery and Force Quit. Provide approachable scripting and a modern
  HyperCard successor over time.
- Provide a full-screen recovery terminal by keyboard shortcut so the GUI can
  be repaired when necessary; do not make a terminal window part of ordinary
  desktop use.
- Every running third-party component must correspond to a visible file.
  Show its permissions in Get Info and its activity in About This Computer.
  No third-party background operation should be invisible to the Owner.

## Development sequence

1. Reproduce a stock Haiku x86-64 build and boot it in QEMU.
2. Establish an OSTen image profile and launch placeholder System and Finder
   applications in place of Tracker and Deskbar.
3. Implement the global menu, Finder desktop, disk and file operations, and
   System Folder semantics in small bootable increments.
4. Add the native Toolbox and progressively replace remaining visible Haiku
   conventions with the approved OSTen behavior.

The immediate acceptance criterion is a reproducible, bootable baseline.
Later features should be demonstrated on that baseline rather than claimed
from a mockup alone.

## Licensing

Preserve the licenses and notices of inherited Haiku and other third-party
components. New OSTen-authored code is intended to use the MIT license;
OSTen-authored documentation and art are intended to use CC BY 4.0. The OSTen
name and logo are reserved for project identity. Specific licensing of new
files must be recorded with those files as they are introduced.
