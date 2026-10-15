# OSTen application tray: native Deskbar integration preview

Source branch: `osten/main`.

## Implemented source changes

The **existing Haiku Deskbar process remains the native BShelf host**. OSTen
System sends an `osht` message to Deskbar at startup and periodically.
Deskbar moves its **actual interactive Replicant tray** to a small borderless
floating window over OSTen's reserved tray slot. It retains the established
`BDeskbar::AddItem()`, removal, loading, and native view behavior. The
proprietary `osir` / `osix` API from the first attempt was removed.

The panel order is **Application Tray → Clock → Application Switcher**.
The OSTen Control Strip is responsible for volume, display, network,
power, Bluetooth, drives, and settings.

## Outstanding issues — not a finished or verified integration

- This change has **not been built or boot-tested**. Verify Deskbar's window
  look/feel, native input, view reparenting, and desktop startup timing.
- The tray slot is temporarily fixed at 96 pixels. Overflow, resizing and
  screen-resolution changes require testing and further work.
- **Filtering native system-setting Replicants is NOT YET IMPLEMENTED.**
  They may appear in the tray if installed in Deskbar. Do not claim the
  Control Strip separation is completed. Implement identity-based exclusion
  with an explicit rule for third-party Replicants; do not filter generic
  names such as "Network Monitor".
- Empty-state visual behavior and persistence need verification.
- While in OSTen-host mode, Deskbar's original menus are hidden intentionally;
  ensure no background features depend on their visibility.

## Required checks

1. Compile Deskbar and OSTenSystem using the normal OSTen Haiku image build.
2. Open a fresh boot and verify the three controls' position and hit testing.
3. Install an unchanged third-party Deskbar Replicant and exercise its native
   context menu and behavior. Remove and re-add it.
4. Check with multiple icons, a restart, a screen-size change, and no icons.
5. Check native PowerStatus, NetworkStatus, Bluetooth, etc. Do not sign off
   until system settings are only presented in Control Strip.
6. Fix any regressions before calling this production-ready.

Commit history records the implementation, but no successful build evidence
has yet been attached.
