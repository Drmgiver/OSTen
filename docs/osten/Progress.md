<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# OSTen development handoff

Updated October 3, 2026 after native build, boot, and transfer verification.
This file is the durable resume point after a lost conversation or session.

## Repository and product decisions

- Repository: https://github.com/Drmgiver/OSTen
- Published branch: `osten/main`. Fetch it before starting the next increment.
- The user gave standing authorization for OSTen development on October 3,
  including commits, publication here, builds, and verification.
- [Project.md](Project.md) remains the product charter: evolve System 8 for
  modern hardware, with ordinary two-button/right-click mouse behavior.
- Finder owns the spatial desktop; System owns the permanent global menu bar.
  Keep the visible System Folder and manually placed application folders.

## Verified milestone

The recovered branch was `55a1ff1565cd0fea15ea77844594dc1ed488be68`, with
context menus, dragging to move, Option-drag to copy, Trash transfers, and
Finder activation fixes already committed. Do not recreate that implementation.

Runtime commit `16853e36c4394df8aa3d2bdca38e5a5470e5a6ed` adds the reviewed
filesystem operations and 16 native regression cases. Its
[image build](https://github.com/Drmgiver/OSTen/actions/runs/37152078823)
compiled and linked successfully. The combined run later failed in the initial
test harness, not in the compiler: `rg` was absent and UI navigation was wrong.

[Verification run 37154388214](https://github.com/Drmgiver/OSTen/actions/runs/37154388214)
passed on verification commit `f0920877523bac23c556a66855787573aafd7ea6`.
It reused the built image only after comparing runtime sources with the later
commit. Those later changes affected documentation, workflows, and input helpers.
All 16 native cases passed. The desktop, spatial Finder window, right-click menu,
Applications window, and native PASS dialog were visually inspected.

[Committed evidence](evidence/README.md) includes screenshots, native test output,
and the exact build/verification manifest. Source, evidence, and this handoff
remain available after the short-lived Actions image artifact expires.

The stock fallback was also rebuilt and visibly booted:
[baseline image 37152227565](https://github.com/Drmgiver/OSTen/actions/runs/37152227565)
and [baseline boot 37152227519](https://github.com/Drmgiver/OSTen/actions/runs/37152227519),
from commit `38294663ed4a5bf165636a38ecd1ada980bfe56b`.

## Filesystem changes

- Stage complete copies on the destination volume before replacing an item.
- Keep the previous item recoverable until publication succeeds; restore it
  if publication fails and report any leftover visible recovery folder.
- Preserve node identity for same-volume moves. Remove cross-volume sources
  only after publishing the complete copy.
- Reject a folder's self/descendant targets and replacing an ancestor that
  contains the source. Resolve target directories through BDirectory so
  directory aliases cannot bypass those guards.
- Cancel pending drag tracking before refreshing the view's item list.

Native cases cover moves, copied attributes, duplicates, same-folder moves,
Trash collisions, file/directory replacement, inode identity, failed-copy
preservation, self/descendant/ancestor guards, aliased targets, dangling links,
missing sources, and sibling path prefixes.

## Build and verification workflow

The `OSTEN_TRANSFER_TESTS=1` image flag includes a visible test application in
Applications. The preview workflow enables it. Native output reaches QEMU's
serial log through Haiku debug output; require `OSTEN_TRANSFER_TESTS_PASS 16`.

`osten-verify-preview.yml` can retest a saved image when only documentation,
workflows, or boot input helpers changed. It rejects reuse if runtime files
changed or no suitable artifact exists. This avoids recompiling the toolchain
for input-harness corrections. The full preview workflow builds runtime changes.

The baseline workflows now build and download an image for the same source
commit whenever the baseline boot check changes, avoiding expired old artifacts.

## Exact next step

The frozen filesystem review/build/boot increment is complete. Continue with
Finder interaction polish, then System Folder protection:

1. Repair global Command-O/New Folder/Close/Select All shortcuts when a Finder
   folder owns keyboard focus. Initial Command-O opens the root, but subsequent
   focused-folder Command-O did not open its selection in QEMU. Enter did.
2. Hide the implementation folder `_packages_` in the boot-volume view. The
   current hide list checks `_packages` instead. Update the input harness if
   icon positions change: it currently clicks Applications at (215, 95).
3. Add explicit Owner unlock semantics for System Folder changes, and verify
   those boundaries in a bootable increment.

Remaining limits: cross-volume moves have implementation/API review but no
native two-volume regression case yet. Power-loss durability, concurrent
external mutations, and rollback failure injection remain unverified. Transfers
run on the view thread, so a large copy can stall its window. Metadata/archive
conventions, the Toolbox, and the rest of the approved Classic Mac model remain
future work rather than completed features of this milestone.
