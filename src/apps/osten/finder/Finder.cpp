/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <Application.h>
#include <CopyEngine.h>
#include <Directory.h>
#include <Entry.h>
#include <InterfaceDefs.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Node.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <RemoveEngine.h>
#include <Roster.h>
#include <Screen.h>
#include <StorageDefs.h>
#include <String.h>
#include <View.h>
#include <Window.h>
#include <WindowPrivate.h>

#include <algorithm>
#include <math.h>
#include <string.h>
#include <vector>

#include "OSTenMessages.h"


namespace {

const uint32 kOpenEntry = 'open';
const uint32 kFinderWindowActivated = 'oswa';
const uint32 kFinderDragEntries = 'osdg';
const uint32 kFinderMoveToTrash = 'ostr';
const uint32 kFinderRefreshAll = 'osrf';
const uint32 kFinderRefreshFolder = 'osrw';
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


static bool
PathContains(const BPath& directory, const BPath& entry)
{
	const char* directoryPath = directory.Path();
	const char* entryPath = entry.Path();
	size_t length = strlen(directoryPath);
	return strncmp(directoryPath, entryPath, length) == 0
		&& (entryPath[length] == '\0' || entryPath[length] == '/');
}


static void
MakeUniqueName(BDirectory& directory, const char* original, BString& name)
{
	name = original;
	if (!directory.Contains(name.String()))
		return;

	for (int32 suffix = 2; ; suffix++) {
		name.SetToFormat("%s %ld", original, (long)suffix);
		if (!directory.Contains(name.String()))
			return;
	}
}


static status_t
TransferEntry(const entry_ref& sourceRef, BDirectory& targetDirectory,
	const BPath& targetDirectoryPath, bool copy, bool preserveExisting)
{
	BEntry source(&sourceRef, false);
	if (source.InitCheck() != B_OK)
		return source.InitCheck();

	BPath sourcePath;
	status_t error = source.GetPath(&sourcePath);
	if (error != B_OK)
		return error;

	if (source.IsDirectory() && PathContains(sourcePath, targetDirectoryPath))
		return B_BAD_VALUE;

	char originalName[B_FILE_NAME_LENGTH];
	error = source.GetName(originalName);
	if (error != B_OK)
		return error;

	BEntry parent;
	error = source.GetParent(&parent);
	if (error != B_OK)
		return error;
	BEntry targetDirectoryEntry;
	error = targetDirectory.GetEntry(&targetDirectoryEntry);
	if (error != B_OK)
		return error;
	bool sameDirectory = parent == targetDirectoryEntry;
	if (sameDirectory && !copy)
		return B_OK;

	BString targetName(originalName);
	if (copy && sameDirectory) {
		BString copyName;
		copyName.SetToFormat("%s copy", originalName);
		MakeUniqueName(targetDirectory, copyName.String(), targetName);
	} else if (preserveExisting)
		MakeUniqueName(targetDirectory, originalName, targetName);

	BPath destinationPath(targetDirectoryPath);
	error = destinationPath.Append(targetName.String());
	if (error != B_OK)
		return error;

	BEntry destination(destinationPath.Path(), false);
	if (destination.Exists()) {
		error = BRemoveEngine().RemoveEntry(destination);
		if (error != B_OK)
			return error;
	}

	if (copy) {
		return BCopyEngine(BCopyEngine::COPY_RECURSIVELY
			| BCopyEngine::UNLINK_DESTINATION).CopyEntry(sourcePath.Path(),
				destinationPath.Path());
	}

	error = source.MoveTo(&targetDirectory, targetName.String(), true);
	if (error != B_CROSS_DEVICE_LINK)
		return error;

	error = BCopyEngine(BCopyEngine::COPY_RECURSIVELY
		| BCopyEngine::UNLINK_DESTINATION).CopyEntry(sourcePath.Path(),
			destinationPath.Path());
	if (error != B_OK)
		return error;
	return BRemoveEngine().RemoveEntry(source);
}


static void
TransferEntries(const BMessage* message, const entry_ref& targetRef,
	bool preserveExisting = false)
{
	BDirectory targetDirectory(&targetRef);
	BEntry targetEntry(&targetRef);
	BPath targetPath;
	status_t firstError = targetDirectory.InitCheck();
	if (firstError == B_OK)
		firstError = targetEntry.GetPath(&targetPath);

	bool copy = false;
	message->FindBool("copy", &copy);
	for (int32 index = 0; firstError == B_OK; index++) {
		entry_ref sourceRef;
		if (message->FindRef("refs", index, &sourceRef) != B_OK)
			break;
		status_t error = TransferEntry(sourceRef, targetDirectory, targetPath,
			copy, preserveExisting);
		if (error != B_OK)
			firstError = error;
	}

	if (firstError != B_OK) {
		(new BAlert("Finder", "The item could not be moved or copied.",
			"OK"))->Go();
	}
	be_app->PostMessage(kFinderRefreshAll);
}


class FolderView : public BView {
public:
	FolderView(BRect frame, const entry_ref& directory)
		:
		BView(frame, "Folder icons", B_FOLLOW_ALL,
			B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
		fDirectory(directory),
		fIsBootVolumeRoot(false),
		fTrackingDrag(false),
		fPressedIndex(-1)
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
		int32 buttons = B_PRIMARY_MOUSE_BUTTON;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("buttons", &buttons);

		if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
			if (hit >= 0 && !fItems[hit].selected) {
				_ClearSelection();
				fItems[hit].selected = true;
				Invalidate();
			}
			_ShowContextMenu(where, hit);
			return;
		}
		if ((buttons & B_PRIMARY_MOUSE_BUTTON) == 0)
			return;

