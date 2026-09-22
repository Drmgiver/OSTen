/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Messenger.h>
#include <Node.h>
#include <Path.h>
#include <Roster.h>
#include <Screen.h>
#include <StorageDefs.h>
#include <String.h>
#include <View.h>
#include <Window.h>
#include <WindowPrivate.h>

#include <algorithm>
#include <string.h>
#include <vector>

#include "OSTenMessages.h"


namespace {

const uint32 kOpenEntry = 'open';
const uint32 kFinderWindowActivated = 'oswa';
const char* kWindowFrameAttribute = "OSTen:window_frame";

const float kCellWidth = 94;
const float kCellHeight = 86;
const float kIconTop = 8;


struct FinderItem {
	entry_ref	ref;
	BString		name;
	bool		isDirectory;
	bool		selected;
	BRect		cell;
};


class FolderView : public BView {
public:
	FolderView(BRect frame, const entry_ref& directory)
		:
		BView(frame, "Folder icons", B_FOLLOW_ALL,
			B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
		fDirectory(directory),
		fIsBootVolumeRoot(false)
	{
		SetViewColor(255, 255, 255);
		SetLowColor(255, 255, 255);
		BEntry folder(&fDirectory);
		BPath path;
		fIsBootVolumeRoot = folder.GetPath(&path) == B_OK
			&& strcmp(path.Path(), "/boot") == 0;
		Refresh();
	}

	void AttachedToWindow() override
	{
		MakeFocus(true);
	}

	void Draw(BRect updateRect) override
	{
		for (size_t index = 0; index < fItems.size(); index++) {
			if (fItems[index].cell.Intersects(updateRect))
				_DrawItem(fItems[index]);
		}
	}

	void FrameResized(float width, float height) override
	{
		_LayoutItems();
		Invalidate();
	}

	void KeyDown(const char* bytes, int32 numBytes) override
	{
		if (numBytes == 1 && bytes[0] == B_ENTER) {
			_OpenSelection();
			return;
		}
		BView::KeyDown(bytes, numBytes);
	}

	void MouseDown(BPoint where) override
	{
		MakeFocus(true);
		int32 hit = _ItemAt(where);
		uint32 keyModifiers = modifiers();
		bool extend = (keyModifiers & (B_COMMAND_KEY | B_SHIFT_KEY)) != 0;

		if (!extend)
			_ClearSelection();
		if (hit >= 0) {
			if (extend)
				fItems[hit].selected = !fItems[hit].selected;
			else
				fItems[hit].selected = true;
		}
		Invalidate();

		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		if (hit >= 0 && clicks >= 2) {
			BMessage open(kOpenEntry);
			open.AddInt32("index", hit);
			Window()->PostMessage(&open);
		}
	}

	void Refresh(const char* selectName = NULL)
	{
		fItems.clear();
		BDirectory contents(&fDirectory);
		BEntry entry;
		while (contents.GetNextEntry(&entry) == B_OK) {
			entry_ref ref;
			char name[B_FILE_NAME_LENGTH];
			if (entry.GetRef(&ref) != B_OK || entry.GetName(name) != B_OK)
				continue;
			if (_ShouldHide(name))
				continue;

			FinderItem item;
			item.ref = ref;
			item.name = name;
			item.isDirectory = entry.IsDirectory();
			item.selected = selectName != NULL && item.name == selectName;
			fItems.push_back(item);
		}

		std::sort(fItems.begin(), fItems.end(),
			[](const FinderItem& left, const FinderItem& right) {
				return left.name.ICompare(right.name) < 0;
			});
		_LayoutItems();
		Invalidate();
	}

	void SelectAll()
	{
		for (size_t index = 0; index < fItems.size(); index++)
			fItems[index].selected = true;
		Invalidate();
	}

	std::vector<int32> SelectedIndices() const
	{
		std::vector<int32> selected;
		for (size_t index = 0; index < fItems.size(); index++) {
			if (fItems[index].selected)
				selected.push_back((int32)index);
		}
		return selected;
	}

	bool EntryAt(int32 index, entry_ref& ref) const
	{
		if (index < 0 || (size_t)index >= fItems.size())
			return false;
		ref = fItems[index].ref;
		return true;
	}

private:
	bool _ShouldHide(const char* name) const
	{
		if (name[0] == '.')
			return true;
		if (!fIsBootVolumeRoot)
			return false;
		return strcmp(name, "_packages") == 0
			|| strcmp(name, "data") == 0
			|| strcmp(name, "home") == 0
			|| strcmp(name, "system") == 0
			|| strcmp(name, "trash") == 0;
	}

	void _LayoutItems()
	{
		int32 columns = (int32)((Bounds().Width() - 12) / kCellWidth);
		if (columns < 1)
			columns = 1;
		for (size_t index = 0; index < fItems.size(); index++) {
			int32 column = (int32)index % columns;
			int32 row = (int32)index / columns;
			float left = 10 + column * kCellWidth;
			float top = 8 + row * kCellHeight;
			fItems[index].cell.Set(left, top, left + kCellWidth - 8,
				top + kCellHeight - 6);
		}
	}

	int32 _ItemAt(BPoint point) const
	{
		for (size_t index = 0; index < fItems.size(); index++) {
			if (fItems[index].cell.Contains(point))
				return (int32)index;
		}
		return -1;
	}

	void _ClearSelection()
	{
		for (size_t index = 0; index < fItems.size(); index++)
			fItems[index].selected = false;
	}

	void _OpenSelection()
	{
		for (size_t index = 0; index < fItems.size(); index++) {
			if (!fItems[index].selected)
				continue;
			BMessage open(kOpenEntry);
			open.AddInt32("index", (int32)index);
			Window()->PostMessage(&open);
		}
	}

	void _DrawItem(const FinderItem& item)
	{
		BRect icon(item.cell.left + 25, item.cell.top + kIconTop,
			item.cell.left + 61, item.cell.top + kIconTop + 34);
		if (item.isDirectory)
			_DrawFolderIcon(icon);
		else
			_DrawFileIcon(icon);

		BString display(item.name);
		TruncateString(&display, B_TRUNCATE_MIDDLE, item.cell.Width() - 6);
		float textWidth = StringWidth(display.String());
		float textX = item.cell.left + (item.cell.Width() - textWidth) / 2;
		float baseline = item.cell.top + 65;
		BRect label(textX - 3, baseline - 13, textX + textWidth + 3,
			baseline + 3);
		if (item.selected) {
			SetHighColor(0, 0, 128);
			FillRect(label);
			SetHighColor(255, 255, 255);
		} else
			SetHighColor(0, 0, 0);
		DrawString(display.String(), BPoint(textX, baseline));
	}

	void _DrawFolderIcon(BRect rect)
	{
		SetHighColor(35, 35, 35);
		FillRect(BRect(rect.left + 2, rect.top + 7, rect.right,
			rect.bottom));
		FillRect(BRect(rect.left + 6, rect.top + 2, rect.left + 20,
			rect.top + 9));
		SetHighColor(242, 206, 82);
		FillRect(BRect(rect.left + 4, rect.top + 9, rect.right - 2,
			rect.bottom - 2));
		FillRect(BRect(rect.left + 8, rect.top + 4, rect.left + 19,
			rect.top + 9));
		SetHighColor(255, 231, 137);
		StrokeLine(BPoint(rect.left + 6, rect.top + 11),
			BPoint(rect.right - 4, rect.top + 11));
	}

	void _DrawFileIcon(BRect rect)
	{
		SetHighColor(35, 35, 35);
		FillRect(BRect(rect.left + 7, rect.top, rect.right - 5,
			rect.bottom));
		SetHighColor(244, 244, 244);
		FillRect(BRect(rect.left + 9, rect.top + 2, rect.right - 7,
			rect.bottom - 2));
		SetHighColor(110, 110, 110);
		StrokeLine(BPoint(rect.left + 13, rect.top + 11),
			BPoint(rect.right - 11, rect.top + 11));
		StrokeLine(BPoint(rect.left + 13, rect.top + 17),
			BPoint(rect.right - 11, rect.top + 17));
		StrokeLine(BPoint(rect.left + 13, rect.top + 23),
			BPoint(rect.right - 15, rect.top + 23));
	}

private:
	entry_ref				fDirectory;
	bool					fIsBootVolumeRoot;
	std::vector<FinderItem>	fItems;
};


static BRect
FolderWindowFrame(const entry_ref& directory, int32 depth)
{
	BRect frame(60 + depth * 28, 60 + depth * 28,
		530 + depth * 28, 405 + depth * 28);
	BNode node(&directory);
	BRect stored;
	if (node.ReadAttr(kWindowFrameAttribute, B_RECT_TYPE, 0, &stored,
			sizeof(BRect)) == (ssize_t)sizeof(BRect)
		&& stored.Width() >= 280 && stored.Height() >= 180) {
		frame = stored;
	}

	BRect screen = BScreen().Frame();
	screen.top = 26;
	if (frame.Width() > screen.Width())
		frame.right = frame.left + screen.Width();
	if (frame.Height() > screen.Height())
		frame.bottom = frame.top + screen.Height();
	if (frame.left < screen.left)
		frame.OffsetBy(screen.left - frame.left, 0);
	if (frame.top < screen.top)
		frame.OffsetBy(0, screen.top - frame.top);
	if (frame.right > screen.right)
		frame.OffsetBy(screen.right - frame.right, 0);
	if (frame.bottom > screen.bottom)
		frame.OffsetBy(0, screen.bottom - frame.bottom);
	return frame;
}


static void
NotifyFinderWindowActivated(BWindow* window)
{
	BMessage activated(kFinderWindowActivated);
	activated.AddMessenger("window", BMessenger(window));
	be_app->PostMessage(&activated);
}


class FinderWindow : public BWindow {
public:
	FinderWindow(const entry_ref& directory, int32 depth,
		const char* displayName = NULL)
		:
		BWindow(FolderWindowFrame(directory, depth), "Finder",
			B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS),
		fDirectory(directory),
		fDepth(depth),
		fView(NULL)
	{
		BEntry folder(&directory);
		char title[B_FILE_NAME_LENGTH];
		if (displayName != NULL)
			SetTitle(displayName);
		else if (folder.GetName(title) == B_OK)
			SetTitle(title);

		BRect frame = Bounds();
		frame.InsetBy(7, 7);
		fView = new FolderView(frame, directory);
		AddChild(fView);
	}

