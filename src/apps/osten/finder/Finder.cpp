/* SPDX-License-Identifier: MIT */

#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <ListView.h>
#include <Message.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StorageDefs.h>
#include <StringItem.h>
#include <Window.h>

#include <vector>


namespace {

const uint32 kOpenEntry = 'open';


class FinderWindow : public BWindow {
public:
	FinderWindow(const entry_ref& directory, int32 depth, bool desktop)
		:
		BWindow(BRect(60 + depth * 28, 60 + depth * 28,
			530 + depth * 28, 405 + depth * 28), "Finder",
			B_TITLED_WINDOW, desktop ? B_NOT_CLOSABLE : 0),
		fDepth(depth),
		fList(NULL)
	{
		BEntry folder(&directory);
		char title[B_FILE_NAME_LENGTH];
		if (!desktop && folder.GetName(title) == B_OK)
			SetTitle(title);

		BRect frame = Bounds();
		frame.InsetBy(10, 10);
		frame.right -= 15;
		fList = new BListView(frame, "Folder contents");
		fList->SetInvocationMessage(new BMessage(kOpenEntry));
		fList->SetTarget(this);
		AddChild(new BScrollView("Contents", fList, B_FOLLOW_ALL, 0,
			false, true));

		BDirectory contents(&directory);
		BEntry item;
		while (contents.GetNextEntry(&item) == B_OK) {
			entry_ref ref;
			char name[B_FILE_NAME_LENGTH];
			if (item.GetRef(&ref) != B_OK || item.GetName(name) != B_OK)
				continue;
			fEntries.push_back(ref);
			fList->AddItem(new BStringItem(name));
		}
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kOpenEntry) {
			int32 selection = fList->CurrentSelection();
			if (selection >= 0 && (size_t)selection < fEntries.size()) {
				entry_ref ref = fEntries[selection];
				BEntry item(&ref);
				if (item.IsDirectory())
					(new FinderWindow(ref, fDepth + 1, false))->Show();
				else
					be_roster->Launch(&ref);
			}
			return;
		}
		BWindow::MessageReceived(message);
	}

private:
	int32				fDepth;
	BListView*			fList;
	std::vector<entry_ref>	fEntries;
};


class FinderApp : public BApplication {
public:
	FinderApp()
		:
		BApplication("application/x-vnd.OSTen-Finder")
	{
	}

	void ReadyToRun() override
	{
		BEntry bootVolume("/boot");
		entry_ref ref;
		if (bootVolume.GetRef(&ref) == B_OK)
			(new FinderWindow(ref, 0, true))->Show();
	}
};

} // namespace


int
main()
{
	FinderApp app;
	app.Run();
	return 0;
}
