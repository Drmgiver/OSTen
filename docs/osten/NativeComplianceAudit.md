# OSTen Native Application Compliance Audit
**Code inspection: October 21, 2026 | Target: OSTen Software Standard draft 0.2**

This is a **source-level audit**, not a certification. ResEdit, Finder and
System are **not yet fully compliant**. The last passing preview build (#39686115116) validates the earlier
resource-externalization fixes, not the additional typed index and
specialized editors staged for the next build. Only an updated
build and hands-on testing can establish their runtime behavior.

## Scope and evidence

- Standard: `docs/osten/SoftwareStandard.md`
- Native resource API: `src/apps/osten/OSTenResources.h`
- ResEdit: `src/apps/osten/resedit/ResourceEditor.cpp`,
  `ResourceBrowser.cpp`, `data/osten/resources/ResourceEditor/Interface.ostr`
- Finder: `src/apps/osten/finder/Finder.cpp`, `FinderActions.cpp`,
  `FileOperations.cpp`, `data/osten/resources/Finder/Interface.ostr`
- System: `src/apps/osten/system/System.cpp`, `SystemExtras.cpp`,
  `ControlStrip.cpp`, `AboutComputer.cpp`, plus window decoration code
  and `data/osten/resources/System/Interface.ostr`

## Section-by-section findings

| Requirement | Resource Editor | Finder | System |
| --- | --- | --- | --- |
| 1. User ownership and manageability | **Partial** — self-editing catalog, but incomplete UI exposure | **Partial** — visible resource catalog, code-defined components remain | **Partial** — catalog covers many elements, not the whole OS |
| 2. Universal, extensible editable resources | **Partial** — stable typed IDs now present; widget schemas and many graphical editors absent | **Fail** — icons and many keys editable, not all dialogs, controls or metadata | **Fail** — missing complete menus/notifications/decorator structures |
| 3. Self-contained visible application structure | **Partial** — executable in Applications; resource catalog in System Folder, not an application folder | **Partial** — system component by design, resources separately staged | **Partial** — OS component identifiable, resources separately staged |
| 4. Native interface conventions | **Partial** — native BWindow/BFilePanel but no per-app global-menu integration | **Partial** — Finder conventions work, shortcut collision validation incomplete | **Partial** — global menu targets Finder commands, no general active-app negotiation |
| 5. Owner control and transparent changes | **Partial** — Owner check, source-data edits, optional backups enabled by explicit setting | **Partial** — Owner lock applies, some operations still code-defined | **Partial** — menu/strip edits supported, some controls aren't exposed |
| 6. Legacy Haiku compatibility boundary | **Pass in scope** — unsupported binaries rejected rather than treated as native | **Pass in scope** — native Finder, compatibility not conflated | **Pass in scope** — system component, Haiku compatibility distinct |
| 7. Public formats and future extensibility | **Partial** — versioned typed index exists; field schemas and many editors absent | **Fail** — relies on application-specific keys and fixed asset dimensions | **Fail** — application-specific keys and partially hardcoded controls |

## Corrections included in the accompanying source change

1. **Resource Editor:** Migrate its new type browser, resource-list and
   pixel-editor window bounds, principal captions, backgrounds, selection
   colors and editable paint swatches into its own visible `Interface.ostr`
   catalog. Pixel swatch 00 remains an eraser; RGB swatches 01-15 are
   owner-editable.
2. **Resource Editor:** The interim property inspector exposes additional
   previously fixed labels. System and non-system documents continue to
   honor filesystem permissions and the Finder Owner workflow.
3. **Finder:** Move more error text, item metadata labels, folder naming,
   Trash confirmation text and window titles to `Interface.ostr`.
   The folder-name number suffix is constructed with a fixed printf format
   so edited labels cannot be interpreted as format directives.
4. **System:** Move notification failure and shutdown error text and
   titles into the System catalog.
5. **Shared resource reader:** Interpret documented `\\n`, `\\t` and
   `\\\\` escapes so editable one-line manifest values can represent
   multi-line dialog text without changing its display.
6. **Owner control:** Saving manifests, importing RGBA and pixel painting
   no longer *require* creating backups. Resource keys
   `editor.keep_backup` and `browser.pixel.keep_backup` default to
   `0`. Owners may set them to `1` for optional backup-on-save;
   failure to create a backup does not veto an otherwise permitted edit.
   Existing Undo/restore tools remain available when a `.bak` exists.

These changes are **necessary but insufficient** for compliance.

## Unresolved blocking work

### Resource Editor
- Typed resource IDs and a public, versioned index have now been added.
  Field-level schemas, a universal runtime registry and a complete
  application-facing API are still absent.
- All painted UI details, browser glyphs, pixel-canvas geometry, strings,
  fonts, icons, menu structures, file-panel settings, dialogs, notification
  and window behaviors are **not** resource-defined yet.
- Dedicated MENU sequence editing and RGB color editing now exist.
  Dialogs, layout controls, sounds, fonts, vector/HVIF, metadata and
  unknown typed resources still require specialized editors.
- Standard global menu integration is not implemented for ResEdit.
- The executable remains a single file under Applications, with
  its own resource catalog staged under System Folder. The shared
  reader now additionally supports self-contained third-party apps.

### Finder
- Unexposed user-facing dialog strings and some controls still exist in
  `Finder.cpp` and `FinderActions.cpp`. Resource support is not complete.
- Menus, shortcuts, and layouts have resources but require active-app
  consistency checks and full shortcut conflict validation.
- Not all Finder artwork and states are independent resources. Several
  visual fallbacks and window arrangements remain directly defined in code.

### System
- The global menu bar is Finder-command-oriented. Native applications
  cannot yet publish a complete active-app menu model through the Standard.
- SystemExtras, AboutComputer, ControlStrip, notifications, and the
  OSTenClassicDecorator retain non-resource-defined drawing and behavior.
- The resource catalog and API do not yet define fully editable menu,
  widget, sound and notification *structures* with stable IDs.

## Acceptance tests before certifying compliance

- Boot the resulting image; visually compare System, Finder, and ResEdit
  against the approved default desktop. No default artwork may change.
- Open ResEdit immediately, browse types and ICON assets, edit Trash,
  save without automatic backups, and verify Finder reloads edited pixels.
- Edit ResEdit's own type-browser title, caption, background, geometry
  and paint-palette colors; relaunch and verify the changes appear.
- Edit Finder dialogs, a folder's default name, and a shared shortcut;
  verify System and Finder show and use the same command mapping.
- Edit System notification text and an About/dialog resource;
  restart the affected component and confirm changes appear.
- Attempt to edit locked System Folder resources; verify Finder's existing
  Owner access rule. Verify an intentionally invalid but permitted
  appearance change is **not** blocked solely to protect the Owner.
- Install a third-party native OSTen application as a visible folder,
  browse all its types without editing ResEdit's source, and verify
  system-wide menu/window/shortcut integration.
- Verify stable resource IDs, public versioned schemas, full editor
  coverage, runtime error handling and cross-application conformance.

**Integration gate:** neither a green compile nor a working ICON editor
constitutes OSTen-native compliance. Keep draft PR #1 unmerged until
the Owner approves the implementation and the required tests pass.

## Additional progress in current draft

- The `ResourceIndex.osti` format and portable reader have been added;
  type, stable ID, display label and image dimensions are now indexed
  for all existing manifest keys and Finder artwork.
- The resource browser consumes these indexes rather than relying on
  hardcoded asset dimensions for OSTen's indexed resources.
- Resource Editor now has purpose-built `MENU` and `COLR` windows.
  Their visible labels and frames are Resource Editor resources, so
  basic self-customization extends to the new windows.
- A documented application-local catalog path and indexed-resource
  preflight test support future third-party native applications.
- **This does not close the audit.** Full resource schemas, dialog
  construction and editing, system-wide active-app menus, sound/HVIF
  editing, packaging and comprehensive OS resource coverage still block
  compliance.

## Resource-composed dialogs milestone (October 31, 2026)

**Implemented on the draft branch; not yet a full compliance signoff.**

- Introduced a single publicly documented \`OSTenDialogLayout.h\`
  runtime builder for resource-declared text controls, labels, buttons,
  and list views. Actions remain explicit stable IDs bound by the app.
- Converted Finder's Rename window and System's Notification Center to
  the **same runtime builder**, removing their source-defined layouts
  and visible control lists.
- Implemented a graphical editor in ResEdit for structured \`DLOG\` and
  \`NTFY\` \`.controls\` resources, with selection, dragging, resizing,
  Revert, and an atomic grouped Save preserving the target's permissions.
- Added a pre-build test that checks control ordering, types, frames,
  captions, stable index declarations, and associated action IDs.
- **Remaining compliance failures**: many Finder windows and dialogs,
  System accessories, Control Strip, global menu active-app integration,
  OS window decorator, Resource Editor's own remaining interface elements,
  complex controls, fonts, sounds, cursors/HVIF and full app packaging.
  A green CI build verifies the included test gates, not exhaustive
  conformance or hands-on GUI behavior.

Do not merge until the Owner approves and remaining standard requirements
have been independently verified.

### ResEdit self-composition extension

Resource Editor now constructs its type-browser and resource-list
windows through the shared native control factory as well.
Their controls have owner-editable type, ordering, caption reference,
action binding and frame alias metadata. The graphical layout editor
supports \`frame_key\` references to existing geometry values, avoiding
duplicate resources. This is a concrete self-editing milestone; other
ResEdit windows, advanced controls and the global menu still require
conversion.

## Active-app global menu milestone (October 31, 2026)

- System now queries the foreground application's native menu model
  using the public \`OSTenGlobalMenu.h\` protocol. It displays the
  application's own File/Edit/View/etc. sections beside the OSTen
  system menu and dispatches selected stable command IDs back to
  the publishing application.
- Finder and ResEdit publish their owner-editable menu model through
  the same API. Labels, section and item order, and shortcuts live
  in each application's \`Interface.ostr\` and \`ResourceIndex.osti\`.
- The System Control Strip now accepts owner-selected subsets and
  reordered/duplicated module IDs rather than automatically forcing
  all seven modules to reappear. No safety-mode limitation is imposed.
- New pre-build tests verify published menu models, indexed commands,
  one-character shortcut uniqueness within each native application,
  and the Control Strip's chosen module IDs.

**These changes do not certify compliance.** Dynamic menu presentation,
keyboard routing and focus transitions require interactive boot tests.
Further complete resource conversion, advanced resource editors,
self-contained native app packaging, and extensibility tests remain.

## Finder Get Info conversion (October 31, 2026)

Finder's Get Info interface has been converted from a fixed Haiku
\`BAlert\` to a native, resource-composed window with a selectable
scrolling information pane and a Close button. The dialog's window,
controls, labels, layout and colors are editable resources. The same
ResEdit graphical layout editor handles its \`dialog.info.controls\`
entry. This closes a particular Finder violation but **not** the
remaining hardcoded folder, desktop, file-operation and contextual UI,
nor the incomplete System/ResEdit coverage.

## Native artwork coverage milestone (October 31, 2026)

- ResEdit's legacy graphic importer now enumerates `ICON` and
  `IMAG` entries from any native app's standard resource index,
  instead of relying on a hardcoded nine-asset System/Finder table.
- Owner permission queries now use the actual artwork file path.
  Modifying a standalone app's resources does not incorrectly require
  the System Folder unlock.
- The three compiled OSTen System graphics were decoded byte-for-byte
  into indexed, installed, editable `.rgba` assets. No source art
  was redrawn, recolored or substituted.
- More full application resource coverage remains outstanding:
  cursors, advanced vector graphics, additional dialogs and native
  window chrome, sound formats, and extensible non-built-in modules.

This work improves standards compliance but does **not** certify the
three applications as fully compliant.

## ResEdit strings and permission-preserving saves

The legacy property editor's principal alert, status and diagnostic
strings now have explicit indexed resource entries. CI verifies that
literal `EditorResources().String("editor.status.*")` and
`editor.alert.*` references correspond to bundled editable keys.

Resource Editor's legacy manifest and image importer, and its graphical
pixel editor, now preserve the original file permission bits when
performing atomic save replacements. This preserves Owner-selected file
access settings rather than silently imposing temporary-file modes.

**Not a complete Resource Editor compliance signoff.** The remaining
custom canvas, inspector controls, advanced resource editors and
unconverted windows still need the universal runtime resource system.

## Composed dialog authoring milestone (November 1, 2026)

The graphical layout editor can now add, remove, reorder and edit
the type/caption/action of controls in resource-composed dialogs in
addition to drag/resize. Additions receive typed stable IDs in the
public resource index. The common loader accepts checkboxes, sliders,
and registered application-specific factories. Finder Rename/Get Info
and ResEdit's browser use declared types when casting returned
controls, so intentionally changing the control type doesn't cause an
unsafe C++ downcast.

**Not yet complete:** Many native windows still use direct C++ view
construction. There is no cross-file transaction for adding manifest
and index entries, nor full GUI integration tests for all edit flows.
Native System window decorations, Control Strip internals, additional
Finder controls and several special resource types remain outside the
universal composed-control path. Full-standard compliance still fails.

## Control Strip resource module IDs (November 1, 2026)

System's Control Strip no longer enforces a compiled seven-slot module
table. `strip.module.order` accepts named, repeatable resource IDs, and
the descriptor for each module is read from editable keys
`strip.module.<id>.name`, `.label`, and `.summary`. The Settings
module identity comes from `strip.module.settings_id`; it no longer
depends on its position in a fixed array. Missing descriptors display
the Owner-supplied ID and a configurable fallback summary.

This makes module presentation extensible without modifying System C++.
**It is not yet a native executable Control Strip module plugin
protocol**; dynamic module code loading, permission contracts and
lifecycle APIs still need to be implemented.

## Composed window-frame editing (November 1, 2026)

ResEdit's graphical layout canvas now outlines the selected dialog's
resource-defined **outer window frame**. Dragging the title region
repositions it; dragging the lower-right handle resizes it.
The side inspector also accepts direct left, top, right, bottom
coordinates. Edits save to the actual frame resource key for Finder's
Rename/Get Info and Resource Editor's own browser windows. For System
notifications an explicit `notification.center.window_frame_key` alias
binds the window's existing `notification.window_frame` resource.

Edits remain Owner-controlled, while the loader consumes saved geometry
on application restart. Large windows may extend beyond the current
preview canvas; scrolling and whole-window live preview are still
future improvements. Full conformance requires conversion of all
remaining native C++-defined windows.

## Legacy Resource Editor inspector composition and cross-component text (November 12, 2026)

The older Resource Editor property inspector is now instantiated through
`OSTenCreateDialogControls` rather than a fixed constructor full of
BButton/BStringView/BTextControl view layouts. Its 19 visible controls are
declared in `editor.inspector.controls` with stable resource identifiers,
types, frames, captions and action bindings. The existing artwork menu
and rectangle-preview canvas are registered custom widget factories,
available to the same loader that powers native Finder dialogs.

The Owner can now open `editor.inspector.controls` in ResEdit's layout
designer and remove, reorder, resize or replace its controls without
recompiling the inspector. Expected types are verified before casts.
If a required widget is intentionally absent, the modified window still
appears but editing operations are disabled rather than dereferencing
the missing widget. This is an implementation reliability constraint,
not a developer-enforced prohibition on changes.

Additional Finder System Folder access/error messages and inline rename
diagnostics now load from Finder resources. System's notification detail
strings, list formatting and 22 native tray diagnostics fields now
load from System resources. The catalog indexes cover these additions.
ResEdit preview instruction colors and positions are also Owner-editable.

**Remaining blockers:** universal conversion of Finder windows, System
About/Control Strip rendering and menus, ResEdit's pixel/menu/color
editors and painter internals; format-specific vector/audio/cursor
editing; cross-file atomicity between manifest/index when adding keys;
and actual interaction testing after boot. Compilation alone never
certifies the standard.

## About This Computer native control composition (November 12, 2026)

System's running-application list in About This Computer is now declared
through `about.controls` with a typed `list`, a frame-key alias to
`about.app_list_frame`, and a resource-defined background. The
`AboutComputerWindow` requests the view from the universal native
control loader instead of constructing a list and its scroll view
directly in C++. It checks the declared type before casting; removing
or retyping the list leaves the rest of the About view usable rather
than crashing.

The native-resource preflight now validates the About control schema.
The overview's custom-drawn statistics, memory visualization and
wordmark still require complete independent resource composition and
graphics editing to meet every aspect of the standard.

## Menu and RGB editing window composition (November 12, 2026)

Resource Editor's native Menu Editor and RGB Color Editor now request
their visible controls from the universal `OSTenCreateDialogControls`
loader. Their ordered control IDs, types, frames, captions and commands are
editable and indexed in Resource Editor's own public resource catalog.
The color preview uses a registered native `color_sample` factory;
RGB sliders expose their range and hash-mark styling as resource fields.
Changes to these `.controls` sequences can be made in the graphical
dialog layout designer without editing C++.

If the Owner intentionally removes or retypes a required data-entry
control, the editor's operations are disabled instead of making an unsafe
downcast. The Owner's edit itself is not prohibited or reversed.
Catalog/index preflight validates both new default dialog schemas.

**Still not full compliance:** Pixel Editor's canvas/palette drawing,
resource-type specific editors (audio, cursors, vectors, metadata), remaining
System/Finder custom UI, native executable Control Strip modules,
cross-file manifest/index transactions and live post-boot GUI tests remain.
No merge or certification before Owner approval and verification.

## OSTen Classic decorator naming (November 12, 2026)

The OSTen-native window decorator, its C++ classes, add-on path, build
target, installed image target and app_server default lookup are now named
`OSTenClassicDecorator`. The publicly displayed style is **OSTen Classic**.
Window chrome, colors and drawing behavior are unchanged. The preflight
checks that all core identity bindings agree. This rename does not confer
full standards compliance or settle any separate intellectual property issue.

## Pixel Editor native control composition (November 12, 2026)

Resource Editor's pixel-artwork window now instantiates the artwork
canvas, status line, and Save/Revert/Undo buttons from the public
`browser.pixel.controls` resource sequence. Its five controls have
indexed stable identifiers, editable types, frames and action bindings.
The `pixel_canvas` custom factory receives its current artwork data
through explicit per-composition context instead of a process-global
singleton. The same context API is available to future specialized
native resource widgets.

Owners may edit or remove any declared control. Missing/retyped canvas
or status controls disable the affected editing operations without
silently reinstating them. The graphical dialog editor also exposes
this window's outer frame through `browser.pixel.window_frame_key`.
The resource preflight validates the newly composed Pixel Editor.

**Remaining:** pixel canvas internal palette/checker geometry and line
colors, remaining custom-drawn resource types, System/Finder coverage,
application module protocols, and real booted GUI interaction testing.
This is not complete OSTen Software Standard compliance.

## Pixel Canvas palette, checker and positioning coverage (November 12, 2026)

Resource Editor's pixel-artwork painter now reads its paint origin, paint
scale limits, checkerboard colors/size, grid threshold/color, canvas border,
label positions/colors, sixteen-swatch palette origin/columns/spacing/size,
selected/normal outlines, eraser diagonal, and 1:1 preview position/scale
limits from 29 indexed owner-editable resources. The shipped defaults retain
the approved appearance. Painting, swatch highlighting and mouse selection
share resource-derived rectangles rather than having different hardcoded hit
areas, including after an Owner moves the palette. The native preflight
validates the default types, RGB triples, points, and numeric values.

These data changes do not alter image file formats or force backup/recovery.
Remaining gaps include complete import/export for other asset formats,
font/audio/cursor/vector editors, cross-file resource transactions,
additional Finder/System native UI, and booted interaction tests.
