# OSTen artwork integration

The selected graphics are **Folder B**, **Finder C**, and **System B**.
Artwork is embedded as 40x40 indexed RGBA data in `IconArtwork.h` and
rendered with alpha transparency by the native OSTen Finder. This avoids
fragile absolute asset paths in the bootable image.

- Folder: shown for every directory in OSTen Finder, including System Folder.
- Finder and System: shown for the two actual files inside `/boot/System Folder`.
- Other files retain the neutral paper icon.
- Icon placement, hit testing, launching, dragging, and file operations are
  unchanged. The existing New Folder smoke check now expects blue rather than
  yellow pixels.

These are rasterized versions of the previously approved artwork, scaled down
to the Finder's small icon size; no artwork was regenerated for this commit.
The changes do not yet register application icons for Haiku Tracker or other
third-party applications. The approved baseline remains Run 36 until the next
full OSTen build and QEMU smoke verification completes.
