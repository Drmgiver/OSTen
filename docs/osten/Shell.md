# OSTen desktop shell milestone

Commit `72baeb310e5540aa7b940f5c1e8b7c941b09236f` replaced the transitional
Haiku desktop session with OSTen's first native shell. GitHub Actions run
[35764345932](https://github.com/Drmgiver/OSTen/actions/runs/35764345932)
built `nightly-osten-raw`, saved the raw preview image, booted it in QEMU, and
completed successfully on September 22, 2026.

The captured desktop showed the OSTen global menu bar, an OSTen boot-disk icon,
and Trash. It did not show Tracker's desktop icons or windows, or Deskbar's
clock and application list. This verifies that OSTen Finder owns the desktop
and that the OSTen System and Finder services replace Tracker and Deskbar when
their visible files are present in `System Folder`.

This is deliberately an early shell. Disk and Trash icons are functional
double-click targets, and folder navigation opens separate windows. Classic
icon views, full Finder menus, file manipulation, desktop persistence, and the
remaining System 8 interaction model are subsequent milestones.
