# OSTen development handoff

Updated October 3, 2026. This file records verified repository state and open
work so development can resume after a lost conversation or execution session.

## Repository and established product decisions

- Repository: https://github.com/Drmgiver/OSTen
- Working branch: `osten/main`; upstream Haiku remains the foundation.
- Product charter: [Project.md](Project.md). System 8 behavior evolved for
  modern hardware, with a normal two-button mouse and right-click menus.
- Finder owns the spatial desktop; System owns the permanent global menu bar.
- No Dock or ordinary Unix user workflow. The visible System Folder and
  manually placed application folders remain the intended system model.

## Last verified published state

Commit `55a1ff1565cd0fea15ea77844594dc1ed488be68` is the recovered branch head.
It includes context menus, dragging to move, Option-drag to copy, moving to
Trash, and Finder activation/Command-O fixes.

The [preview build and boot run](https://github.com/Drmgiver/OSTen/actions/runs/35808195992)
completed successfully on September 23. This run built the preview and passed
its existing Finder-window image check. The October 3 recovery did not inspect
that run's expired screenshot artifacts or repeat its boot, so this is a CI
result rather than a new visual verification.

## Filesystem review and current changes

The recovered transfer implementation removed the destination before copying.
The current changes stage copies on the destination volume, keep an existing
item recoverable until publication succeeds, restore it on publication failure,
and delete cross-volume sources only after publishing the complete copy.
Same-volume moves preserve node identity. Any leftover recovery folder is
visible and named in the error message.

The implementation rejects copying/moving a folder into itself or a descendant
and replacing an ancestor containing the source. Target paths are resolved
through BDirectory so directory aliases cannot bypass containment checks.
Refreshing a view cancels pending drag tracking before rebuilding its item list.

The optional `OSTEN_TRANSFER_TESTS=1` image flag places a native test application
in Applications. The preview workflow enables it and runs 16 regression cases,
exporting their results through Haiku debug output to the QEMU serial log.
The input helper uses an emulated USB tablet with absolute coordinates and saves
right-click menu and native test-result screenshots.

Local validation: changed C++ sources passed a syntax/API check against this
checkout's Haiku headers using host compatibility types. The shell helper
passed `bash -n`; the Python helper passed bytecode compilation. These checks
do not establish a target link, native test pass, image build, or boot.

## Exact resume point

On October 3 the user gave standing authorization for OSTen development,
including commits, publication to this repository, builds, and verification.
The current changes are prepared for publication and native validation.
Do not recreate the original context-menu or drag implementation.

Continue with:

1. Push the prepared local commits to `osten/main` without force.
2. Inspect the new OSTen preview workflow. Fix compilation or boot-input failures.
3. Require `OSTEN_TRANSFER_TESTS_PASS 16` in the serial log and inspect the
   desktop, Finder, context menu, and test-result screenshots.
4. Record the tested commit and workflow URL here. Preserve inspected evidence
   in the repository before its short-lived Actions artifact expires.

Remaining limits: cross-volume moves have implementation/API review but no
native two-volume regression case yet. Power-loss durability and rollback
failure injection are unverified. Transfers currently run on the view thread,
so a large copy can stall its window. Owner unlock semantics for the System
Folder and metadata/archive conventions are subsequent work, not completed
features of this increment.