	void FrameMoved(BPoint newPosition) override
	{
		BWindow::FrameMoved(newPosition);
		_SaveFrame();
	}

	void FrameResized(float width, float height) override
	{
		BWindow::FrameResized(width, height);
		_SaveFrame();
	}

	void WindowActivated(bool active) override
	{
		BWindow::WindowActivated(active);
		if (active)
			NotifyFinderWindowActivated(this);
	}

	bool QuitRequested() override
	{
		_SaveFrame();
		return true;
	}

	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kOpenEntry:
			{
				int32 index;
				if (message->FindInt32("index", &index) == B_OK)
					_OpenEntry(index);
				return;
			}
			case kOSTenOpen:
				_OpenSelection();
				return;
			case kOSTenClose:
				PostMessage(B_QUIT_REQUESTED);
				return;
			case kOSTenNewFolder:
				_CreateFolder();
				return;
			case kOSTenSelectAll:
				fView->SelectAll();
				return;
			case kOSTenViewAsIcons:
				return;
		}
		BWindow::MessageReceived(message);
	}

private:
	void _OpenEntry(int32 index)
	{
		entry_ref ref;
		if (!fView->EntryAt(index, ref))
			return;
		BEntry entry(&ref);
		if (entry.IsDirectory())
			(new FinderWindow(ref, fDepth + 1))->Show();
		else
			be_roster->Launch(&ref);
	}

