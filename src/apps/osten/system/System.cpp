/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <MimeType.h>
#include <PopUpMenu.h>
#include <algorithm>
#include <MessageRunner.h>
#include <Notification.h>
#include <View.h>
#include <List.h>
#include <time.h>
#include <vector>
#include <Application.h>
#include <Bitmap.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Screen.h>
#include <Roster.h>
#include <RosterPrivate.h>
#include <String.h>
#include <string.h>
#include <Window.h>

#include "AboutComputer.h"
#include "OSTenMessages.h"
#include "SystemExtras.h"
#include "OSTenWordmark.h"


namespace {

const uint32 kAboutOSTen = 'osab';
const uint32 kFocusAbout = 'osaf';
const uint32 kShutDownOSTen = 'ossh';
const uint32 kRestartOSTen = 'osre';
const uint32 kClockTick = 'osct';
const uint32 kTrayDiagnostics = 'osdg';


// Draw the complete OSTen wordmark as the system menu's actual clickable item.
// The rest of the menu remains a normal BMenu with all of its original actions.
class OSTenLogoMenuItem : public BMenuItem {
public:
    OSTenLogoMenuItem(BMenu* submenu)
        :
        BMenuItem(submenu),
        fLogo(_CreateLogo())
    {
    }

    ~OSTenLogoMenuItem() override
    {
        delete fLogo;
    }

protected:
    void GetContentSize(float* width, float* height) override
    {
        if (fLogo == NULL) {
            BMenuItem::GetContentSize(width, height);
            return;
        }
        *width = kOSTenWordmarkWidth;
        *height = 18;
    }

    void DrawContent() override
    {
        if (fLogo == NULL) {
            BMenuItem::DrawContent();
            return;
        }
        BMenu* menu = Menu();
        if (menu == NULL)
            return;
        BPoint where = ContentLocation();
        menu->SetDrawingMode(B_OP_ALPHA);
        menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
        menu->DrawBitmap(fLogo, fLogo->Bounds(),
            BRect(where.x, where.y, where.x + kOSTenWordmarkWidth - 1,
                where.y + 17));
        menu->SetDrawingMode(B_OP_COPY);
    }

private:
    static BBitmap* _CreateLogo()
    {
        BBitmap* bitmap = new BBitmap(BRect(0, 0,
            kOSTenWordmarkWidth - 1, kOSTenWordmarkHeight - 1), B_RGBA32);
        if (bitmap->InitCheck() != B_OK) {
            delete bitmap;
            return NULL;
        }
        const char* alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        uint8* pixels = (uint8*)bitmap->Bits();
        const int32 stride = bitmap->BytesPerRow();
        for (int y = 0; y < kOSTenWordmarkHeight; y++) {
            for (int x = 0; x < kOSTenWordmarkWidth; x++) {
                const char* colorIndex = strchr(alphabet,
                    kOSTenWordmarkPixels[y][x]);
                if (colorIndex == NULL) {
                    delete bitmap;
                    return NULL;
                }
                uint32 rgba = kOSTenWordmarkPalette[colorIndex - alphabet];
                uint8* bgra = pixels + y * stride + x * 4;
                bgra[0] = (rgba >> 8) & 255;
                bgra[1] = (rgba >> 16) & 255;
                bgra[2] = (rgba >> 24) & 255;
                bgra[3] = rgba & 255;
            }
        }
        return bitmap;
    }

