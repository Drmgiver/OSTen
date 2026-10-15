# OSTen application tray (preview protocol)

OSTen's right-hand menu bar has the order **Application Tray → Clock → Application Switcher**.

This first-pass tray is opt-in. It does not automatically enumerate all Haiku Deskbar replicants: Deskbar's BShelf lives in a separate process. Third-party developers may send messages to the OSTen System application using the protocol below.

```cpp
BMessenger tray("application/x-vnd.OSTen-System");
BMessage registerIcon('osir');
registerIcon.AddString("signature", "application/x-vnd.example-app");
registerIcon.AddString("label", "Example app");
registerIcon.AddInt32("action", 'exac');
tray.SendMessage(&registerIcon);

// On quit:
BMessage removeIcon('osix');
removeIcon.AddString("signature", "application/x-vnd.example-app");
tray.SendMessage(&removeIcon);
```

The tray uses the application's registered mini icon from the MIME database and sends the configured action message to that application's signature on click. An unregistered application does not appear. The initial presentation supports four icons in a 96-pixel area. It is not a drop-in replacement for native Deskbar replicant hosting, and dynamic resizing, per-application icon updates, context menus, and dead-app cleanup remain to be implemented.

OSTen-native volume, display, network, power, Bluetooth, drives, and settings controls belong in the Control Strip, not this tray.

The old Notices click target has been removed from the panel. Existing notification handling remains in the source pending integration into a separate notification feature.

Build and test on the Haiku target; the GitHub source change has not been compiled in this environment.