	void _OpenSelection()
	{
		std::vector<int32> selected = fView->SelectedIndices();
		for (size_t index = 0; index < selected.size(); index++)
			_OpenEntry(selected[index]);
	}

	void _CreateFolder()
	{
		BDirectory directory(&fDirectory);
		BString name("untitled folder");
		int32 suffix = 2;
		while (directory.Contains(name.String())) {
			name.SetToFormat("untitled folder %ld", (long)suffix);
			suffix++;
		}
		if (directory.CreateDirectory(name.String(), NULL) == B_OK)
			fView->Refresh(name.String());
		else {
			(new BAlert("New Folder", "The folder could not be created.",
				"OK"))->Go();
		}
	}

	void _SaveFrame()
	{
		BNode node(&fDirectory);
		BRect frame = Frame();
		node.WriteAttr(kWindowFrameAttribute, B_RECT_TYPE, 0, &frame,
			sizeof(BRect));
	}

private:
	entry_ref	fDirectory;
	int32		fDepth;
	FolderView*	fView;
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
		BView(frame, "OSTen Desktop", B_FOLLOW_ALL,
			B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
		fDiskSelected(false),
		fTrashSelected(false)
	{
		SetViewColor(54, 104, 150);
		_LayoutItems();
	}

	void AttachedToWindow() override
	{
		MakeFocus(true);
	}

	void Draw(BRect updateRect) override
	{
		_DrawDisk(fDiskRect, fDiskSelected);
		_DrawTrash(fTrashRect, fTrashSelected);
	}

	void FrameResized(float width, float height) override
	{
		_LayoutItems();
		Invalidate();
	}

	void KeyDown(const char* bytes, int32 numBytes) override
	{
		if (numBytes == 1 && bytes[0] == B_ENTER) {
			OpenSelection();
			return;
		}
		BView::KeyDown(bytes, numBytes);
	}

	void MouseDown(BPoint where) override
	{
		MakeFocus(true);
		desktop_item item = kNoDesktopItem;
		if (fDiskHitRect.Contains(where))
			item = kBootVolumeItem;
		else if (fTrashHitRect.Contains(where))
			item = kTrashItem;

		bool extend = (modifiers() & (B_COMMAND_KEY | B_SHIFT_KEY)) != 0;
		if (!extend) {
			fDiskSelected = false;
			fTrashSelected = false;
		}
		if (item == kBootVolumeItem)
			fDiskSelected = extend ? !fDiskSelected : true;
		else if (item == kTrashItem)
			fTrashSelected = extend ? !fTrashSelected : true;
		Invalidate();

		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		if (item != kNoDesktopItem && clicks >= 2)
			_OpenItem(item);
	}

	void OpenSelection()
	{
		if (!fDiskSelected && !fTrashSelected) {
			_OpenItem(kBootVolumeItem);
			return;
		}
		if (fDiskSelected)
			_OpenItem(kBootVolumeItem);
		if (fTrashSelected)
			_OpenItem(kTrashItem);
	}

	void SelectAll()
	{
		fDiskSelected = true;
		fTrashSelected = true;
		Invalidate();
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
	bool	fDiskSelected;
	bool	fTrashSelected;
	BRect	fDiskRect;
	BRect	fDiskHitRect;
	BRect	fTrashRect;
	BRect	fTrashHitRect;
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
			B_ALL_WORKSPACES),
		fView(new DesktopView(Bounds()))
	{
		AddChild(fView);
	}

	void WindowActivated(bool active) override
	{
		BWindow::WindowActivated(active);
		if (active)
			NotifyFinderWindowActivated(this);
	}

	bool QuitRequested() override
	{
		return false;
	}

	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kOSTenOpen:
				fView->OpenSelection();
				return;
			case kOSTenSelectAll:
				fView->SelectAll();
				return;
			case kOSTenClose:
			case kOSTenNewFolder:
			case kOSTenViewAsIcons:
				return;
		}
		BWindow::MessageReceived(message);
	}

private:
	DesktopView* fView;
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
		DesktopWindow* desktop = new DesktopWindow();
		fDesktopTarget = BMessenger(desktop);
		fCommandTarget = fDesktopTarget;
		desktop->Show();
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kFinderWindowActivated) {
			BMessenger target;
			if (message->FindMessenger("window", &target) == B_OK)
				fCommandTarget = target;
			return;
		}
		if (message->what == kOSTenNewFolder
			|| message->what == kOSTenOpen
			|| message->what == kOSTenClose
			|| message->what == kOSTenSelectAll
			|| message->what == kOSTenViewAsIcons) {
			if (fCommandTarget.SendMessage(message) != B_OK)
				fDesktopTarget.SendMessage(message);
			return;
		}
		BApplication::MessageReceived(message);
	}

private:
	BMessenger fCommandTarget;
	BMessenger fDesktopTarget;
};

} // namespace


int
main()
{
	FinderApp app;
	app.Run();
	return 0;
}
