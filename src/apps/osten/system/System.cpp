/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <sys/stat.h>
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
#include "ControlStrip.h"
#include "OSTenMessages.h"
#include "SystemExtras.h"
#include "OSTenWordmark.h"
#include "OSTenResources.h"
#include <string>
#include <sstream>
#include <set>


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
        BBitmap* override = OSTenSystemResources().Bitmap("wordmark",
            kOSTenWordmarkWidth, kOSTenWordmarkHeight);
        if (override != NULL)
            return override;
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



// Stable command IDs allow the visual Resource Editor to rearrange menus
// without granting it permission to execute arbitrary code.
struct OSTenActionDefinition {
    const char* id;
    const char* label;
    const char* shortcutKey;
    char defaultShortcut;
    uint32 message;
};
static const OSTenActionDefinition kSystemActions[] = {
    { "about", "About OSTen...", NULL, 0, kAboutOSTen },
    { "restart", "Restart...", NULL, 0, kRestartOSTen },
    { "shutdown", "Shut Down...", NULL, 0, kShutDownOSTen },
    { "new_folder", "New Folder", "shortcut.new_folder", 'N', kOSTenNewFolder },
    { "open", "Open", "shortcut.open", 'O', kOSTenOpen },
    { "rename", "Rename...", NULL, 0, kOSTenRename },
    { "get_info", "Get Info", "shortcut.get_info", 'I', kOSTenGetInfo },
    { "close", "Close Window", "shortcut.close", 'W', kOSTenClose },
    { "select_all", "Select All", "shortcut.select_all", 'A', kOSTenSelectAll },
    { "view_icons", "as Icons", NULL, 0, kOSTenViewAsIcons },
    { "view_list", "as List", NULL, 0, kOSTenViewAsList },
    { "arrange_name", "Arrange by Name", NULL, 0, kOSTenArrangeName },
    { "arrange_kind", "Arrange by Kind", NULL, 0, kOSTenArrangeKind },
    { "clean_up", "Clean Up", NULL, 0, kOSTenCleanUp },
    { "tray_diagnostics", "System Tray Diagnostics...", NULL, 0, kTrayDiagnostics },
    { "empty_trash", "Empty Trash...", NULL, 0, kOSTenEmptyTrash },
    { "toggle_popup", "Dock / Undock Pop-Up Folder", NULL, 0, kOSTenTogglePopupFolder },
    { "system_folder_access", "System Folder Access...", NULL, 0, kOSTenSystemFolderAccess }
};

static BMenu*
_ResourceMenu(const std::string& section, BMenuItem*& icons,
    BMenuItem*& list)
{
    const OSTenResourceCatalog& resources = OSTenSystemResources();
    const char* defaultItems = NULL;
    const char* defaultTitle = NULL;
    if (section == "system") {
        defaultItems = "about,separator,restart,shutdown";
        defaultTitle = "OSTen";
    } else if (section == "file") {
        defaultItems = "new_folder,separator,open,rename,get_info,close";
        defaultTitle = "File";
    } else if (section == "edit") {
        defaultItems = "select_all";
        defaultTitle = "Edit";
    } else if (section == "view") {
        defaultItems = "view_icons,view_list,separator,arrange_name,arrange_kind,clean_up";
        defaultTitle = "View";
    } else if (section == "special") {
        defaultItems = "tray_diagnostics,empty_trash,toggle_popup,separator,system_folder_access";
        defaultTitle = "Special";
    } else
        return NULL;

    const std::string titleKey = "menu.title." + section;
    const std::string itemsKey = "menu." + section;
    BMenu* menu = new BMenu(resources.String(titleKey.c_str(), defaultTitle));
    std::stringstream source(resources.String(itemsKey.c_str(), defaultItems));
    std::string entry;
    std::set<std::string> added;
    while (std::getline(source, entry, ',')) {
        if (entry == "separator") {
            menu->AddSeparatorItem();
            continue;
        }
        if (!added.insert(entry).second)
            continue;
        for (size_t i = 0; i < sizeof(kSystemActions) / sizeof(kSystemActions[0]); ++i) {
            const OSTenActionDefinition& action = kSystemActions[i];
            if (entry != action.id)
                continue;
            std::string labelKey = std::string("menu.item.") + action.id;
            char shortcut = 0;
            if (action.shortcutKey != NULL) {
                const char* value = OSTenFinderResources().String(
                    action.shortcutKey, "");
                shortcut = strlen(value) == 1 ? value[0] : action.defaultShortcut;
            }
            BMenuItem* item = new BMenuItem(
                resources.String(labelKey.c_str(), action.label),
                new BMessage(action.message), shortcut);
            menu->AddItem(item);
            if (entry == "view_icons") {
                icons = item;
                icons->SetMarked(true);
            } else if (entry == "view_list")
                list = item;
            break;
        }
    }
    return menu;
}


