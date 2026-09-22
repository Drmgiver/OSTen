/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <Application.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Screen.h>
#include <Window.h>

#include "OSTenMessages.h"


namespace {

const uint32 kAboutOSTen = 'osab';


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
		bar->AddItem(systemMenu);

		BMenu* fileMenu = new BMenu("File");
		fileMenu->AddItem(new BMenuItem("New Folder",
			new BMessage(kOSTenNewFolder), 'N'));
		fileMenu->AddSeparatorItem();
		fileMenu->AddItem(new BMenuItem("Open",
			new BMessage(kOSTenOpen), 'O'));
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
		bar->AddItem(viewMenu);

		BMenu* specialMenu = new BMenu("Special");
		BMenuItem* emptyTrash = new BMenuItem("Empty Trash...", NULL);
		emptyTrash->SetEnabled(false);
		specialMenu->AddItem(emptyTrash);
		bar->AddItem(specialMenu);
		AddChild(bar);
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kAboutOSTen) {
			(new BAlert("About OSTen", "OSTen preview", "OK"))->Go();
			return;
		}
		if (message->what == kOSTenNewFolder
			|| message->what == kOSTenOpen
			|| message->what == kOSTenClose
			|| message->what == kOSTenSelectAll
			|| message->what == kOSTenViewAsIcons) {
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
