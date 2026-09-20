/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <Application.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Screen.h>
#include <Window.h>


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
		AddChild(bar);
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kAboutOSTen) {
			(new BAlert("About OSTen", "OSTen preview", "OK"))->Go();
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
