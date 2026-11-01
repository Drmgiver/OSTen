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

## Typed native resource index milestone (October 21, 2026)

A versioned, portable `ResourceIndex.osti` now accompanies each of the
three native catalogs and declares stable resource IDs, types, display names
and image dimensions. The Resource Editor reads those catalogs for the
type → instance → editor browser and displays resource IDs. Unindexed
legacy catalogs retain an explicitly limited fallback.

The first dedicated `MENU` sequence editor supports command ordering,
separators, adding, replacing, removing, saving and reverting. The
`COLR` RGB editor offers three sliders and a live preview. `ICON`
editing continues to use direct pixel painting. `DLOG`, `WLAY`,
`FONT`, sound, vector and several other types still need dedicated
graphical implementations. Their presence in the index does not mean
their editing capabilities are complete.

Native third-party applications may store their own resource catalog
and index at `<Application Folder>/Resources/`; the shared resource
reader supports an explicit application-folder constructor argument.
The core System and Finder packaging has not yet migrated from its
shared System Folder resource directory.

A fast preflight checker now checks index/manifest coverage, IDs,
dimensions and selected source bindings, and a platform-independent
C++ smoke test compiles the index reader before the expensive OS build.
Neither test proves full runtime conformance.

**Full OSTen Software Standard compliance remains unachieved.**
The unresolved blockers are documented in
[NativeComplianceAudit.md](NativeComplianceAudit.md).

## Shared native dialog composition milestone (October 31, 2026)

A new public \`OSTenDialogLayout.h\` creates native text fields,
buttons, static labels, and list views from ordered, typed control
records in each application's editable resource catalog. Finder's
Rename dialog and System's Notification Center now consume the same
resource-driven builder. ResEdit's graphical layout editor reads those
same records and lets the Owner drag and resize their frames, then
save all changes in a single atomic manifest update. The file's
permissions are preserved.

CI now validates the packaged dialog declarations before the full
image build. The general resource-index and smoke tests remain.

**This is a significant step toward—but not completion of—universal
native compliance.** Additional widget families, complete Finder and
System UI conversion, active-application global menus, application
folders, fonts/sounds/HVIF editors and interactive GUI verification
are still outstanding.

Resource Editor's browser type and instance windows now also use the
shared control composer, making the application itself a consumer of
the same structured resources. Frame aliases avoid duplicating
existing layout keys. No special safeguards restrict Owner edits.

## Active application menu protocol and Owner-owned Strip (October 31, 2026)

The public \`OSTenGlobalMenu.h\` introduces an application-to-System
native menu publication protocol. System requests the active app's
resource-backed menus, installs them in its global bar, and sends
selected action IDs back to the app. Finder and ResEdit are the first
native publishers; compatibility with existing Haiku binaries remains
separate. A fast conformance check covers declared menus, labels,
shortcuts and known command bindings.

The Control Strip accepts a resource-defined visible subset or
reordering of the built-in modules; an empty list leaves the
collapsible handle only. Previously it forcibly restored seven
module slots. Control Strip extensibility to independently installed
native modules is still unfinished.

The draft branch is **not fully compliant**. Compiling the code and
passing the automated checks is necessary, not sufficient to verify
every requirement in the standard.

The Finder Get Info display now uses a structured native window with
editable layout and a read-only text area. Its layout and control
definitions are fully indexed, with a conformance check. The existing
file-data logic remains executable code, appropriately separated from
the user-editable presentation.

System's three original wordmark/switcher images are installed as
owner-editable, indexed RGBA assets. The legacy ResEdit graphic importer
now discovers indexed assets in any native application instead of
hardcoding known Finder/System filenames. Target permissions are checked
against the real file, preserving independent application ownership.

ResEdit's previously hardcoded common warnings and status messages
are now indexed owner-editable strings. Atomic resource writes preserve
existing file permission bits rather than replacing them with new
temporary-file defaults. This is a targeted correctness improvement,
not a complete standards certification.