		uint32 keyModifiers = modifiers();
		bool extend = (keyModifiers & (B_COMMAND_KEY | B_SHIFT_KEY)) != 0;

		if (!extend && (hit < 0 || !fItems[hit].selected))
			_ClearSelection();
		if (hit >= 0 && extend)
			fItems[hit].selected = !fItems[hit].selected;
		else if (hit >= 0)
			fItems[hit].selected = true;
		Invalidate();

		int32 clicks = 1;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("clicks", &clicks);
		if (hit >= 0 && clicks >= 2) {
			BMessage open(kOpenEntry);
			open.AddInt32("index", hit);
			Window()->PostMessage(&open);
			return;
		}

		fTrackingDrag = hit >= 0 && fItems[hit].selected;
		fPressedIndex = hit;
		fDragStart = where;
		if (fTrackingDrag)
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	}

	void MouseUp(BPoint where) override
	{
		fTrackingDrag = false;
		fPressedIndex = -1;
	}

	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage) override
	{
		if (!fTrackingDrag || dragMessage != NULL)
			return;
		if (fabsf(where.x - fDragStart.x) < 4
			&& fabsf(where.y - fDragStart.y) < 4) {
			return;
		}

		BMessage drag(kFinderDragEntries);
		_AddSelectedRefs(drag);
		drag.AddBool("copy", (modifiers() & B_OPTION_KEY) != 0);
		BRect dragRect = fItems[fPressedIndex].cell;
		fTrackingDrag = false;
		DragMessage(&drag, dragRect, this);
	}

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kFinderDragEntries && message->WasDropped()) {
			entry_ref target = fDirectory;
			BPoint point = message->DropPoint();
			ConvertFromScreen(&point);
			int32 hit = _ItemAt(point);
			if (hit >= 0 && fItems[hit].isDirectory)
				target = fItems[hit].ref;
			TransferEntries(message, target);
			return;
		}
		BView::MessageReceived(message);
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

	void MoveSelectionToTrash()
	{
		BMessage move(kFinderDragEntries);
		_AddSelectedRefs(move);
		move.AddBool("copy", false);
		BEntry trash("/boot/trash");
		entry_ref trashRef;
		if (trash.GetRef(&trashRef) == B_OK)
			TransferEntries(&move, trashRef, true);
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
	void _AddSelectedRefs(BMessage& message) const
	{
		for (size_t index = 0; index < fItems.size(); index++) {
			if (fItems[index].selected)
				message.AddRef("refs", &fItems[index].ref);
		}
	}

	void _ShowContextMenu(BPoint where, int32 hit)
	{
		BPopUpMenu* menu = new BPopUpMenu("", false, false);
		if (hit >= 0) {
			BMessage* open = new BMessage(kOpenEntry);
			open->AddInt32("index", hit);
			menu->AddItem(new BMenuItem("Open", open));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Move to Trash",
				new BMessage(kFinderMoveToTrash)));
		} else {
			menu->AddItem(new BMenuItem("New Folder",
				new BMessage(kOSTenNewFolder)));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Select All",
				new BMessage(kOSTenSelectAll)));
		}
		menu->SetTargetForItems(Window());
		menu->SetAsyncAutoDestruct(true);
		ConvertToScreen(&where);
		menu->Go(where, true, false, true);
	}

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
	bool					fTrackingDrag;
	int32					fPressedIndex;
	BPoint					fDragStart;
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
			case kFinderMoveToTrash:
				fView->MoveSelectionToTrash();
				return;
			case kFinderRefreshFolder:
				fView->Refresh();
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
		Window()->Activate(true);
		MakeFocus(true);
		desktop_item item = kNoDesktopItem;
		if (fDiskHitRect.Contains(where))
			item = kBootVolumeItem;
		else if (fTrashHitRect.Contains(where))
			item = kTrashItem;
		int32 buttons = B_PRIMARY_MOUSE_BUTTON;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("buttons", &buttons);

		if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
			fDiskSelected = item == kBootVolumeItem;
			fTrashSelected = item == kTrashItem;
			Invalidate();
			if (item != kNoDesktopItem)
				_ShowContextMenu(where);
			return;
		}
		if ((buttons & B_PRIMARY_MOUSE_BUTTON) == 0)
			return;

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

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kFinderDragEntries && message->WasDropped()) {
			BPoint point = message->DropPoint();
			ConvertFromScreen(&point);
			const char* targetPath = NULL;
			bool preserveExisting = false;
			if (fTrashHitRect.Contains(point)) {
				targetPath = "/boot/trash";
				preserveExisting = true;
			} else if (fDiskHitRect.Contains(point))
				targetPath = "/boot";

			if (targetPath != NULL) {
				BEntry target(targetPath);
				entry_ref targetRef;
				if (target.GetRef(&targetRef) == B_OK)
					TransferEntries(message, targetRef, preserveExisting);
			}
			return;
		}
		BView::MessageReceived(message);
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
	void _ShowContextMenu(BPoint where)
	{
		BPopUpMenu* menu = new BPopUpMenu("", false, false);
		menu->AddItem(new BMenuItem("Open", new BMessage(kOSTenOpen)));
		menu->SetTargetForItems(Window());
		menu->SetAsyncAutoDestruct(true);
		ConvertToScreen(&where);
		menu->Go(where, true, false, true);
	}

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
		if (message->what == kFinderRefreshAll) {
			for (int32 index = 0; BWindow* window = WindowAt(index); index++)
				window->PostMessage(kFinderRefreshFolder);
			return;
		}
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
