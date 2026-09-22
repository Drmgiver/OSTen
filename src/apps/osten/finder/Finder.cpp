/* SPDX-License-Identifier: MIT */

#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <InterfaceDefs.h>
#include <ListView.h>
#include <Message.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <StorageDefs.h>
#include <StringItem.h>
#include <View.h>
#include <Window.h>
#include <WindowPrivate.h>

#include <vector>


namespace {

const uint32 kOpenEntry = 'open';


class FinderWindow : public BWindow {
public:
	FinderWindow(const entry_ref& directory, int32 depth,
		const char* displayName = NULL)
		:
		BWindow(BRect(60 + depth * 28, 60 + depth * 28,
			530 + depth * 28, 405 + depth * 28), "Finder",
			B_TITLED_WINDOW, 0),
		fDepth(depth),
		fList(NULL)
	{
		BEntry folder(&directory);
		char title[B_FILE_NAME_LENGTH];
		if (displayName != NULL)
			SetTitle(displayName);
		else if (folder.GetName(title) == B_OK)
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
					(new FinderWindow(ref, fDepth + 1))->Show();
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


enum desktop_item {
	kNoDesktopItem,
	kBootVolumeItem,
	kTrashItem
};


class DesktopView : public BView {
public:
	DesktopView(BRect frame)
		:
		BView(frame, "OSTen Desktop", B_FOLLOW_ALL, B_WILL_DRAW),
		fSelectedItem(kNoDesktopItem)
	{
		SetViewColor((rgb_color){54, 104, 150, 255});
		_LayoutItems();
	}

	void Draw(BRect updateRect) override
	{
		_DrawDisk(fDiskRect, fSelectedItem == kBootVolumeItem);
		_DrawTrash(fTrashRect, fSelectedItem == kTrashItem);
	}

	void FrameResized(float width, float height) override
	{
		_LayoutItems();
		Invalidate();
	}

	void MouseDown(BPoint where) override
	{
		desktop_item item = kNoDesktopItem;
		if (fDiskHitRect.Contains(where))
			item = kBootVolumeItem;
		else if (fTrashHitRect.Contains(where))
			item = kTrashItem;

		fSelectedItem = item;
		Invalidate();

		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		if (item != kNoDesktopItem && clicks >= 2)
			_OpenItem(item);
	}

private:
	void _LayoutItems()
	{
		BRect bounds = Bounds();
		fDiskRect.Set(bounds.right - 76, 34, bounds.right - 36, 64);
		fDiskHitRect.Set(bounds.right - 108, 24, bounds.right - 12, 100);
		fTrashRect.Set(bounds.right - 70, bounds.bottom - 86,
			bounds.right - 42, bounds.bottom - 52);
		fTrashHitRect.Set(bounds.right - 108, bounds.bottom - 98,
			bounds.right - 12, bounds.bottom - 20);
	}

	void _DrawLabel(const char* label, BRect hitRect, bool selected)
	{
		float width = StringWidth(label);
		float x = hitRect.left + (hitRect.Width() - width) / 2;
		float y = hitRect.bottom - 8;
		BRect labelRect(x - 3, y - 13, x + width + 3, y + 3);
		if (selected) {
			SetHighColor(0, 0, 128);
			FillRect(labelRect);
			SetHighColor(255, 255, 255);
		} else
			SetHighColor(0, 0, 0);
		DrawString(label, BPoint(x, y));
	}

	void _DrawDisk(BRect rect, bool selected)
	{
		SetHighColor(40, 40, 40);
		FillRoundRect(rect, 4, 4);
		SetHighColor(224, 224, 224);
		BRect face = rect.InsetByCopy(2, 2);
		FillRoundRect(face, 3, 3);
		SetHighColor(90, 90, 90);
		StrokeLine(BPoint(face.left + 4, face.top + 7),
			BPoint(face.right - 4, face.top + 7));
		SetHighColor(0, 0, 0);
		FillRect(BRect(face.right - 6, face.bottom - 5,
			face.right - 3, face.bottom - 2));
		_DrawLabel("OSTen", fDiskHitRect, selected);
	}

	void _DrawTrash(BRect rect, bool selected)
	{
		SetHighColor(30, 30, 30);
		StrokeRect(BRect(rect.left + 3, rect.top + 7,
			rect.right - 3, rect.bottom));
		StrokeLine(BPoint(rect.left, rect.top + 6),
			BPoint(rect.right, rect.top + 6));
		StrokeLine(BPoint(rect.left + 8, rect.top + 2),
			BPoint(rect.right - 8, rect.top + 2));
		for (float x = rect.left + 9; x < rect.right; x += 7)
			StrokeLine(BPoint(x, rect.top + 11), BPoint(x, rect.bottom - 4));
		_DrawLabel("Trash", fTrashHitRect, selected);
	}

	void _OpenItem(desktop_item item)
	{
		const char* path = item == kBootVolumeItem ? "/boot" : "/boot/trash";
		const char* title = item == kBootVolumeItem ? "OSTen" : "Trash";
		BEntry entry(path);
		entry_ref ref;
		if (entry.GetRef(&ref) == B_OK)
			(new FinderWindow(ref, 0, title))->Show();
	}

private:
	desktop_item	fSelectedItem;
	BRect			fDiskRect;
	BRect			fDiskHitRect;
	BRect			fTrashRect;
	BRect			fTrashHitRect;
};


static BRect
DesktopFrame()
{
	BRect frame = BScreen().Frame();
	frame.top = 26;
	return frame;
}


class DesktopWindow : public BWindow {
public:
	DesktopWindow()
		:
		BWindow(DesktopFrame(), "Desktop", kDesktopWindowLook,
			kDesktopWindowFeel,
			B_NOT_MOVABLE | B_WILL_ACCEPT_FIRST_CLICK | B_NOT_ZOOMABLE
				| B_NOT_CLOSABLE | B_NOT_MINIMIZABLE | B_NOT_RESIZABLE
				| B_ASYNCHRONOUS_CONTROLS,
			B_ALL_WORKSPACES)
	{
		AddChild(new DesktopView(Bounds()));
	}

	bool QuitRequested() override
	{
		return false;
	}
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
		(new DesktopWindow())->Show();
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