class SystemBar : public BWindow {
public:
	SystemBar()
		:
		BWindow(BRect(0, 0, BScreen().Frame().Width(),
                OSTenSystemResources().Number("layout.bar.height", 25, 24, 40)),
            OSTenSystemResources().String("layout.bar.title", "System"),
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
		menuArea.right -= OSTenSystemResources().Number(
            "layout.menu.reserve", 289, 240, 500);
		BMenuBar* bar = new BMenuBar(menuArea, "System menu bar",
			B_FOLLOW_LEFT_TOP, B_ITEMS_IN_ROW, false);
        // The menu structure, labels, order and shortcuts come from editable
        // System and Finder resource catalogs. Action IDs remain fixed.
        const OSTenResourceCatalog& resources = OSTenSystemResources();
        const char* order = resources.String("menu.order",
            "system,file,edit,view,special");
        std::stringstream menuOrder(order);
        std::string section;
        std::set<std::string> added;
        while (std::getline(menuOrder, section, ',')) {
            if (section.empty() || !added.insert(section).second)
                continue;
            BMenu* menu = _ResourceMenu(section, fIconsMenuItem,
                fListMenuItem);
            if (menu == NULL)
                continue;
            if (section == "system")
                bar->AddItem(new OSTenLogoMenuItem(menu));
            else
                bar->AddItem(menu);
        }
		// The app switcher is a right-side status control, not an ordinary
		// File/Edit/View menu. Reserve separate space for all three controls.
		AddChild(bar);
		BRect bounds = Bounds();
        BRect trayFrame = OSTenSystemResources().Rect(
            "layout.bar.tray", BRect(-225, 1, -193, 24));
        trayFrame.OffsetBy(bounds.right, 0);
        fTray = new BView(trayFrame, "Native Application Tray Slot",
            B_FOLLOW_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW);
        fTray->SetViewColor(ui_color(B_MENU_BACKGROUND_COLOR));
        AddChild(fTray);
        BRect clockFrame = OSTenSystemResources().Rect(
            "layout.bar.clock", BRect(-191, 1, -44, 24));
        clockFrame.OffsetBy(bounds.right, 0);
        fClock = CreateOSTenClockView(clockFrame);
		AddChild(fClock);
		// Classic Mac-style icon-only switcher at the far right.
        BRect switcherFrame = OSTenSystemResources().Rect(
            "layout.bar.switcher", BRect(-43, 1, -2, 24));
        switcherFrame.OffsetBy(bounds.right, 0);
        fSwitcher = CreateOSTenApplicationSwitcher(switcherFrame);
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
        float left = (fClock != NULL ? fClock->Frame().left
            : bounds.right - 191) - width
            - OSTenSystemResources().Number("layout.bar.tray_clock_gap",
                2, 0, 16);
		if (fTray != NULL) {
			fTray->MoveTo(left, fTray->Frame().top);
			fTray->ResizeTo(width, fTray->Bounds().Height());
		}
		BView* menus = FindView("System menu bar");
		if (menus != NULL)
			menus->ResizeTo(left, menus->Bounds().Height());
	}

	~SystemBar() override { delete fClockRunner; }

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kOSTenFinderViewModeChanged) {
			bool listMode = false;
			if (message->FindBool("list_mode", &listMode) == B_OK) {
				if (fIconsMenuItem != NULL)
                    fIconsMenuItem->SetMarked(!listMode);
				if (fListMenuItem != NULL)
                    fListMenuItem->SetMarked(listMode);
			}
			return;
		}
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
			(new BAlert(OSTenSystemResources().String("dialog.tray_diagnostics.title",
                "System Tray Diagnostics"), report.String(),
                OSTenSystemResources().String("dialog.ok", "OK")))->Go(NULL);
			return;
		}
		if (message->what == kOSTenShowNotices) {
			if (fCenter == NULL) {
				BRect screen = BScreen().Frame();
                BRect popup = OSTenSystemResources().Rect(
                    "notification.window_frame",
                    BRect(-425, 32, -14, 379));
                popup.OffsetBy(screen.right, 0);
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
			test.SetTitle(OSTenSystemResources().String(
                    "notification.test.title", "OSTen Notification Test"));
			test.SetContent(OSTenSystemResources().String(
                    "notification.test.message",
                    "The native notification system is connected."));
			test.SetGroup("OSTen");
			if (test.Send() != B_OK)
				(new BAlert(OSTenSystemResources().String("dialog.notifications.title", "Notifications"),
                    OSTenSystemResources().String("dialog.notifications.unavailable", "The notification service is unavailable."),
                    OSTenSystemResources().String("dialog.ok", "OK")))->Go(NULL);
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
				notice.title = OSTenSystemResources().String(
                    "notification.default_title", "System Notification");
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
            const OSTenResourceCatalog& ui = OSTenSystemResources();
            if ((new BAlert(
                restart ? ui.String("dialog.restart.title", "Restart OSTen")
                    : ui.String("dialog.shutdown.title", "Shut Down OSTen"),
                restart ? ui.String("dialog.restart.message",
                        "Restart OSTen now? Save your work first.")
                    : ui.String("dialog.shutdown.message",
                        "Shut down OSTen now? Save your work first."),
                ui.String("dialog.cancel", "Cancel"),
                restart ? ui.String("dialog.restart.button", "Restart")
                    : ui.String("dialog.shutdown.button", "Shut Down")))->Go() != 1)
				return;
			// Use Haiku's normal registrar shutdown sequence: running
			// applications are notified and the filesystem is unmounted.
			BRoster roster;
			BRoster::Private privateRoster(roster);
			status_t error = privateRoster.ShutDown(restart, false, false);
			if (error != B_OK) {
				BString explanation(OSTenSystemResources().String("dialog.power.error", "The system could not shut down: "));
				explanation << strerror(error);
				(new BAlert(OSTenSystemResources().String("dialog.system.title", "System"), explanation.String(), OSTenSystemResources().String("dialog.ok", "OK")))->Go();
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
	BMenuItem* fIconsMenuItem = NULL;
	BMenuItem* fListMenuItem = NULL;
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
		// Reserve a user-visible home for independent Control Strip modules.
        // Built-in modules remain compiled into OSTen System for now.
        mkdir("/boot/System Folder/Control Strip Modules", 0755);
        fBar = new SystemBar();
		fBar->Show();
        fControlStrip = CreateOSTenControlStrip();
        fControlStrip->Show();
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kOSTenNewNotice
			|| message->what == kOSTenFinderViewModeChanged
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
    BWindow* fControlStrip = NULL;
};

} // namespace


int
main()
{
	SystemApp app;
	app.Run();
	return 0;
}
