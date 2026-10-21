# OSTen Software Standard — Implementation Status
**Development snapshot | October 21, 2026 | Informational, not normative**

This document tracks current alpha progress and gaps against the
[OSTen Software Standard](SoftwareStandard.md). The Standard itself
defines policy; this file records implementation details. Where the two
differ, the Standard governs.

## Current resource storage

Current OSTen-native catalogs use visible files:

- `/boot/System Folder/Resources/System/Interface.ostr`
- `/boot/System Folder/Resources/Finder/Interface.ostr`
- `/boot/System Folder/Resources/ResourceEditor/Interface.ostr`
- `/boot/System Folder/Resources/<Application>/<asset>.rgba`

`Interface.ostr` is a UTF-8, line-oriented key/value manifest. Keys
currently refer to labels, menu order, shortcuts, positions, numeric
dimensions, colors and fonts. Applications accept valid overrides and
may use compiled defaults when no override exists.

`.rgba` files contain raw, headerless 8-bit RGBA pixels in row-major
order. They are dimension-dependent. The six original Finder images
(disk, empty/full Trash, folder, Finder and System) are staged as
editable resources with their source pixels preserved.

These formats are an **interim compatibility layer**, not the
complete, documented, versioned and extensible typed resource format
envisioned by the Standard.

## Current Resource Editor

The first ResEdit-inspired browser provides an Open dialog, a window of
resource types, a list of resources within each type, and a resource
editing window. The Finder artwork browser shows thumbnails and opens a
pixel painter with color swatches, transparent eraser and saving.

For other resource types, the editor still falls back to the earlier
property inspector. The type grouping is inferred from existing key
names and values rather than formal resource schemas or stable numeric
resource IDs.

Some currently implemented editors offer Save, Revert, Undo Save and
backup copies. **These are implementation conveniences, not Standard
requirements.** They do not establish a prohibition against the Owner
modifying or removing permitted resources.

## Remaining work

- Convert **all** user-facing resources in OSTen, System, Finder,
  Resource Editor and other native components to editable definitions.
- Implement stable resource identities, documented type schemas, and
  application-facing APIs without per-application editor hardcoding.
- Implement dedicated graphical editors for menus, dialogs and controls,
  fonts, sounds, native vector artwork and future resource types.
- Expose the Resource Editor's own complete interface through the same
  resource model; ordinary self-editing must be possible.
- Ensure global menu commands and keyboard shortcuts remain consistent
  with OSTen interface conventions.
- Validate runtime resource loading, unknown data handling, file
  permissions, and accurate scoped writes without imposing forced
  Owner-protection or recovery policies.
- Run manual, booted-OS interaction tests in addition to compilation and
  QEMU smoke tests.

The active implementation resides in draft pull request
[#1](https://github.com/Drmgiver/OSTen/pull/1).
**Do not merge into `osten/main` before Owner approval.**

## Native compliance audit (October 21, 2026)

The source-level compliance audit is recorded in
[NativeComplianceAudit.md](NativeComplianceAudit.md). Resource Editor,
Finder and System remain **partially compliant**—the current alpha
does not meet the universal requirement.

The latest revision makes Resource Editor's browser appearance,
paint swatches, and selected Finder/System dialog text editable
through their respective manifests. Multi-line UI strings may be
encoded as `\\n`, tabs as `\\t`, and literal backslashes as `\\\\`
in a one-line manifest. Resource writes no longer depend on a
successful automatic backup. Backups are opt-in and Owner-managed.
