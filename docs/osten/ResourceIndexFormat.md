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
