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
