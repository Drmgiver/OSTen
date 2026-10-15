# OSTen application tray — native Replicant integration

## Required end state

The menu bar order, left to right, is **Application Tray → Clock → Application Switcher**.

The application tray contains externally supplied application UI only. OSTen's
Volume, Display, Network, Power, Bluetooth, Drives, and Settings belong in the
**Control Strip**, not the application tray. A tray with no app Replicants
should consume no icon slots. Never show a generic "Notices" launcher in it.

## Current implementation status

The preview OSTen System window in `src/apps/osten/system/System.cpp`
has a limited OSTen-specific registration protocol (`osir` / `osix`).
**This is not native Deskbar Replicant compatibility.** It displays MIME mini-icons
and forwards one click action; existing apps that use `BDeskbar::AddItem()`
do not reach it. Its four-icon cap, static allocation, and lack of liveness
tracking are temporary preview limitations. Do not describe this as a
complete system tray.

## Haiku integration facts

- Native Replicants are actual `BView` instances hosted in a `BShelf`.
- Existing applications target the Deskbar API, serviced by the **Deskbar
  process**, including its `TReplicantTray` and `TReplicantShelf` in
  `src/apps/deskbar/StatusView.*` and `StatusViewShelf.*`.
- OSTen System is a **different process**. Reading the running-app roster,
  copying icons, or drawing a `BView` of the same name does not transfer
  ownership or native input handling of a Deskbar Replicant.
- A production integration must arrange for the native registration endpoint
  and the displayed `BShelf` to agree. Do not simultaneously host the same
  add-on in Deskbar and System.

## Integration sequence

1. Establish the actual lifecycle of Deskbar in the OSTen image (whether
   replaced, launched hidden, or still running). Confirm boot scripts and
   packaging before changing it.
2. Reuse native `TReplicantTray` / `TReplicantShelf` hosting behavior,
   persistence, loading/unloading, and API registration semantics. Decide
   whether to consolidate the OSTen panel into the Deskbar process or bridge
   registered messages and instantiate views in OSTen's process. Never
   attempt to reparent live `BView` objects across processes.
3. Separate placement policy from native registration. Exclude OSTen-owned
   Control Strip components and recognized native system-setting Replicants
   by **verified component identity**, not merely by labels. Keep a documented
   exception/allow-list mechanism for third-party apps using similar names;
   arbitrary application Replicants should remain accepted. Unknown items
   must not be discarded solely because they lack OSTen metadata.
4. Make layout dynamic while reserving fixed space on the right for the clock
   and active-app switcher. Handle app removal, resize, multiple Replicants,
   loading at boot, context menus, and cleanup reliably.
5. Remove the temporary `osir` / `osix` interface once native routing has
   been verified. Existing applications must require **no OSTen-specific
   code changes**.

## Acceptance tests

- Install a third-party Deskbar Replicant using unchanged Haiku instructions:
  its **real interactive view** appears in OSTen's tray and remains operable.
- Remove and reinstall it through the native API; verify no duplicate and no
  stale slot.
- Restart the desktop with persistent add-ons and verify behavior.
- Launch an OSTen Control Strip module and confirmed system-setting Replicants;
  neither duplicates volume/display/network/power/Bluetooth/drives/settings
  in the application tray.
- Verify a third-party item with a name similar to a system setting is not
  erroneously suppressed.
- With zero application Replicants verify the visual layout remains
  tray → clock → app switcher and the empty tray has no placeholder.
- Confirm that this native implementation has been compiled and tested
  **before marking compatibility complete**.
