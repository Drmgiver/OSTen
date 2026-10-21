# OSTen Resource Editor
**Development preview, Software Standard migration**

The native Resource Editor works directly with the same editable interface
resources used by System, Finder, MacDecorator, and Resource Editor itself.
It does not patch executable code.

## Where to find it

The OSTen image stages **Resource Editor** in the visible
`/boot/Applications/` directory.

The interface catalog files live here:

- `/boot/System Folder/Resources/System/Interface.ostr`
- `/boot/System Folder/Resources/Finder/Interface.ostr`
- `/boot/System Folder/Resources/ResourceEditor/Interface.ostr`

**The new browser starts with an Open dialog**, then opens a
resource-type window for the chosen OSTen application or `.ostr` file.
Double-click a type (such as `ICON`, `MENU`, `RECT`, or `TEXT`)
to open a window listing individual resources of that type. Double-click an
entry to edit it in a separate window.

`ICON` resources now have a **real pixel editor** with a magnified grid,
color swatches, a transparent eraser, direct mouse painting, and
Save/Revert/Undo Save. The six original approved Finder images (disk,
empty/full Trash, folder, Finder, System) are bundled as native `.rgba`
resource files and can be edited without first importing replacements.
Changes to the installed artwork still require Finder's System Folder
unlock. Restart Finder after saving to load new pixels.

For the first milestone, other resources use the existing property editor
as a **temporary inspector**. The browser discovers them from the opened
`.ostr` rather than limiting them to hardcoded applications.
Types are presently inferred from key/value conventions and image files:
the `.ostr` format does not yet carry stable numeric resource IDs or rich
resource schemas. Opening arbitrary compiled Haiku binaries is not supported.

## Basic editing

1. Open Resource Editor from Applications; select Finder, System, Resource
   Editor, or an OSTen `.ostr` document.
2. In the resource-type window, double-click `ICON` to browse artwork
   thumbnails, then double-click `trash-empty` or `trash-full`.
3. Paint directly in the magnified image, selecting a color or transparent
   eraser from the palette. Select **Save**; **Revert** discards changes
   not yet saved, and **Undo Save** restores the previous file from `.bak`.
4. For `MENU`, `COLR`, `RECT`, `TEXT`, etc., double-click an entry to
   open the interim resource property inspector. Edit, Apply, and Save.
5. Use **Refresh** in the type browser after adding or changing resources.
   Restart the edited application to see changes.

## What can be changed today

- **Menus:** Select a `menu.*` order list. Use **Up** or **Down** in the
  command list to rearrange menu entries; select **Apply** then **Save**.
  Menu actions use stable IDs, so reordering never rewrites executable logic.
- **Shortcuts:** Open Finder's `shortcut.*` resources and edit the
  desired letter. The System menu and Finder use the same mapping after
  both applications restart. Invalid shortcuts fall back to compiled defaults.
- **Rectangles:** A setting in `left,top,right,bottom` format appears in
  the graphical preview. Drag to reposition it, or edit all four values
  directly to resize it. Apply and save the new value.
- **Colors:** `R,G,B` properties use decimal components from 0 through 255.
  Examples include Finder icon labels, list-view highlights, Control Strip
  backgrounds and the Platinum window decorator palette.
- **Fonts:** Font resources accept the optional `.family`, `.style`, and
  `.size` suffixes. Unsupported families fall back to native fonts.
- **Artwork:** When the installed System or Finder catalog is open, choose
  a known artwork asset from **Artwork** and use **Import...** to select
  a compatible image, such as a PNG. The editor loads it through
  Haiku's image translators, checks the dimensions and pixel format,
  and writes an exact-size `.rgba` replacement. The approved embedded
  artwork remains the application fallback.

Currently supported artwork includes the Finder folder, Finder and System
icons (40x40), disk and empty/full Trash (48x48), OSTen menu wordmark
(71x20), About wordmark (248x69), and Finder application-switcher art (22x22).

## Recovery

- **Revert** discards *unsaved* manifest changes and reloads the current file.
- **Undo Save** swaps the saved manifest with its prior `.bak` copy.
- **Undo Art** restores the previously imported artwork for the selected asset.
- Invalid resource dimensions or numeric values should fall back to safe
  in-application defaults. Missing artwork files also fall back to approved
  source-compiled art.
- System Folder Access locks again when Finder restarts.

## Limits before full Software Standard compliance

This is the first functional editor, **not yet the final classic ResEdit-level
visual editing environment**. It can drag rectangle resources and reorder
menu/module lists, but does not yet provide a complete visual hierarchy of
every control in every dialog. Some specialized UI geometry, window-control
drawing details, font settings, and application behavior are still implemented
in code. The editor cannot safely invent new executable command handlers.

Resource changes are loaded at startup instead of live. Schema versioning,
transactional multi-resource changes, collision testing for all shortcuts,
and automated booted-OS interaction tests are also still needed.

The definition of OSTen-native compliance remains in
[SoftwareStandard.md](SoftwareStandard.md).

## Owner-directed saving

ResEdit saves the selected resource directly and atomically by default.
Automatic backup creation is **not required**. To opt into `.bak`
copies, edit `browser.pixel.keep_backup=1` and
`editor.keep_backup=1` in the Resource Editor's own catalog, then
restart Resource Editor. Failed backup creation does not block an
otherwise permitted edit. Existing Undo Save/Undo Art only work where
a backup file actually exists. Make a working copy first if you want
to keep the original unchanged.

The current implementation still requires complete resource-ID, schema,
menu/dialog and global-menu work before it can claim full compliance.
See [NativeComplianceAudit.md](NativeComplianceAudit.md).