    BBitmap* fLogo;
};



class SystemBar : public BWindow {
public:
	SystemBar()
		:
		BWindow(BRect(0, 0, BScreen().Frame().Width(), 25), "System",
			B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
			B_NOT_CLOSABLE | B_NOT_MOVABLE | B_NOT_RESIZABLE
				| B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
				| B_WILL_ACCEPT_FIRST_CLICK),
		fClock(NULL), fTray(NULL), fSwitcher(NULL), fCenter(NULL),
		fUnread(0), fClockRunner(NULL)
	{
		// The bar must not cover the status controls to its right.
		// Haiku hit-tests sibling views front-to-back (first child first),
		// so a full-width BMenuBar would steal every Notices/Clock/Switcher
		// click, even when those controls render over it.
		BRect menuArea = Bounds();
		menuArea.right -= 289;
		BMenuBar* bar = new BMenuBar(menuArea, "System menu bar",
			B_FOLLOW_LEFT_TOP, B_ITEMS_IN_ROW, false);
		BMenu* systemMenu = new BMenu("OSTen");
		systemMenu->AddItem(new BMenuItem("About OSTen...",
			new BMessage(kAboutOSTen)));
		systemMenu->AddSeparatorItem();
		systemMenu->AddItem(new BMenuItem("Restart...",
			new BMessage(kRestartOSTen)));
		systemMenu->AddItem(new BMenuItem("Shut Down...",
			new BMessage(kShutDownOSTen)));
		bar->AddItem(new OSTenLogoMenuItem(systemMenu));

		BMenu* fileMenu = new BMenu("File");
		fileMenu->AddItem(new BMenuItem("New Folder",
			new BMessage(kOSTenNewFolder), 'N'));
		fileMenu->AddSeparatorItem();
		fileMenu->AddItem(new BMenuItem("Open",
			new BMessage(kOSTenOpen), 'O'));
		fileMenu->AddItem(new BMenuItem("Rename...",
			new BMessage(kOSTenRename)));
		fileMenu->AddItem(new BMenuItem("Get Info",
			new BMessage(kOSTenGetInfo), 'I'));
		fileMenu->AddItem(new BMenuItem("Close Window",
			new BMessage(kOSTenClose), 'W'));
		bar->AddItem(fileMenu);

		BMenu* editMenu = new BMenu("Edit");
		editMenu->AddItem(new BMenuItem("Select All",
			new BMessage(kOSTenSelectAll), 'A'));
		bar->AddItem(editMenu);

		BMenu* viewMenu = new BMenu("View");
		BMenuItem* iconsItem = new BMenuItem("as Icons",
			new BMessage(kOSTenViewAsIcons));
		iconsItem->SetMarked(true);
		viewMenu->AddItem(iconsItem);
		viewMenu->AddItem(new BMenuItem("as List",
			new BMessage(kOSTenViewAsList)));
		viewMenu->AddSeparatorItem();
		viewMenu->AddItem(new BMenuItem("Arrange by Name",
			new BMessage(kOSTenArrangeName)));
		viewMenu->AddItem(new BMenuItem("Arrange by Kind",
			new BMessage(kOSTenArrangeKind)));
		viewMenu->AddItem(new BMenuItem("Clean Up",
			new BMessage(kOSTenCleanUp)));
		bar->AddItem(viewMenu);

		BMenu* specialMenu = new BMenu("Special");
		specialMenu->AddItem(new BMenuItem("System Tray Diagnostics...",
			new BMessage(kTrayDiagnostics)));
		specialMenu->AddItem(new BMenuItem("Empty Trash...",
			new BMessage(kOSTenEmptyTrash)));
		specialMenu->AddItem(new BMenuItem("Dock / Undock Pop-Up Folder",
			new BMessage(kOSTenTogglePopupFolder)));
		specialMenu->AddSeparatorItem();
		specialMenu->AddItem(new BMenuItem("System Folder Access...",
			new BMessage(kOSTenSystemFolderAccess)));
		bar->AddItem(specialMenu);
		// The app switcher is a right-side status control, not an ordinary
		// File/Edit/View menu. Reserve separate space for all three controls.
		AddChild(bar);
		BRect bounds = Bounds();
		fTray = new BView(BRect(
            bounds.right - 225, 1, bounds.right - 193, 24),
            "Native Application Tray Slot", B_FOLLOW_RIGHT | B_FOLLOW_TOP,
            B_WILL_DRAW);
        fTray->SetViewColor(ui_color(B_MENU_BACKGROUND_COLOR));
        AddChild(fTray);
		fClock = CreateOSTenClockView(BRect(
			bounds.right - 191, 1, bounds.right - 44, 24));
		AddChild(fClock);
		// Classic Mac-style icon-only switcher at the far right.
		fSwitcher = CreateOSTenApplicationSwitcher(BRect(
			bounds.right - 43, 1, bounds.right - 2, 24));
		AddChild(fSwitcher);
		BMessage tick(kClockTick);
		// A two-second pulse also refreshes app labels after activations
		// without requiring a separate background polling thread.
		fClockRunner = new BMessageRunner(BMessenger(this), &tick, 2000000);
        // Native Deskbar owns the Replicant BShelf. Position its actual
        // interactive tray over this reserved menu-bar area.
        BMessenger deskbar("application/x-vnd.Be-TSKB");
        BMessage attach('osht');
        BMessage reply;
        if (deskbar.SendMessage(&attach, &reply, 250000, 250000) == B_OK) {
            float width;
            if (reply.FindFloat("width", &width) == B_OK)
                _UpdateTrayWidth(width);
        }
	}

