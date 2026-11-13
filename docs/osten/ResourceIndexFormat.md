# OSTen Resource Index v1
**Public developer specification — introduced October 21, 2026**

The **OSTen Software Standard** requires native applications and OS
components to expose their user-facing, non-executable resources.
`ResourceIndex.osti` is a first, deliberately simple, versioned catalog
supporting stable identities and typed discovery. The index supplements
`Interface.ostr` and raw graphics; it does **not** yet implement the
full universal binary resource container or runtime widget/action schema.

## Native application directory

A self-contained native application can ship:

```text
My Application/
  My Application                 (native executable)
  Resources/
    Interface.ostr               (editable resource values)
    ResourceIndex.osti           (typed index with stable IDs)
    example.rgba                 (optional artwork)
```

The public C++ `OSTenResourceCatalog` constructor accepts
`OSTenResourceCatalog("MyApplication", "/absolute/path/to/My Application")`
to load application-local resources without modifying Resource Editor's
source. The owner may open the application's folder or executable in
Resource Editor. Operating-system components can continue using
`/boot/System Folder/Resources/<Component>/` until packaging migrates.

## Index file encoding

UTF-8 text with LF line endings. The first line is exactly
`OSTEN-RESOURCE-INDEX/1`. Remaining noncomment lines use six
pipe-delimited columns:

```text
ID|TYPE|KEY|DISPLAY NAME|WIDTH|HEIGHT
0x72a963d4|MENU|menu.file|File menu|0|0
0x4d33ae79|ICON|app-icon|Application icon|48|48
```

The first row after the header above is a documentation heading,
not part of the file. Valid data types use `ID` as a unique unsigned
32-bit integer written with `0x` hexadecimal notation. `TYPE` is an
extensible mnemonic such as `MENU`, `ICON`, `COLR`, `WLAY`,
`DLOG`, `FONT`, `KEYS`, `NTFY`, `CTRL`, `INFO` or `TEXT`.
`KEY` is an ASCII resource key consisting of letters, digits, periods,
underscores and hyphens; duplicate keys or IDs are invalid.
`DISPLAY NAME` is editable UTF-8, but currently cannot contain the pipe
delimiter. `WIDTH` and `HEIGHT` are nonnegative decimal integers;
they must specify the true pixel size for raw RGBA assets, and should
be zero for non-image resources.

An indexed `KEY` matches a key in `Interface.ostr` or, for image
resources, the basename of a sibling `.rgba` file. A catalog can
have resource types that the editor has not implemented graphically.
The correct fallback is an explicit generic inspector, not an invented
graphical editor or executable behavior.

The `OSTenResourceIndex.h` reader rejects malformed records,
duplicate IDs, out-of-range dimensions and unsafe keys. An older
unindexed `.ostr` remains browsable through type inference; its
fallback ID uses FNV-1a on `<Application>:<resource-key>`, which is
deterministic but not as stable as a developer-maintained explicit ID.

## Values and save semantics

`Interface.ostr` remains a line-oriented UTF-8 key/value file. An
escaped newline is `\\n`, a tab is `\\t`, and a literal backslash
is `\\\\`. Resources are loaded at app startup; app restarts are
required to observe edits in the current preview.

Every resource editor operation should write the intended value or
artwork without changing unrelated resources or executable files.
The Owner may choose to work on a copy; automatic backups and recovery
are **not required** and do not determine permission to edit.

## Limitations and next revision

Version 1 indexes type, identity, display label and raw image dimensions.
It does not yet define field-level type schemas, named nested controls,
dialog composition, executable action registration, sound encoding,
vector/HVIF formats, locale bundles, drag/drop contracts, multi-resource
transactions or system-wide active-application menu publication. These
must be designed before calling the three bundled native components
fully OSTen Software Standard compliant.

See [NativeComplianceAudit.md](NativeComplianceAudit.md) for the current
compliance blockers.

## Native dialog and window control composition (revision 1)

