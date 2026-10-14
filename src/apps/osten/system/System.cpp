/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <Application.h>
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

#include "OSTenMessages.h"


namespace {

const uint32 kAboutOSTen = 'osab';
const uint32 kShutDownOSTen = 'ossh';
const uint32 kRestartOSTen = 'osre';


class SystemBar : public BWindow {
public:
	SystemBar()
		:
		BWindow(BRect(0, 0, BScreen().Frame().Width(), 25), "System",
			B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
			B_NOT_CLOSABLE | B_NOT_MOVABLE | B_NOT_RESIZABLE
				| B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE)
	{
		BMenuBar* bar = new BMenuBar(Bounds(), "System menu bar");
		BMenu* systemMenu = new BMenu("OSTen");
		systemMenu->AddItem(new BMenuItem("About OSTen...",
			new BMessage(kAboutOSTen)));
		systemMenu->AddSeparatorItem();
		systemMenu->AddItem(new BMenuItem("Restart...",
			new BMessage(kRestartOSTen)));
		systemMenu->AddItem(new BMenuItem("Shut Down...",
			new BMessage(kShutDownOSTen)));
		bar->AddItem(systemMenu);

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
		viewMenu->AddSeparatorItem();
		viewMenu->AddItem(new BMenuItem("Arrange by Name",
			new BMessage(kOSTenArrangeName)));
		viewMenu->AddItem(new BMenuItem("Arrange by Kind",
			new BMessage(kOSTenArrangeKind)));
		viewMenu->AddItem(new BMenuItem("Clean Up",
			new BMessage(kOSTenCleanUp)));
		bar->AddItem(viewMenu);

		BMenu* specialMenu = new BMenu("Special");
		specialMenu->AddItem(new BMenuItem("Empty Trash...",
			new BMessage(kOSTenEmptyTrash)));
		specialMenu->AddSeparatorItem();
		specialMenu->AddItem(new BMenuItem("System Folder Access...",
			new BMessage(kOSTenSystemFolderAccess)));
		bar->AddItem(specialMenu);
		AddChild(bar);
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kAboutOSTen) {
			(new BAlert("About OSTen", "OSTen preview", "OK"))->Go();
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
			|| message->what == kOSTenRename
			|| message->what == kOSTenGetInfo
			|| message->what == kOSTenEmptyTrash
			|| message->what == kOSTenArrangeName
			|| message->what == kOSTenArrangeKind
			|| message->what == kOSTenCleanUp
			|| message->what == kOSTenSystemFolderAccess) {
			BMessenger finder("application/x-vnd.OSTen-Finder");
			finder.SendMessage(message);
			return;
		}
		BWindow::MessageReceived(message);
	}
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
		(new SystemBar())->Show();
	}
};

} // namespace


int
main()
{
	SystemApp app;
	app.Run();
	return 0;
}