	// Resize the reserved background and the menu area together.
	void _UpdateTrayWidth(float width)
	{
		if (width < 32)
			width = 32;
		BRect bounds = Bounds();
		float left = bounds.right - 193 - width;
		if (fTray != NULL) {
			fTray->MoveTo(left, 1);
			fTray->ResizeTo(width, 23);
		}
		BView* menus = FindView("System menu bar");
		if (menus != NULL)
			menus->ResizeTo(left, menus->Bounds().Height());
	}

	~SystemBar() override { delete fClockRunner; }

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kClockTick) {
			RefreshOSTenClock(fClock);
			RefreshOSTenApplicationSwitcher(fSwitcher);
            BMessenger deskbar("application/x-vnd.Be-TSKB");
            BMessage attach('osht');
            BMessage reply;
            if (deskbar.SendMessage(&attach, &reply, 250000, 250000) == B_OK) {
                float width;
                if (reply.FindFloat("width", &width) == B_OK)
                    _UpdateTrayWidth(width);
            }
			return;
		}
		if (message->what == kTrayDiagnostics) {
			BMessenger deskbar("application/x-vnd.Be-TSKB");
			BMessage query('osdq');
			BMessage reply;
			BString report("Native Deskbar System Tray\n\n");
			status_t error = deskbar.SendMessage(&query, &reply, 500000, 500000);
			if (error != B_OK) {
				report << "Deskbar did not respond: " << strerror(error);
			} else {
				bool hosted = false;
				int32 count = -1, attempts = 0, result = B_ERROR;
				reply.FindBool("host", &hosted);
				reply.FindInt32("count", &count);
				reply.FindInt32("attempts", &attempts);
				reply.FindInt32("result", &result);
				report << "OSTen host active: " << (hosted ? "Yes" : "No")
					<< "\nReplicants: " << count
					<< "\nRegistration attempts: " << attempts
					<< "\nLast attempt: ";
				if (attempts == 0)
					report << "None observed";
				else if (result == B_OK)
					report << "Accepted";
				else
					report << "Failed / incomplete (" << result << ")";
			}
			if (error == B_OK) {
				bool value = false;
				BRect frame;
				BPoint point;
				if (reply.FindBool("window_hidden", &value) == B_OK)
					report << "\nWindow hidden: " << (value ? "Yes" : "No");
				if (reply.FindRect("window_frame", &frame) == B_OK)
					report << "\nWindow at: " << (int)frame.left << ", "
						<< (int)frame.top << " size " << (int)frame.Width()
						<< "x" << (int)frame.Height();
				if (reply.FindBool("parent_present", &value) == B_OK)
					report << "\nTray has parent: " << (value ? "Yes" : "No");
				if (reply.FindBool("parent_hidden", &value) == B_OK)
					report << "\nParent hidden: " << (value ? "Yes" : "No");
				if (reply.FindBool("tray_hidden", &value) == B_OK)
					report << "\nTray hidden: " << (value ? "Yes" : "No");
				if (reply.FindRect("tray_frame", &frame) == B_OK)
					report << "\nTray frame: " << (int)frame.left << ", "
						<< (int)frame.top << " " << (int)frame.Width()
						<< "x" << (int)frame.Height();
				if (reply.FindBool("replicant_hidden", &value) == B_OK)
					report << "\nReplicant hidden: " << (value ? "Yes" : "No");
				else
					report << "\nReplicant child: Not found";
				if (reply.FindRect("replicant_frame", &frame) == B_OK)
					report << "\nReplicant frame: " << (int)frame.left << ", "
						<< (int)frame.top << " " << (int)frame.Width()
						<< "x" << (int)frame.Height();
				if (reply.FindPoint("replicant_screen", &point) == B_OK)
					report << "\nReplicant screen: " << (int)point.x
						<< ", " << (int)point.y;
			}
			(new BAlert("System Tray Diagnostics", report.String(), "OK"))->Go(NULL);
			return;
		}
		if (message->what == kOSTenShowNotices) {
			if (fCenter == NULL) {
				BRect screen = BScreen().Frame();
				BRect popup(screen.right - 425, 32,
					screen.right - 14, 379);
				fCenter = CreateOSTenNotificationCenter(
					BMessenger(this), popup);
				UpdateOSTenNotifications(fCenter, fNotices);
				fCenter->Show();
			} else {
				UpdateOSTenNotifications(fCenter, fNotices);
				if (fCenter->IsHidden())
					fCenter->Show();
				fCenter->Activate(true);
			}
			fUnread = 0;
			
			return;
		}
		if (message->what == kOSTenClearNotices) {
			fNotices.clear();
			fUnread = 0;
			
			UpdateOSTenNotifications(fCenter, fNotices);
			return;
		}
		if (message->what == kOSTenTestNotice) {
			BNotification test(B_INFORMATION_NOTIFICATION);
			test.SetTitle("OSTen Notification Test");
			test.SetContent("The native notification system is connected.");
			test.SetGroup("OSTen");
			if (test.Send() != B_OK)
				(new BAlert("Notifications",
					"The notification service is unavailable.",
					"OK"))->Go(NULL);
			return;
		}
		if (message->what == kOSTenNewNotice) {
			OSTenNotice notice;
			const char* value;
			if (message->FindString("_title", &value) == B_OK)
				notice.title = value;
			if (message->FindString("_content", &value) == B_OK)
				notice.content = value;
			if (message->FindString("_sourceName", &value) == B_OK)
				notice.source = value;
			if (notice.title.IsEmpty())
				notice.title = "System Notification";
			notice.when = time(NULL);
			fNotices.push_back(notice);
			if (fNotices.size() > 100)
				fNotices.erase(fNotices.begin());
			if (fCenter == NULL || fCenter->IsHidden())
				++fUnread;
			
			UpdateOSTenNotifications(fCenter, fNotices);
			return;
		}
		if (message->what == kOSTenActivateApplication
			|| message->what == kOSTenNextApplication) {
			BRoster roster;
			team_id team = -1;
			if (message->what == kOSTenActivateApplication) {
				int32 requested;
				if (message->FindInt32("team", &requested) == B_OK)
					team = requested;
			} else {
				BList teams;
				roster.GetAppList(&teams);
				app_info current;
				team_id active = roster.GetActiveAppInfo(&current) == B_OK
					? current.team : -1;
				std::vector<team_id> apps;
				for (int32 i = 0; i < teams.CountItems(); ++i) {
					team_id id = (team_id)(addr_t)teams.ItemAt(i);
					app_info info;
					if (roster.GetRunningAppInfo(id, &info) == B_OK
						&& !(info.flags & B_BACKGROUND_APP)
						&& strcmp(info.signature,
							"application/x-vnd.OSTen-System") != 0
						&& strcmp(info.signature,
							"application/x-vnd.Be-TSKB") != 0)
						apps.push_back(id);
				}
				if (!apps.empty()) {
					team = apps.front();
					for (size_t i = 0; i < apps.size(); ++i)
						if (apps[i] == active) {
							team = apps[(i + 1) % apps.size()];
							break;
						}
				}
			}
			if (team >= 0)
				roster.ActivateApp(team);
			RefreshOSTenApplicationSwitcher(fSwitcher);
			return;
		}
		if (message->what == kAboutOSTen) {
			if (fAboutMessenger.IsValid()) {
				BMessage focus(kFocusAbout);
				fAboutMessenger.SendMessage(&focus);
			} else {
				BWindow* about = CreateAboutComputerWindow();
				fAboutMessenger = BMessenger(about);
				about->Show();
				// Show() makes the window visible; an explicit activation also
				// ensures the first About request does not leave it behind Finder.
				about->Activate(true);
			}
			return;
		}
		if (message->what == kShutDownOSTen
			|| message->what == kRestartOSTen) {
			bool restart = message->what == kRestartOSTen;
			if ((new BAlert(restart ? "Restart OSTen" : "Shut Down OSTen",
				restart ? "Restart OSTen now? Save your work first."
					: "Shut down OSTen now? Save your work first.",
				"Cancel", restart ? "Restart" : "Shut Down"))->Go() != 1)
				return;
			// Use Haiku's normal registrar shutdown sequence: running
			// applications are notified and the filesystem is unmounted.
			BRoster roster;
			BRoster::Private privateRoster(roster);
			status_t error = privateRoster.ShutDown(restart, false, false);
			if (error != B_OK) {
				BString explanation("The system could not shut down: ");
				explanation << strerror(error);
				(new BAlert("System", explanation.String(), "OK"))->Go();
			}
			return;
		}
		if (message->what == kOSTenNewFolder
			|| message->what == kOSTenOpen
			|| message->what == kOSTenClose
			|| message->what == kOSTenSelectAll
			|| message->what == kOSTenViewAsIcons
			|| message->what == kOSTenViewAsList
			|| message->what == kOSTenRename
			|| message->what == kOSTenGetInfo
			|| message->what == kOSTenEmptyTrash
			|| message->what == kOSTenArrangeName
			|| message->what == kOSTenArrangeKind
			|| message->what == kOSTenCleanUp
			|| message->what == kOSTenTogglePopupFolder
			|| message->what == kOSTenSystemFolderAccess) {
			BMessenger finder("application/x-vnd.OSTen-Finder");
			finder.SendMessage(message);
			return;
		}
		BWindow::MessageReceived(message);
	}

private:
	BMessenger fAboutMessenger;
	BView* fClock;
	BView* fTray;
	BView* fSwitcher;
	BWindow* fCenter;
	int32 fUnread;
	BMessageRunner* fClockRunner;
	std::vector<OSTenNotice> fNotices;
};


class SystemApp : public BApplication {
public:
	SystemApp()
		:
		BApplication("application/x-vnd.OSTen-System")
	{
	}

	void ReadyToRun() override
	{
		fBar = new SystemBar();
		fBar->Show();
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kOSTenNewNotice
			|| message->what == kOSTenNextApplication
			|| message->what == kOSTenActivateApplication) {
			if (fBar != NULL)
				fBar->PostMessage(message);
			return;
		}
		BApplication::MessageReceived(message);
	}
private:
	SystemBar* fBar = NULL;
};

} // namespace


int
main()
{
	SystemApp app;
	app.Run();
	return 0;
}
