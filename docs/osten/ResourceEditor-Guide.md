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

The editor offers **System**, **Finder** and **Resource Editor** modes.
The System catalog also contains the Platinum/MacDecorator entries.

## Basic editing

1. In Finder, choose **Special > System Folder Access...** and unlock
   the System Folder. This is the existing Finder-level Owner guard.
2. Open **Applications > Resource Editor**.
3. Select System, Finder or Resource Editor and choose a resource in the list.
4. Edit the resource's **Key** or **Value**, then select **Apply**.
5. Choose **Save**. The editor writes a new manifest and backs up the
   previous version as `Interface.ostr.bak`.
6. Restart the edited application to reload its resources. Decorator changes
   may require restarting the graphical server or rebooting OSTen.

The editor checks Finder's current unlock state before writing to the
protected System Folder. This is an editing workflow, **not a filesystem
permission boundary**.

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
- **Artwork:** Choose an artwork asset from **Artwork** and use **Import...**
  to select a compatible image, such as a PNG. The editor loads it through
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
