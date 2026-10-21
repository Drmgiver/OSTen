# OSTen Software Standard
**Working Draft 0.1 | Resource architecture migration in progress**

## Purpose

OSTen evolves the classic Macintosh software philosophy on Haiku. Every
application written specifically for OSTen must be intelligible, portable,
visually coherent, and fully configurable through the **OSTen Resource Editor**.

**The target is universal resource editing:** every menu, menu order, shortcut,
graphic, string, dialog, window layout, font choice, color, sound, and other
user-facing resource of a native OSTen application must be inspectable and
editable without changing or recompiling executable code. Native controls may
be arranged or replaced by compatible alternatives. Resource IDs remain stable
when labels, ordering, or layout are changed.

This document is a normative design target, **not a claim that the current
alpha already supports every resource category**.

## Native application requirements

1. **Resources, not fixed UI:** User-facing interface elements must be defined
   in documented, editable resources. Program code may keep safe defaults for
   recovery but must prefer valid edited resources.
2. **Stable actions:** A menu or button resource names a stable application
   action ID. Rearranging or renaming it does not change the action's behavior.
   New executable behaviors require appropriately installed application code.
3. **Uniform shortcuts:** Application and global menus use the same shortcut
   mapping where they share a command. A changed shortcut must behave the same
   in the active window and in the visible menu. Reserved global keys, duplicate
   bindings, and invalid modifiers must be detected.
4. **Editable graphics:** Icons, cursors, toolbars, wordmarks, interface artwork,
   and their states are resource assets. The original approved art remains
   available as a recovery default. Edits must not degrade unrelated assets.
5. **Editable layouts:** Dialogs, buttons, menus, Control Strip modules, window
   templates, and user-facing control placement must be described by resources.
   Validate dimensions and preserve access to essential recovery controls.
6. **Application portability:** Prefer self-contained visible application
   folders and application-local settings. Install or remove through ordinary
   visible file operations.
7. **Owner control:** Resource Editor operations must respect OSTen's System
   Folder Access workflow. The existing Finder lock is a Finder-level editing
   guard, not an OS permission boundary.
8. **Safety:** Validate all resource data before use; provide reliable revert
   and recovery paths. Never interpret a resource label as executable code.
   Corrupt, absent or unrecognized resources must fall back safely.
9. **Compatibility:** Existing Haiku applications can run without satisfying
   OSTen-native compliance. Native compliance must be opt-in and verifiable.
10. **Extensibility:** Resource type identifiers, format versions, and APIs
    must remain documented for third-party apps and Control Strip modules.

## Resource storage, alpha implementation

Current resource bindings for the OSTen System and Finder use visible files:

- `/boot/System Folder/Resources/System/Interface.ostr`
- `/boot/System Folder/Resources/Finder/Interface.ostr`
- `/boot/System Folder/Resources/<Application>/<asset>.rgba`

`Interface.ostr` is a UTF-8, line-oriented `key=value` manifest with `#`
comments. Keys have stable identifiers; values are text, command-ID lists,
numeric layout properties, `left,top,right,bottom` rectangles or `R,G,B`
colors. Each command list is comma-delimited, with `separator` for dividers.
Unknown keys are ignored. Supported values are validated with safe fallback
defaults inside the application.

`*.rgba` is an exact-sized raw 8-bit RGBA bitmap, top-to-bottom,
left-to-right, four bytes per pixel, without headers or compression.
The graphical Resource Editor will eventually provide image import/export so
that users do not manipulate pixel files by hand. Resource replacement never
changes executable logic. Incorrectly sized assets are ignored.

The preview image installs the stock manifests into the visible System Folder.
Files are **currently loaded at application startup**, so edited resources
require restarting System or Finder to take effect. Live reload remains a
future standard requirement.

### Initial bindings

- **System:** global menu order, menu sections, command order and labels,
  Finder-shared shortcuts, shutdown/restart dialogs, menu reservation width,
  Control Strip labels and module order, its palette and geometry, About window
  title/labels/geometry/wordmark, notification text and selected layouts.
- **Finder:** shared shortcuts in folder and desktop windows, selected menu
  text and ordering, Rename dialog dimensions and labels, System Folder Access
  dialogs, pop-up tab sizing, Finder icon overrides, and disk/Trash states.
- Source-compiled, approved desktop artwork remains the safe fallback if
  no corresponding RGBA resource exists.

### Current limitations that must be resolved for full compliance

A native graphical Resource Editor now exists. It prompts for an
application or an `.ostr` file on launch and loads the selected manifest.
The known System, Finder and Resource Editor executables resolve to their
installed catalogs. Third-party `.ostr` files can be opened directly.
This does **not** make arbitrary compiled Haiku executables editable, nor
does it mean every OSTen UI property has been abstracted into resources.

Remaining work includes resource definitions for all visual geometry, fonts,
colors, menus and dialogs; fully editable desktop contextual menus; full icon
state handling; preference storage rules; stable container metadata; schema
versioning; atomic editing; live reload; comprehensive validation, undo and
revert; and third-party developer libraries and conformance tests.

Do not label the System or Finder **fully standard-compliant** until these
items are implemented and tested on a booted development image.

## Milestone acceptance tests

- Reorder File menu commands without code changes, and confirm each retains its
  action.
- Change an accelerator in the Finder catalog and confirm it updates both
  System's displayed menu and the active Finder window and desktop.
- Replace a disk or Trash RGBA resource with another correctly sized image,
  restart Finder, and verify the new asset appears without recompiling.
- Reposition Rename dialog buttons through its resource rectangle keys.
- Alter Control Strip colors, labels and module order while retaining working
  collapse/expand behavior.
- Remove or corrupt a resource and verify safe fallbacks.
- Confirm user-visible resources survive reboot and Owner locking behaves as
  documented.
- Confirm malformed resource values cannot break startup or execute code.

## Preservation

Do not alter the approved System and Finder visual identity, disk/Trash
artwork, Control Strip positioning, or established shortcut semantics
during migration except where a resource override explicitly requests it.

## Credits and licensing

Haiku, the BeOS tradition, and classic Macintosh interface design inform
OSTen. Preserve upstream copyright notices and individual resource licenses.
New native OSTen source follows the relevant per-file licensing declarations.