The public header \`src/apps/osten/OSTenDialogLayout.h\` constructs
native BeAPI controls from the same indexed \`Interface.ostr\` resources.
An application supplies **only** a stable action ID to \`BMessage\`
binding table; the Owner controls appearance, order, labels, type, and
position through its resource data.

Example from Finder:

\`\`\`ini
dialog.rename.controls=name,cancel,ok
dialog.rename.control.name.type=text
dialog.rename.control.name.frame=20,27,365,57
dialog.rename.control.name.caption_key=dialog.rename.label
dialog.rename.control.name.divider=56
dialog.rename.control.cancel.type=button
dialog.rename.control.cancel.frame=185,100,268,129
dialog.rename.control.cancel.caption_key=dialog.rename.cancel
dialog.rename.control.cancel.action=rename.cancel
dialog.rename.control.ok.type=button
dialog.rename.control.ok.frame=278,100,370,129
dialog.rename.control.ok.caption_key=dialog.rename.ok
dialog.rename.control.ok.action=rename.accept
dialog.rename.control.ok.default=1
\`\`\`

The controlling \`*.controls\` value is a comma-separated ordered list
of stable control IDs. Each \`*.control.<id>\` structure defines \`type\`,
\`frame\` (left,top,right,bottom), optional \`caption_key\`, \`divider\`,
\`background\` and \`default\`, and an action ID used for binding.
Type v1 supports \`text\` (BTextControl), \`button\` (BButton),
\`label\` (BStringView), and \`list\` (BListView with a scroll view).
The builder also returns a map of control IDs to instantiated native
views so application behavior remains separate from presentation.

The Finder Rename and System Notification Center windows are the first
runtime consumers of this common builder. Their appearance declarations
are in their respective \`Interface.ostr\` files, with matching entries
in \`ResourceIndex.osti\`. The Resource Editor recognizes any indexed
\`DLOG\`/\`NTFY\` resource whose key ends in \`.controls\` and opens a
drag-and-resize layout editor. Saving rewrites all modified \`*.frame\`
values in one atomic file replacement without changing unrelated data
or source code.

This is **not** yet a universal command system: arbitrary action strings
must be implemented and registered by their respective applications.
The UI editor does not create executable behavior. The first builder
also does not support tab controls, hierarchical widgets, sound assets,
layout constraints, live reload, or every Haiku native control type.

### Reusing an existing frame resource

A control declaration may provide \`frame_key\` instead of \`frame\`.
The value identifies an existing frame resource, which remains the
single source of truth for layout geometry. This supports migration of
existing OSTen interfaces without duplicating coordinate data:

\`\`\`ini
browser.types.controls=open,refresh,current,hint,resource_types
browser.types.control.open.type=button
browser.types.control.open.frame_key=browser.types.open_frame
browser.types.control.open.caption_key=browser.types.open
browser.types.control.open.action=browser.open
\`\`\`

Resource Editor's type-browser **and** resource-list windows are built
through this same shared runtime composer. Their \`*.controls\` resources
open in ResEdit's graphical layout editor, which saves the referenced
frame values rather than introducing copies. These resources let the
Owner redesign the browser with ResEdit itself.

## Active-application global menus (revision 1)

The public \`OSTenGlobalMenu.h\` defines \`kOSTenGlobalMenuQuery\`,
\`kOSTenGlobalMenuInvoke\`, and \`OSTenAnswerGlobalMenuQuery\`. An
OSTen-native \`BApplication\` implements the query and invoke cases
in its \`MessageReceived\` handler. It **publishes resources, not code**.
The system bar discovers the active application through the roster,
asks it for its menu model, creates ordinary native \`BMenu\` items,
and dispatches an invoked stable action ID back to that same app.

The application defines the resource-backed menu layout:

\`\`\`ini
menu.order=file
menu.title.file=File
menu.file=open,refresh,separator,quit
menu.item.open=Open...
menu.item.refresh=Refresh Resources
menu.item.quit=Quit Resource Editor
shortcut.open=O
shortcut.refresh=R
shortcut.quit=Q
\`\`\`

Sections and command order, names, and shortcuts are owner-editable.
An application's handler registers the **meaning** of each known
action ID; changing its label or moving it does not change behavior.
Unrecognized IDs do not acquire executable behavior.

System keeps the OS menu; an active app supplies its own remaining
menus. Legacy Haiku apps without the protocol use the existing
Finder-oriented compatibility menus. The current implementation
rechecks the active application on System's periodic menu-bar tick;
native apps must restart to reload edited resource manifests.
The protocol uses \`BMessage\` as a typed data transport, not binary
patching or command execution.

**Current limitations:** menu publication has been implemented for
Finder and ResEdit, not every OSTen-native or legacy application.
Shortcut collisions across applications, menu activation focus and
live mutation still require comprehensive booted GUI tests.

## Finder Get Info: resource-driven text area

The universal control builder now supports \`text_area\` using Haiku's
read-only, selectable \`BTextView\` and a native scroll view. The
\`dialog.info.controls\` layout in Finder contains a dynamically
populated details pane and a Close button, both owner-editable through
ResEdit. Information content (file name, path, size, modification date)
comes from Finder's runtime file inspection; labels, window frame,
text-area colors, controls and their positions come from resources.

\`\`\`ini
dialog.info.controls=details,close
dialog.info.control.details.type=text_area
dialog.info.control.details.frame=15,15,488,314
dialog.info.control.close.type=button
dialog.info.control.close.action=info.close
\`\`\`

No source-code changes are required to resize, reorder or relabel
these controls. New control *behavior* still requires a native handler.

## Indexed artwork importing for any native application

ResEdit's property inspector no longer contains a fixed System/Finder
image table. It reads each selected application's sibling
`ResourceIndex.osti`, discovers `ICON` and `IMAG` entries,
verifies their stated dimensions against the raw `.rgba` file, and
offers those resources for image import and optional undo. System
Folder Access is checked **only** when the actual target is inside
`/boot/System Folder/`; a standalone native application's own files
continue to use normal filesystem permissions.

The OSTen image now includes the exact compiled source artwork as
editable `System/wordmark.rgba` (71×20),
`System/about-wordmark.rgba` (248×69) and
`System/switcher-finder.rgba` (22×22). All three are indexed and
installed without changing their original pixels. Compiled fallback
art remains available if an Owner removes an asset.

## Runtime-composed dialog controls (November 1, 2026)

A dialog's `<prefix>.controls` entry is a comma-separated ordered list
of stable IDs. Each control is declared by the keys
`<prefix>.control.<id>.type`, `.frame` or `.frame_key`, optional
`.caption_key`, optional `.action` and type-specific attributes.

Built-in types: `text`, `text_area`, `label`, `button`, `list`,
`checkbox` and `slider`. For `checkbox`, `.checked` specifies the
initial state, 0 or 1. For `slider`, `.minimum`, `.maximum`, and
`.value` specify the initial numeric range and value. Dialog code
passes only known `action ID → message` bindings. Resource authors
can select registered handlers but cannot create executable commands
by changing a resource.

`OSTenDialogControlFactories` can supply application-specific native
view constructors keyed by type without changing the common loader.

The graphical dialog editor can create and reorder controls. It writes
new stable IDs to the index, preserving the current application ID
namespace. `Interface.ostr` and `ResourceIndex.osti` are independently
atomic files; a failure after saving the manifest is reported rather
than falsely claiming the whole operation succeeded. A future
transactional resource container should eliminate this gap.
