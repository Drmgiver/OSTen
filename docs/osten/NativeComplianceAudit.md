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
  MacDecorator retain non-resource-defined drawing and behavior.
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
