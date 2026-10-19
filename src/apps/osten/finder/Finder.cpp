/* SPDX-License-Identifier: MIT */

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <InterfaceDefs.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <Message.h>
#include <Messenger.h>
#include <Node.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <StorageDefs.h>
#include <String.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>
#include <Window.h>
#include <WindowPrivate.h>

#include <algorithm>
#include <mutex>
#include <atomic>
#include <math.h>
#include <string.h>
#include <vector>

#include "OSTenMessages.h"
#include "FileOperations.h"
#include "FinderActions.h"
#include "IconArtwork.h"


namespace {

const uint32 kOpenEntry = 'open';
const uint32 kFinderWindowActivated = 'oswa';
const uint32 kFinderDragEntries = 'osdg';
const uint32 kFinderMoveToTrash = 'ostr';
const uint32 kFinderRefreshAll = 'osrf';
const uint32 kFinderRefreshFolder = 'osrw';
const uint32 kFinderPopupTabClicked = 'ospt';
const uint32 kFinderPopupTabPlacement = 'ospp';
const uint32 kFinderRestorePopup = 'ospr';
static std::mutex sPopupSlotLock;
static std::vector<int32> sPopupSlotsInUse;
const uint32 kInlineRenameCommit = 'osic';
const uint32 kInlineRenameCancel = 'osix';
const uint32 kInlineRenameAfterClick = 'osid';
const char* kWindowFrameAttribute = "OSTen:window_frame";
const char* kPopupFolderAttribute = "OSTen:popup_folder";
const char* kArrangeAttribute = "OSTen:arrange_kind";
// Store desktop icon locations on their filesystem nodes, not by filename.
// Renaming an icon therefore does not reset its position.
const char* kDesktopPositionAttribute = "OSTen:desktop_position";

const float kCellWidth = 94;
const float kCellHeight = 86;
const float kIconTop = 8;
// Client-frame top must also accommodate the MacDecorator's title above it.
// The global System bar occupies the first 26 screen pixels.
const float kFinderClientMinTop = 54;
// Compact three-icon-wide classic Mac pop-up drawer, independent of its
// regular Finder window geometry.
const float kPopupDrawerWidth = 314;
const float kPopupDrawerHeight = 252;

// Finder-local Owner permission. Reset to locked whenever Finder restarts.
static std::atomic<bool> sSystemFolderUnlocked(false);



enum OSTenIcon {
    kFolderIcon = 0,
    kFinderIcon = 1,
    kSystemIcon = 2
};


// Decode the palette and indexed pixels embedded in IconArtwork.h. This
// keeps approved artwork inside the Finder executable without runtime paths.
static bool
DecodeBase64(const char* encoded, uint8* output, size_t expectedSize)
{
    uint32 bits = 0;
    int available = 0;
    size_t written = 0;
    for (const char* p = encoded; *p != '\0' && *p != '='; p++) {
        char value = *p;
        int digit;
        if (value >= 'A' && value <= 'Z')
            digit = value - 'A';
        else if (value >= 'a' && value <= 'z')
            digit = value - 'a' + 26;
        else if (value >= '0' && value <= '9')
            digit = value - '0' + 52;
        else if (value == '+')
            digit = 62;
        else if (value == '/')
            digit = 63;
        else
            return false;
        bits = (bits << 6) | digit;
        available += 6;
        if (available >= 8) {
            available -= 8;
            if (written >= expectedSize)
                return false;
            output[written++] = (bits >> available) & 0xff;
        }
    }
    return written == expectedSize;
}


static BBitmap*
CreateArtworkBitmap(OSTenIcon kind)
{
    uint8 palette[kOSTenArtworkColors * 4];
    uint8 pixels[kOSTenArtworkSize * kOSTenArtworkSize];
    const OSTenArtwork& artwork = kOSTenArtwork[(int)kind];
    if (!DecodeBase64(artwork.palette, palette, sizeof(palette))
        || !DecodeBase64(artwork.pixels, pixels, sizeof(pixels))) {
        return NULL;
    }

    BBitmap* bitmap = new BBitmap(BRect(0, 0,
        kOSTenArtworkSize - 1, kOSTenArtworkSize - 1), B_RGBA32);
    if (bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return NULL;
    }
    uint8* data = (uint8*)bitmap->Bits();
    const int32 stride = bitmap->BytesPerRow();
    for (int y = 0; y < kOSTenArtworkSize; y++) {
        for (int x = 0; x < kOSTenArtworkSize; x++) {
            uint8 index = pixels[y * kOSTenArtworkSize + x];
            if (index >= kOSTenArtworkColors) {
                delete bitmap;
                return NULL;
            }
            const uint8* rgba = palette + index * 4;
            uint8* bgra = data + y * stride + x * 4;
            bgra[0] = rgba[2];
            bgra[1] = rgba[1];
            bgra[2] = rgba[0];
            bgra[3] = rgba[3];
        }
    }
    return bitmap;
}


static const BBitmap*
ArtworkBitmap(OSTenIcon kind)
{
    static std::once_flag once;
    static BBitmap* bitmaps[3] = { NULL, NULL, NULL };
    std::call_once(once, []() {
        for (int index = 0; index < 3; index++)
            bitmaps[index] = CreateArtworkBitmap((OSTenIcon)index);
    });
    return bitmaps[(int)kind];
}

struct FinderItem {
	entry_ref	ref;
	BString		name;
	bool		isDirectory;
	bool		selected;
	BRect		cell;
};


static void
TransferEntries(const BMessage* message, const entry_ref& targetRef,
	bool preserveExisting = false)
{
	BDirectory targetDirectory(&targetRef);
	BEntry targetEntry;
	BPath targetPath;
	status_t firstError = targetDirectory.InitCheck();
	if (firstError == B_OK)
		firstError = targetDirectory.GetEntry(&targetEntry);
	if (firstError == B_OK)
		firstError = targetEntry.GetPath(&targetPath);

	BString recoveryPath;
	bool copy = false;
	message->FindBool("copy", &copy);
	for (int32 index = 0; firstError == B_OK; index++) {
		entry_ref sourceRef;
		if (message->FindRef("refs", index, &sourceRef) != B_OK)
			break;
		status_t error = TransferEntry(sourceRef, targetDirectory, targetPath,
			copy, preserveExisting, &recoveryPath,
			sSystemFolderUnlocked.load());
		if (error != B_OK)
			firstError = error;
	}

	if (firstError != B_OK) {
		BString explanation;
		if (firstError == B_PERMISSION_DENIED) {
			explanation = "The System Folder is locked.\n\n"
				"Use Special > System Folder Access... to unlock it.";
		} else {
			explanation = "The item could not be moved or copied.\n\n";
			explanation << strerror(firstError);
		}
		if (!recoveryPath.IsEmpty()) {
			explanation << "\n\nFiles were kept for recovery in:\n"
				<< recoveryPath;
		}
		(new BAlert("Finder", explanation.String(), "OK"))->Go();
	}
	be_app->PostMessage(kFinderRefreshAll);
}


// Use the same native text-entry control as the working Rename dialog.
// Its embedded BTextView gets keyboard focus through BTextControl::MakeFocus.
static filter_result
InlineRenameKeyFilter(BMessage* message, BHandler** target,
    BMessageFilter* filter)
{
    const char* bytes = NULL;
    if (message->FindString("bytes", &bytes) != B_OK
        || bytes == NULL || bytes[0] == '\0') {
        return B_DISPATCH_MESSAGE;
    }
    const char key = bytes[0];
    if (key != B_ENTER && key != B_RETURN && key != B_ESCAPE)
        return B_DISPATCH_MESSAGE;

    BView* input = target != NULL ? dynamic_cast<BView*>(*target) : NULL;
    BView* control = input != NULL ? input->Parent() : NULL;
    BView* folder = control != NULL ? control->Parent() : NULL;
    if (folder != NULL && folder->Window() != NULL) {
        // Do not synchronously SendMessage to our own window looper.
        folder->Window()->PostMessage(key == B_ESCAPE
            ? kInlineRenameCancel : kInlineRenameCommit, folder);
    }
    return B_SKIP_MESSAGE;
}


class InlineRenameEditor : public BTextControl {
public:
    InlineRenameEditor(BRect frame, const char* filename)
        :
        BTextControl(frame, "Inline filename", NULL, filename, NULL)
    {
        SetDivider(0);
        SetViewColor(255, 255, 255);
        BTextView* input = TextView();
        input->SetViewColor(255, 255, 255);
        input->SetLowColor(255, 255, 255);
        input->SetHighColor(0, 0, 0);
        input->SetWordWrap(false);
        input->SetMaxBytes(B_FILE_NAME_LENGTH - 1);
        input->AddFilter(new BMessageFilter(B_KEY_DOWN,
            InlineRenameKeyFilter));
    }
};


class FolderView : public BView {
public:
	FolderView(BRect frame, const entry_ref& directory, bool desktop = false)
		:
		BView(frame, "Folder icons", B_FOLLOW_ALL,
			B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
		fDirectory(directory),
		fIsBootVolumeRoot(false),
		fIsSystemFolder(false),
		fIsDesktop(desktop),
		fArrangeKind(false),
		fTrackingDrag(false),
		fPressedIndex(-1),
		fRenameEditor(NULL),
		fRenameIndex(-1),
        fRenameClickRunner(NULL),
        fPendingRenameIndex(-1),
        fRenameClickCandidate(-1),
        fRenameClickGeneration(0)
	{
		if (fIsDesktop) {
			SetViewColor(54, 104, 150);
			SetLowColor(54, 104, 150);
		} else {
			SetViewColor(255, 255, 255);
			SetLowColor(255, 255, 255);
		}
		BNode folderNode(&fDirectory);
		int32 arrangeKind = 0;
		if (folderNode.ReadAttr(kArrangeAttribute, B_INT32_TYPE, 0,
				&arrangeKind, sizeof(arrangeKind)) == (ssize_t)sizeof(arrangeKind))
			fArrangeKind = arrangeKind != 0;
		BEntry folder(&fDirectory);
		BPath path;
		if (folder.GetPath(&path) == B_OK) {
			fIsBootVolumeRoot = strcmp(path.Path(), "/boot") == 0;
			fIsSystemFolder = strcmp(path.Path(), "/boot/System Folder") == 0;
		}
		Refresh();
	}

    ~FolderView() override { delete fRenameClickRunner; }

	void AttachedToWindow() override
	{
		MakeFocus(true);
		_LayoutItems();
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
        _CancelPendingRename();
        fRenameClickCandidate = -1;
		if (numBytes == 1 && bytes[0] == B_ENTER) {
            BeginRenameSelected();
			return;
		}
		// Route Finder commands to the window that actually has keyboard focus.
		// The separate System menu bar must not decide which folder owns them.
		if (numBytes == 1 && (modifiers() & B_COMMAND_KEY) != 0) {
			uint32 command = 0;
			switch (bytes[0]) {
				case 'o': case 'O': command = kOSTenOpen; break;
				case 'n': case 'N': command = kOSTenNewFolder; break;
				case 'w': case 'W': command = kOSTenClose; break;
				case 'a': case 'A': command = kOSTenSelectAll; break;
			}
			if (command != 0) {
				Window()->PostMessage(command);
				return;
			}
		}
		BView::KeyDown(bytes, numBytes);
	}

	void MouseDown(BPoint where) override
	{
        // A double-click must outrun a pending single-click rename.
        _CancelPendingRename();
        fRenameClickCandidate = -1;
        if (fRenameEditor != NULL)
            _FinishInlineRename(true);
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
        int32 clicks = 1;
        if (Window()->CurrentMessage() != NULL)
            Window()->CurrentMessage()->FindInt32("clicks", &clicks);
        if (hit >= 0 && fItems[hit].selected && !extend && clicks == 1
            && where.y >= fItems[hit].cell.top + 50
            && where.y <= fItems[hit].cell.top + 70) {
            // Wait until the double-click window has passed, allowing
            // the same label to be used for opening and dragging.
            fRenameClickCandidate = hit;
        }

		if (!extend && (hit < 0 || !fItems[hit].selected))
			_ClearSelection();
		if (hit >= 0 && extend)
			fItems[hit].selected = !fItems[hit].selected;
		else if (hit >= 0)
			fItems[hit].selected = true;
		Invalidate();

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
        if (fRenameClickCandidate >= 0)
            _ScheduleInlineRename(fRenameClickCandidate);
        fRenameClickCandidate = -1;
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

        fRenameClickCandidate = -1;
        _CancelPendingRename();
		BMessage drag(kFinderDragEntries);
		_AddSelectedRefs(drag);
		drag.AddBool("copy", (modifiers() & B_OPTION_KEY) != 0);
		// A drag that returns to this desktop rearranges icons instead of
		// attempting to move their files back into the same directory.
		if (fIsDesktop) {
			drag.AddBool("osten:desktop-drag", true);
			drag.AddPoint("osten:drag-origin", fDragStart);
		}
		BRect dragRect = fItems[fPressedIndex].cell;
		fTrackingDrag = false;
		DragMessage(&drag, dragRect, this);
	}

	void MessageReceived(BMessage* message) override
	{
        if (message->what == kInlineRenameAfterClick) {
            int32 generation = -1;
            if (message->FindInt32("generation", &generation) == B_OK
                && generation == fRenameClickGeneration) {
                int32 index = fPendingRenameIndex;
                _CancelPendingRename();
                if (index >= 0 && (size_t)index < fItems.size()
                    && fItems[index].selected) {
                    _BeginInlineRename(index);
                }
            }
            return;
        }
        if (message->what == kInlineRenameCommit) {
            _FinishInlineRename(true);
            return;
        }
        if (message->what == kInlineRenameCancel) {
            _FinishInlineRename(false);
            return;
        }
		if (message->what == kFinderDragEntries && message->WasDropped()) {
			entry_ref target = fDirectory;
			BPoint point = message->DropPoint();
			ConvertFromScreen(&point);
			int32 hit = _ItemAt(point);
			// Dropping onto a different folder still transfers the files.
			// Dropping onto a dragged icon, or empty desktop, is a move.
			if (hit >= 0 && fItems[hit].isDirectory
				&& !_IsDraggedItem(message, fItems[hit].ref))
				target = fItems[hit].ref;
			if (fIsDesktop && target == fDirectory
				&& RepositionDroppedIcons(message))
				return;
			TransferEntries(message, target);
			return;
		}
		BView::MessageReceived(message);
	}

	void SetDirectory(const entry_ref& directory)
	{
		fDirectory = directory;
		fIsBootVolumeRoot = false;
		fIsSystemFolder = false;
		BEntry entry(&fDirectory);
		BPath path;
		if (entry.GetPath(&path) == B_OK) {
			fIsBootVolumeRoot = strcmp(path.Path(), "/boot") == 0;
			fIsSystemFolder = strcmp(path.Path(), "/boot/System Folder") == 0;
		}
		Refresh();
	}

	void Refresh(const char* selectName = NULL)
	{
        _CancelPendingRename();
        fRenameClickCandidate = -1;
        if (fRenameEditor != NULL) {
            RemoveChild(fRenameEditor);
            delete fRenameEditor;
            fRenameEditor = NULL;
            fRenameIndex = -1;
        }
		fTrackingDrag = false;
		fPressedIndex = -1;
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
			// The Desktop contains symlinks such as Home. Treat a link to a
			// folder as a folder, while preserving its original icon label.
			BEntry resolved(&ref, true);
			item.isDirectory = resolved.IsDirectory();
			item.selected = selectName != NULL && item.name == selectName;
			fItems.push_back(item);
		}

		std::sort(fItems.begin(), fItems.end(),
			[this](const FinderItem& left, const FinderItem& right) {
				if (fArrangeKind && left.isDirectory != right.isDirectory)
					return left.isDirectory;
				return left.name.ICompare(right.name) < 0;
			});
		_LayoutItems();
		Invalidate();
	}

	bool BeginRenameSelected()
    {
        _CancelPendingRename();
        fRenameClickCandidate = -1;
        if (fRenameEditor != NULL)
            return true;
        std::vector<int32> indices = SelectedIndices();
        if (indices.size() != 1)
            return false;
        _BeginInlineRename(indices[0]);
        return true;
    }

	void SelectAll()
	{
		for (size_t index = 0; index < fItems.size(); index++)
			fItems[index].selected = true;
		Invalidate();
	}

	void Arrange(bool byKind)
	{
		// "Clean Up" / "Arrange" really restores the desktop grid, instead
		// of simply refreshing icons that were never free to move.
		if (fIsDesktop) {
			for (size_t i = 0; i < fItems.size(); ++i) {
				BNode node(&fItems[i].ref);
				if (node.InitCheck() == B_OK)
					node.RemoveAttr(kDesktopPositionAttribute);
			}
		}
		fArrangeKind = byKind;
		int32 value = byKind ? 1 : 0;
		BNode folder(&fDirectory);
		folder.WriteAttr(kArrangeAttribute, B_INT32_TYPE, 0,
			&value, sizeof(value));
		Refresh();
	}


	// Called both by this desktop FolderView and its parent DesktopView,
	// which receives drops in the reserved OSTen-volume / Trash column.
	bool RepositionDroppedIcons(BMessage* message)
	{
		bool fromDesktop = false;
		BPoint dragOrigin;
		if (!fIsDesktop || !message->WasDropped()
			|| message->FindBool("osten:desktop-drag", &fromDesktop) != B_OK
			|| !fromDesktop
			|| message->FindPoint("osten:drag-origin", &dragOrigin) != B_OK)
			return false;
		bool copy = false;
		message->FindBool("copy", &copy);
		if (copy)
			return false;

		BPoint dropped = message->DropPoint();
		ConvertFromScreen(&dropped);
		BPoint delta = dropped - dragOrigin;
		bool changed = false;
		for (int32 i = 0; ; ++i) {
			entry_ref ref;
			if (message->FindRef("refs", i, &ref) != B_OK)
				break;
			for (size_t index = 0; index < fItems.size(); ++index) {
				FinderItem& item = fItems[index];
				if (item.ref != ref)
					continue;
				BPoint position(item.cell.left + delta.x,
					item.cell.top + delta.y);
				position = _ConstrainDesktopPosition(position);
				BNode node(&item.ref);
				if (node.InitCheck() == B_OK
					&& node.WriteAttr(kDesktopPositionAttribute, B_POINT_TYPE,
						0, &position, sizeof(position)) == (ssize_t)sizeof(position)) {
					item.cell.OffsetTo(position);
					changed = true;
				}
				break;
			}
		}
		if (changed)
			Invalidate();
		return true;
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
    void _CancelPendingRename()
    {
        delete fRenameClickRunner;
        fRenameClickRunner = NULL;
        fPendingRenameIndex = -1;
        ++fRenameClickGeneration;
    }

    void _ScheduleInlineRename(int32 index)
    {
        if (index < 0 || (size_t)index >= fItems.size()
            || !fItems[index].selected || fRenameEditor != NULL) {
            return;
        }
        _CancelPendingRename();
        fPendingRenameIndex = index;
        BMessage message(kInlineRenameAfterClick);
        message.AddInt32("generation", fRenameClickGeneration);
        bigtime_t clickSpeed = 500000;
        get_click_speed(&clickSpeed);
        fRenameClickRunner = new BMessageRunner(BMessenger(this), &message,
            clickSpeed + 100000, 1);
        if (fRenameClickRunner->InitCheck() != B_OK)
            _CancelPendingRename();
    }

    void _BeginInlineRename(int32 index)
    {
        _CancelPendingRename();
        if (fRenameEditor != NULL || index < 0
            || (size_t)index >= fItems.size()) {
            return;
        }
        const FinderItem& item = fItems[index];
        fRenameRef = item.ref;
        fRenameIndex = index;
        BRect editor(item.cell.left - 10, item.cell.top + 49,
            item.cell.right + 10, item.cell.top + 73);
        fRenameEditor = new InlineRenameEditor(editor, item.name.String());
        AddChild(fRenameEditor);
        if (!Window()->IsActive())
            Window()->Activate(true);
        // BTextControl focuses its embedded editable text view and selects
        // the current filename, as in Haiku's standard text-entry dialogs.
        fRenameEditor->MakeFocus(true);
        Invalidate(item.cell);
    }

    void _FinishInlineRename(bool save)
    {
        if (fRenameEditor == NULL)
            return;
        BString filename(fRenameEditor->Text());
        entry_ref target = fRenameRef;
        BString previous;
        if (fRenameIndex >= 0 && (size_t)fRenameIndex < fItems.size())
            previous = fItems[fRenameIndex].name;
        RemoveChild(fRenameEditor);
        delete fRenameEditor;
        fRenameEditor = NULL;
        fRenameIndex = -1;
        MakeFocus(true);
        if (!save) {
            Invalidate();
            return;
        }
        status_t error = RenameFinderEntry(target, filename.String(),
            sSystemFolderUnlocked.load());
        if (error != B_OK) {
            BString details("The item could not be renamed.\n\n");
            if (error == B_BAD_VALUE)
                details << "Choose a nonempty name without slashes.";
            else if (error == B_PERMISSION_DENIED)
                details << "The System Folder is locked. Unlock it first.";
            else if (error == B_FILE_EXISTS)
                details << "An item with this name already exists.";
            else
                details << strerror(error);
            (new BAlert("Rename", details.String(), "OK"))->Go();
            Refresh(previous.String());
            return;
        }
        Refresh(filename.String());
        be_app->PostMessage(kFinderRefreshAll);
    }

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
			menu->AddItem(new BMenuItem("Rename...",
				new BMessage(kOSTenRename)));
			menu->AddItem(new BMenuItem("Get Info",
				new BMessage(kOSTenGetInfo)));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Move to Trash",
				new BMessage(kFinderMoveToTrash)));
		} else {
			menu->AddItem(new BMenuItem("New Folder",
				new BMessage(kOSTenNewFolder)));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Select All",
				new BMessage(kOSTenSelectAll)));
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Arrange by Name",
				new BMessage(kOSTenArrangeName)));
			menu->AddItem(new BMenuItem("Arrange by Kind",
				new BMessage(kOSTenArrangeKind)));
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
		return strcmp(name, "_packages_") == 0
			|| strcmp(name, "data") == 0
			|| strcmp(name, "home") == 0
			|| strcmp(name, "system") == 0
			|| strcmp(name, "trash") == 0;
	}

	BPoint _ConstrainDesktopPosition(BPoint point) const
	{
		// Positions are relative to the desktop icon field. The system disk
		// and Trash keep their reserved column on the right.
		float maxX = std::max(0.0f, Bounds().Width() - (kCellWidth - 8));
		float maxY = std::max(0.0f, Bounds().Height() - (kCellHeight - 6));
		point.x = std::max(0.0f, std::min(point.x, maxX));
		point.y = std::max(0.0f, std::min(point.y, maxY));
		return point;
	}

	bool _IsDraggedItem(const BMessage* message, const entry_ref& ref) const
	{
		for (int32 i = 0; ; ++i) {
			entry_ref dragged;
			if (message->FindRef("refs", i, &dragged) != B_OK)
				return false;
			if (dragged == ref)
				return true;
		}
	}

	void _LayoutItems()
	{
		int32 columns = (int32)((Bounds().Width() - 12) / kCellWidth);
		if (columns < 1)
			columns = 1;

		if (fIsDesktop) {
			// Place manually arranged icons first. Unpositioned/new folders
			// take the first vacant grid cells, left-to-right, top-to-bottom.
			std::vector<BRect> occupied;
			std::vector<bool> positioned(fItems.size(), false);
			for (size_t i = 0; i < fItems.size(); ++i) {
				BPoint point;
				BNode node(&fItems[i].ref);
				if (node.InitCheck() != B_OK
					|| node.ReadAttr(kDesktopPositionAttribute, B_POINT_TYPE,
						0, &point, sizeof(point)) != (ssize_t)sizeof(point))
					continue;
				// Ignore corrupted coordinates; never draw an icon offscreen.
				if (!isfinite(point.x) || !isfinite(point.y))
					continue;
				point = _ConstrainDesktopPosition(point);
				BRect cell(point.x, point.y, point.x + kCellWidth - 8,
					point.y + kCellHeight - 6);
				fItems[i].cell = cell;
				occupied.push_back(cell);
				positioned[i] = true;
			}
			int32 slot = 0;
			for (size_t i = 0; i < fItems.size(); ++i) {
				if (positioned[i])
					continue;
				for (;;) {
					int32 column = slot % columns;
					int32 row = slot / columns;
					++slot;
					BPoint p(10 + column * kCellWidth,
						8 + row * kCellHeight);
					p = _ConstrainDesktopPosition(p);
					BRect cell(p.x, p.y, p.x + kCellWidth - 8,
						p.y + kCellHeight - 6);
					bool taken = false;
					for (size_t j = 0; j < occupied.size(); ++j) {
						if (cell.Intersects(occupied[j])) {
							taken = true;
							break;
						}
					}
					// A packed desktop still needs to display every entry.
					if (!taken || slot > (int32)fItems.size() + columns * 2) {
						fItems[i].cell = cell;
						occupied.push_back(cell);
						break;
					}
				}
			}
		} else {
			for (size_t index = 0; index < fItems.size(); index++) {
				int32 column = (int32)index % columns;
				int32 row = (int32)index / columns;
				float left = 10 + column * kCellWidth;
				float top = 8 + row * kCellHeight;
				fItems[index].cell.Set(left, top, left + kCellWidth - 8,
					top + kCellHeight - 6);
			}
		}
		BScrollBar* scrollbar = ScrollBar(B_VERTICAL);
		if (scrollbar != NULL) {
			float rows = (fItems.size() + columns - 1) / columns;
			float contentHeight = 12 + rows * kCellHeight;
			float viewportHeight = Bounds().Height();
			float overflow = std::max(0.0f, contentHeight - viewportHeight);
			scrollbar->SetRange(0, overflow);
			scrollbar->SetProportion(contentHeight > 0
				? std::min(1.0f, viewportHeight / contentHeight) : 1.0f);
			scrollbar->SetSteps(kCellHeight / 3, kCellHeight);
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
		BRect icon(item.cell.left + 23, item.cell.top + kIconTop,
			item.cell.left + 63, item.cell.top + kIconTop + 37);
		if (item.isDirectory) {
			if (!_DrawArtworkIcon(icon, kFolderIcon))
				_DrawFolderIcon(icon);
		} else if (fIsSystemFolder && item.name == "Finder") {
			if (!_DrawArtworkIcon(icon, kFinderIcon))
				_DrawFileIcon(icon);
		} else if (fIsSystemFolder && item.name == "System") {
			if (!_DrawArtworkIcon(icon, kSystemIcon))
				_DrawFileIcon(icon);
		} else
			_DrawFileIcon(icon);

		if (fRenameEditor != NULL && fRenameIndex >= 0
            && &item == &fItems[fRenameIndex])
            return;
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
		} else if (fIsDesktop)
			SetHighColor(255, 255, 255);
		else
			SetHighColor(0, 0, 0);
		DrawString(display.String(), BPoint(textX, baseline));
	}


    bool _DrawArtworkIcon(BRect rect, OSTenIcon kind)
    {
        const BBitmap* bitmap = ArtworkBitmap(kind);
        if (bitmap == NULL)
            return false;
        SetDrawingMode(B_OP_ALPHA);
        SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
        DrawBitmap(bitmap, bitmap->Bounds(), rect);
        SetDrawingMode(B_OP_COPY);
        return true;
    }

	// Classic Finder folders use a clear tab, a pale face, and a narrow
	// shaded edge so the shape stays readable at the small icon size.
	void _DrawFolderIcon(BRect rect)
	{
		BRect body(rect.left + 2, rect.top + 8, rect.right - 1,
			rect.bottom - 2);
		BRect tab(rect.left + 3, rect.top + 3, rect.left + 20,
			rect.top + 11);

		SetHighColor(41, 88, 148);
		FillRect(body);
		FillRect(tab);

		SetHighColor(90, 159, 225);
		FillRect(BRect(rect.left + 4, rect.top + 10, rect.right - 3,
			rect.bottom - 4));
		FillRect(BRect(rect.left + 5, rect.top + 5, rect.left + 18,
			rect.top + 9));

		SetHighColor(205, 235, 255);
		FillRect(BRect(rect.left + 5, rect.top + 11, rect.right - 5,
			rect.top + 13));
		SetHighColor(141, 195, 247);
		FillRect(BRect(rect.left + 5, rect.top + 14, rect.right - 5,
			rect.bottom - 7));
		SetHighColor(66, 134, 210);
		FillRect(BRect(rect.right - 5, rect.top + 13, rect.right - 3,
			rect.bottom - 4));
		FillRect(BRect(rect.left + 4, rect.bottom - 5, rect.right - 3,
			rect.bottom - 4));

		SetHighColor(55, 107, 176);
		StrokeLine(BPoint(rect.left + 4, rect.top + 9),
			BPoint(rect.left + 19, rect.top + 9));
		StrokeLine(BPoint(rect.left + 20, rect.top + 10),
			BPoint(rect.right - 3, rect.top + 10));
	}

	// A folded-corner paper icon is the neutral Macintosh document symbol.
	void _DrawFileIcon(BRect rect)
	{
		SetHighColor(48, 42, 31);
		FillRect(BRect(rect.left + 6, rect.top + 1, rect.right - 5,
			rect.bottom - 1));
		SetHighColor(249, 247, 237);
		FillRect(BRect(rect.left + 8, rect.top + 3, rect.right - 7,
			rect.bottom - 3));
		SetHighColor(207, 202, 186);
		FillRect(BRect(rect.right - 13, rect.top + 3, rect.right - 8,
			rect.top + 8));
		SetHighColor(48, 42, 31);
		StrokeLine(BPoint(rect.right - 13, rect.top + 3),
			BPoint(rect.right - 13, rect.top + 8));
		StrokeLine(BPoint(rect.right - 13, rect.top + 8),
			BPoint(rect.right - 8, rect.top + 8));

		SetHighColor(139, 133, 117);
		StrokeLine(BPoint(rect.left + 11, rect.top + 13),
			BPoint(rect.right - 11, rect.top + 13));
		StrokeLine(BPoint(rect.left + 11, rect.top + 18),
			BPoint(rect.right - 11, rect.top + 18));
		StrokeLine(BPoint(rect.left + 11, rect.top + 23),
			BPoint(rect.right - 13, rect.top + 23));
		StrokeLine(BPoint(rect.left + 11, rect.top + 28),
			BPoint(rect.right - 11, rect.top + 28));
	}

private:
	entry_ref				fDirectory;
	bool					fIsBootVolumeRoot;
	bool					fIsSystemFolder;
	bool					fIsDesktop;
	bool					fArrangeKind;
	bool					fTrackingDrag;
	int32					fPressedIndex;
	BPoint					fDragStart;
	std::vector<FinderItem>	fItems;
    InlineRenameEditor* fRenameEditor;
    int32 fRenameIndex;
    entry_ref fRenameRef;
    BMessageRunner* fRenameClickRunner;
    int32 fPendingRenameIndex;
    int32 fRenameClickCandidate;
    int32 fRenameClickGeneration;
};



static void
ShowSelectedInfo(FolderView* view)
{
    if (view == NULL)
        return;
    std::vector<int32> indices = view->SelectedIndices();
    entry_ref ref;
    if (indices.size() == 1 && view->EntryAt(indices[0], ref))
        ShowFinderInfo(ref);
}


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
	screen.top = kFinderClientMinTop;
	screen.bottom -= 8;
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


// Pop-up folders have an independent, clickable bottom tab. Keeping the
// actual folder at its normal dimensions avoids WindowShade's long frame.
class FinderPopupTabView : public BView {
public:
	FinderPopupTabView(BRect frame, const char* title,
		const BMessenger& folder)
		:
		BView(frame, "popup-folder-tab", B_FOLLOW_ALL, B_WILL_DRAW),
		fTitle(title), fFolder(folder)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	void Draw(BRect update) override
	{
		BRect r = Bounds();
		SetHighColor(215, 215, 215);
		FillRoundRect(r, 6, 6);
		FillRect(BRect(r.left, r.top + 6, r.right, r.bottom));
		SetHighColor(40, 40, 40);
		StrokeLine(BPoint(r.left, r.bottom), BPoint(r.left, r.top + 6));
		StrokeArc(BRect(r.left, r.top, r.left + 12, r.top + 12), 90, 90);
		StrokeLine(BPoint(r.left + 6, r.top), BPoint(r.right - 6, r.top));
		StrokeArc(BRect(r.right - 12, r.top, r.right, r.top + 12), 0, 90);
		StrokeLine(BPoint(r.right, r.top + 6), r.RightBottom());
		SetHighColor(255, 255, 255);
		StrokeLine(BPoint(2, r.bottom - 1), BPoint(2, 7));
		SetHighColor(45, 45, 45);
		DrawString(fTitle.String(), BPoint(7, 13));
	}

	void MouseDown(BPoint where) override
	{
		fFolder.SendMessage(kFinderPopupTabClicked);
	}

private:
	BString fTitle;
	BMessenger fFolder;
};


class FinderPopupTab : public BWindow {
public:
	FinderPopupTab(const char* title, const BMessenger& folder)
		:
		BWindow(BRect(0, 0, 105, 17), title, B_NO_BORDER_WINDOW_LOOK,
			B_NORMAL_WINDOW_FEEL,
			B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_NOT_MINIMIZABLE
				| B_ASYNCHRONOUS_CONTROLS),
		fFolder(folder)
	{
		AddChild(new FinderPopupTabView(Bounds(), title, fFolder));
		BRect screen = BScreen(this).Frame();
		int32 slot = 0;
		{
			std::lock_guard<std::mutex> guard(sPopupSlotLock);
			while (std::find(sPopupSlotsInUse.begin(),
				sPopupSlotsInUse.end(), slot) != sPopupSlotsInUse.end())
				slot++;
			sPopupSlotsInUse.push_back(slot);
		}
		fSlot = slot;
		int32 columns = std::max(1, (int32)(screen.Width() / 112));
		int32 column = slot % columns;
		int32 row = slot / columns;
		MoveTo(screen.left + 6 + column * 112,
			screen.bottom - 20 - row * 21);
		fCollapsedPosition = Frame().LeftTop();
	}

	~FinderPopupTab() override
	{
		std::lock_guard<std::mutex> guard(sPopupSlotLock);
		std::vector<int32>::iterator it = std::find(sPopupSlotsInUse.begin(),
			sPopupSlotsInUse.end(), fSlot);
		if (it != sPopupSlotsInUse.end())
			sPopupSlotsInUse.erase(it);
	}

	BPoint CollapsedPosition() const { return fCollapsedPosition; }

	void MessageReceived(BMessage* message) override
	{
		if (message->what == kFinderPopupTabPlacement) {
			BPoint p;
			if (message->FindPoint("position", &p) == B_OK)
				MoveTo(p);
			return;
		}
		if (message->what == kFinderPopupTabClicked) {
			fFolder.SendMessage(kFinderPopupTabClicked);
			return;
		}
		BWindow::MessageReceived(message);
	}

private:
	BMessenger fFolder;
	BPoint fCollapsedPosition;
	int32 fSlot;
};


class FinderWindow : public BWindow {
public:
	FinderWindow(const entry_ref& directory, int32 depth,
		const char* displayName = NULL)
		:
		BWindow(FolderWindowFrame(directory, depth), "Finder",
			B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS),
		fDirectory(directory),
		fDepth(depth),
		fView(NULL),
		fShaded(false),
		fUnshadedHeight(0),
		fPopupDocked(false),
		fPopupMoving(false),
		fPopupTab(NULL),
		fZoomed(false)
	{
		BEntry folder(&directory);
		char title[B_FILE_NAME_LENGTH];
		if (displayName != NULL)
			SetTitle(displayName);
		else if (folder.GetName(title) == B_OK)
			SetTitle(title);

		BRect frame = Bounds();
		frame.InsetBy(7, 7);
		// Leave a classic Mac footer beneath the vertical scrollbar. Its
		// lower-right corner belongs to the Platinum window-size box, not
		// to the scroll view. B_FOLLOW_ALL retains the space while resizing.
		frame.bottom -= 16;
		fView = new FolderView(frame, directory);
		AddChild(new BScrollView("Folder scrolling", fView, B_FOLLOW_ALL,
			0, false, true, B_NO_BORDER));

		BRect footer(7, frame.bottom + 1, Bounds().right - 7,
			Bounds().bottom - 7);
		BView* footerView = new BView(footer, "Finder lower inset",
			B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM, B_WILL_DRAW);
		footerView->SetViewColor(255, 255, 255);
		AddChild(footerView);

		AddShortcut('O', B_COMMAND_KEY, new BMessage(kOSTenOpen));
		AddShortcut('N', B_COMMAND_KEY, new BMessage(kOSTenNewFolder));
		AddShortcut('W', B_COMMAND_KEY, new BMessage(kOSTenClose));
		AddShortcut('A', B_COMMAND_KEY, new BMessage(kOSTenSelectAll));
		AddShortcut('I', B_COMMAND_KEY, new BMessage(kOSTenGetInfo));
		// Restore a previously docked folder in its collapsed state.
		BNode state(&fDirectory);
		uint8 docked = 0;
		if (state.ReadAttr(kPopupFolderAttribute, B_BOOL_TYPE, 0,
				&docked, sizeof(docked)) == (ssize_t)sizeof(docked)
			&& docked != 0)
			PostMessage(kFinderRestorePopup);
	}

	void FrameMoved(BPoint newPosition) override
	{
		// The native MacDecorator draws the title above the client frame.
		// Do not let it disappear behind the permanent global menu.
		if (!fPopupDocked && newPosition.y < kFinderClientMinTop) {
			MoveTo(newPosition.x, kFinderClientMinTop);
			return;
		}
		BWindow::FrameMoved(newPosition);
		if (!fPopupDocked && !fPopupMoving)
			_SaveFrame();
	}

	void FrameResized(float width, float height) override
	{
		BWindow::FrameResized(width, height);
		if (!fPopupDocked)
			_SaveFrame();
	}

	// Classic Mac WindowShade keeps a thin client strip beneath the title,
	// including the Platinum resize corner. A one-pixel-high client reduces
	// the window to a naked tab and loses the characteristic shaded outline.
	void Minimize(bool minimize) override
	{
		if (!minimize && !fShaded)
			return;
		if (fShaded) {
			fShaded = false;
			ResizeTo(Frame().Width(), fUnshadedHeight);
		} else {
			fUnshadedHeight = Frame().Height();
			fShaded = true;
			ResizeTo(Frame().Width(), 26);
		}
	}

	// BWindow's nonvirtual Zoom() detects the second click by comparing
	// against its own fullscreen dimensions. Because OSTen reserves space for
	// the global menu, that comparison never matches our actual zoom frame.
	// Track the applied frame explicitly so the zoom button can restore it.
	void Zoom(BPoint origin, float width, float height) override
	{
		if (fShaded)
			Minimize(false);
		BRect current = Frame();
		if (fZoomed
			&& fabsf(current.Width() - fZoomFrame.Width()) < 1.5f
			&& fabsf(current.Height() - fZoomFrame.Height()) < 1.5f) {
			fZoomed = false;
			BWindow::Zoom(fUnzoomFrame.LeftTop(), fUnzoomFrame.Width(),
				fUnzoomFrame.Height());
			return;
		}
		fUnzoomFrame = current;
		BRect screen = BScreen(this).Frame();
		origin.y = std::max(origin.y, screen.top + kFinderClientMinTop);
		origin.x = std::max(origin.x, screen.left + 7);
		float allowedWidth = std::max(280.0f,
			screen.right - origin.x - 8);
		float allowedHeight = std::max(180.0f,
			screen.bottom - origin.y - 8);
		BWindow::Zoom(origin, std::min(width, allowedWidth),
			std::min(height, allowedHeight));
		fZoomFrame = Frame();
		fZoomed = true;
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
		if (fPopupTab != NULL && fPopupTab->Lock())
			fPopupTab->Quit();
		fPopupTab = NULL;
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
			case kFinderRestorePopup:
				_SetPopupDocked(true, false);
				return;
			case kFinderPopupTabClicked:
				if (fPopupDocked) {
					if (IsHidden()) {
						_AnchorPopupWindow();
						_PositionPopupTab(true);
						Show();
						Activate(true);
					} else {
						Hide();
						_PositionPopupTab(false);
					}
				}
				return;
			case kOSTenTogglePopupFolder:
				_SetPopupDocked(!fPopupDocked, true);
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
			case kOSTenRename:
				fView->BeginRenameSelected();
				return;
			case kOSTenGetInfo:
				ShowSelectedInfo(fView);
				return;
			case kOSTenArrangeName:
			case kOSTenCleanUp:
				fView->Arrange(false);
				return;
			case kOSTenArrangeKind:
				fView->Arrange(true);
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
		BEntry entry(&ref, true);
		if (entry.IsDirectory()) {
			entry_ref resolvedRef;
			if (entry.GetRef(&resolvedRef) == B_OK)
				(new FinderWindow(resolvedRef, fDepth + 1))->Show();
		} else
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
		BEntry current(&fDirectory);
		BPath currentPath;
		if (!sSystemFolderUnlocked.load()
			&& current.GetPath(&currentPath) == B_OK
			&& IsOSTenSystemFolderPath(currentPath)) {
			(new BAlert("System Folder",
				"The System Folder is locked. Use Special > "
				"System Folder Access... to unlock it.", "OK"))->Go();
			return;
		}
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

	void _PositionPopupTab(bool expanded)
	{
		if (fPopupTab == NULL)
			return;
		BMessage place(kFinderPopupTabPlacement);
		BRect screen = BScreen(this).Frame();
		BPoint position;
		if (expanded)
			position = BPoint(Frame().left - 1, Frame().top - 18);
		else {
			if (!fPopupTab->Lock())
				return;
			position = fPopupTab->CollapsedPosition();
			fPopupTab->Unlock();
		}
		place.AddPoint("position", position);
		BMessenger(fPopupTab).SendMessage(&place);
	}

	void _AnchorPopupWindow()
	{
		if (fPopupTab == NULL || !fPopupTab->Lock())
			return;
		BRect tab = fPopupTab->Frame();
		fPopupTab->Unlock();
		BRect screen = BScreen(this).Frame();
		float left = std::max(screen.left + 7,
			std::min(tab.left, screen.right - Frame().Width() - 7));
		float top = std::max(kFinderClientMinTop,
			screen.bottom - 7 - Frame().Height());
		fPopupMoving = true;
		MoveTo(left, top);
		fPopupMoving = false;
	}

	void _SetPopupDocked(bool docked, bool persist)
	{
		if (fPopupDocked == docked)
			return;
		if (docked) {
			if (fShaded)
				Minimize(false);
			fPopupNormalFrame = Frame();
			fPopupDocked = true;
			SetLook(B_BORDERED_WINDOW_LOOK);
			fPopupMoving = true;
			ResizeTo(kPopupDrawerWidth, kPopupDrawerHeight);
			fPopupMoving = false;
			fPopupTab = new FinderPopupTab(Title(), BMessenger(this));
			fPopupTab->Show();
			_AnchorPopupWindow();
			Hide();
		} else {
			fPopupDocked = false;
			SetLook(B_TITLED_WINDOW_LOOK);
			if (fPopupTab != NULL && fPopupTab->Lock())
				fPopupTab->Quit();
			fPopupTab = NULL;
			if (fPopupNormalFrame.IsValid()) {
				fPopupMoving = true;
				ResizeTo(fPopupNormalFrame.Width(), fPopupNormalFrame.Height());
				MoveTo(fPopupNormalFrame.LeftTop());
				fPopupMoving = false;
			}
			if (IsHidden())
				Show();
			Activate(true);
		}
		if (persist) {
			BNode node(&fDirectory);
			uint8 value = fPopupDocked ? 1 : 0;
			node.WriteAttr(kPopupFolderAttribute, B_BOOL_TYPE, 0,
				&value, sizeof(value));
			_SaveFrame();
		}
	}

	void _SaveFrame()
	{
		BNode node(&fDirectory);
		BRect frame = fPopupDocked && fPopupNormalFrame.IsValid()
			? fPopupNormalFrame : Frame();
		if (!fPopupDocked && fShaded)
			frame.bottom = frame.top + fUnshadedHeight;
		node.WriteAttr(kWindowFrameAttribute, B_RECT_TYPE, 0, &frame,
			sizeof(BRect));
	}

private:
	entry_ref	fDirectory;
	int32		fDepth;
	FolderView*	fView;
	bool		fShaded;
	float		fUnshadedHeight;
	bool		fPopupDocked;
	bool		fPopupMoving;
	FinderPopupTab* fPopupTab;
	BRect		fPopupNormalFrame;
	bool		fZoomed;
	BRect		fUnzoomFrame;
	BRect		fZoomFrame;
};



static void
OpenFinderEntry(const entry_ref& ref, int32 depth)
{
    BEntry entry(&ref, true);
    if (entry.IsDirectory()) {
        entry_ref resolvedRef;
        if (entry.GetRef(&resolvedRef) == B_OK)
            (new FinderWindow(resolvedRef, depth))->Show();
    } else {
        status_t error = be_roster->Launch(&ref);
        if (error != B_OK) {
            BString reason("The file could not be opened.\n\n");
            reason << strerror(error);
            (new BAlert("Finder", reason.String(), "OK"))->Go();
        }
    }
}


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
		// Route Finder commands to the window that actually has keyboard focus.
		// The separate System menu bar must not decide which folder owns them.
		if (numBytes == 1 && (modifiers() & B_COMMAND_KEY) != 0) {
			uint32 command = 0;
			switch (bytes[0]) {
				case 'o': case 'O': command = kOSTenOpen; break;
				case 'n': case 'N': command = kOSTenNewFolder; break;
				case 'w': case 'W': command = kOSTenClose; break;
				case 'a': case 'A': command = kOSTenSelectAll; break;
			}
			if (command != 0) {
				Window()->PostMessage(command);
				return;
			}
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
				_ShowContextMenu(where, item);
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
			} else {
				// Drops to the rest of the desktop (outside FolderView) should
				// still permit repositioning. Only disk/Trash remain targets.
				FolderView* icons = dynamic_cast<FolderView*>(
					FindView("Folder icons"));
				if (icons == NULL || !icons->RepositionDroppedIcons(message)) {
					BEntry desktop("/boot/home/Desktop");
					entry_ref desktopRef;
					if (desktop.GetRef(&desktopRef) == B_OK)
						TransferEntries(message, desktopRef);
				}
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
	void _ShowContextMenu(BPoint where, desktop_item item)
	{
		BPopUpMenu* menu = new BPopUpMenu("", false, false);
		menu->AddItem(new BMenuItem("Open", new BMessage(kOSTenOpen)));
		if (item == kTrashItem) {
			menu->AddSeparatorItem();
			menu->AddItem(new BMenuItem("Empty Trash...",
				new BMessage(kOSTenEmptyTrash)));
		}
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

	// The startup volume is drawn as a compact beige external hard drive.
	void _DrawDisk(BRect rect, bool selected)
	{
		SetHighColor(48, 42, 31);
		FillRoundRect(rect, 3, 3);
		BRect caseFace = rect.InsetByCopy(1, 1);
		SetHighColor(227, 224, 211);
		FillRoundRect(caseFace, 2, 2);
		SetHighColor(250, 248, 238);
		FillRect(BRect(caseFace.left + 2, caseFace.top + 2,
			caseFace.right - 2, caseFace.top + 7));
		SetHighColor(183, 180, 167);
		FillRect(BRect(caseFace.left + 2, caseFace.top + 9,
			caseFace.right - 2, caseFace.bottom - 2));
		SetHighColor(91, 89, 79);
		StrokeLine(BPoint(caseFace.left + 2, caseFace.top + 8),
			BPoint(caseFace.right - 2, caseFace.top + 8));
		StrokeLine(BPoint(caseFace.left + 3, caseFace.bottom - 5),
			BPoint(caseFace.right - 3, caseFace.bottom - 5));
		SetHighColor(45, 53, 47);
		FillRect(BRect(caseFace.right - 7, caseFace.bottom - 4,
			caseFace.right - 4, caseFace.bottom - 2));
		SetHighColor(112, 137, 91);
		FillRect(BRect(caseFace.right - 6, caseFace.bottom - 4,
			caseFace.right - 5, caseFace.bottom - 3));
		_DrawLabel("OSTen", fDiskHitRect, selected);
	}

	// A wire basket and overhanging lid give Trash its familiar Finder shape.
	void _DrawTrash(BRect rect, bool selected)
	{
		SetHighColor(48, 42, 31);
		FillRect(BRect(rect.left + 2, rect.top + 7, rect.right - 2,
			rect.bottom - 1));
		SetHighColor(225, 222, 207);
		FillRect(BRect(rect.left + 4, rect.top + 9, rect.right - 4,
			rect.bottom - 3));
		SetHighColor(48, 42, 31);
		FillRect(BRect(rect.left, rect.top + 5, rect.right, rect.top + 7));
		FillRect(BRect(rect.left + 7, rect.top + 2, rect.right - 7,
			rect.top + 4));
		FillRect(BRect(rect.left + 10, rect.top, rect.right - 10,
			rect.top + 1));

		SetHighColor(126, 123, 110);
		for (float x = rect.left + 7; x <= rect.right - 7; x += 5)
			StrokeLine(BPoint(x, rect.top + 11), BPoint(x, rect.bottom - 4));
		for (float y = rect.top + 13; y <= rect.bottom - 5; y += 5)
			StrokeLine(BPoint(rect.left + 5, y), BPoint(rect.right - 5, y));
		SetHighColor(250, 248, 238);
		StrokeLine(BPoint(rect.left + 4, rect.top + 9),
			BPoint(rect.left + 4, rect.bottom - 4));
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
		fView(new DesktopView(Bounds())),
		fFolderView(NULL)
	{
		AddChild(fView);
		// Desktop icons are backed by the normal, visible Desktop directory.
		BEntry desktopDirectory("/boot/home/Desktop");
		entry_ref desktopRef;
		if (desktopDirectory.GetRef(&desktopRef) == B_OK) {
			BRect files(6, 28, Bounds().right - 117,
				Bounds().bottom - 14);
			fFolderView = new FolderView(files, desktopRef, true);
			// The full-screen DesktopView would intercept clicks before a
			// sibling FolderView, even though the folder icons draw on top.
			// Nest the folder icons so hit-testing reaches them first.
			fView->AddChild(fFolderView);
		}
	}

	void WindowActivated(bool active) override
	{
		BWindow::WindowActivated(active);
		if (active)
			NotifyFinderWindowActivated(this);
	}

	bool QuitRequested() override
	{
		// The desktop cannot be closed interactively (B_NOT_CLOSABLE),
		// but it must honor the registrar's orderly shutdown request.
		return true;
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
			case kOpenEntry:
			{
				int32 index;
				entry_ref ref;
				if (fFolderView != NULL
					&& message->FindInt32("index", &index) == B_OK
					&& fFolderView->EntryAt(index, ref))
					OpenFinderEntry(ref, 0);
				return;
			}
			case kOSTenEmptyTrash:
				be_app->PostMessage(kOSTenEmptyTrash);
				return;
			case kOSTenNewFolder:
				_CreateDesktopFolder();
				return;
			case kOSTenRename:
                if (fFolderView != NULL)
                    fFolderView->BeginRenameSelected();
				return;
			case kOSTenGetInfo:
				ShowSelectedInfo(fFolderView);
				return;
			case kFinderMoveToTrash:
				if (fFolderView != NULL)
					fFolderView->MoveSelectionToTrash();
				return;
			case kFinderRefreshFolder:
				if (fFolderView != NULL)
					fFolderView->Refresh();
				return;
			case kOSTenArrangeName:
			case kOSTenCleanUp:
				if (fFolderView != NULL)
					fFolderView->Arrange(false);
				return;
			case kOSTenArrangeKind:
				if (fFolderView != NULL)
					fFolderView->Arrange(true);
				return;
			case kOSTenClose:
			case kOSTenViewAsIcons:
				return;
		}
		BWindow::MessageReceived(message);
	}

private:

    void _CreateDesktopFolder()
    {
        if (fFolderView == NULL)
            return;
        BDirectory desktop("/boot/home/Desktop");
        if (desktop.InitCheck() != B_OK)
            return;
        BString name("untitled folder");
        for (int32 suffix = 2; desktop.Contains(name.String()); suffix++)
            name.SetToFormat("untitled folder %ld", (long)suffix);
        if (desktop.CreateDirectory(name.String(), NULL) == B_OK)
            fFolderView->Refresh(name.String());
        else
            (new BAlert("New Folder", "The folder could not be created.",
                "OK"))->Go();
    }

	DesktopView* fView;
	FolderView* fFolderView;
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
		if (message->what == kOSTenSystemFolderAccess) {
			if (!sSystemFolderUnlocked.load()) {
				BAlert* dialog = new BAlert("System Folder Access",
					"Owner: Unlock the System Folder for changes in Finder? "
					"System files affect startup and stability. Finder will "
					"lock it again when restarted.", "Keep Locked", "Unlock");
				if (dialog->Go() == 1) {
					// Opening the folder makes the access change observable
					// instead of silently changing a hidden Finder flag.
					BEntry folder("/boot/System Folder", true);
					entry_ref ref;
					if (folder.InitCheck() != B_OK || !folder.IsDirectory()
						|| folder.GetRef(&ref) != B_OK) {
						(new BAlert("System Folder Access",
							"Cannot open the System Folder. It remains locked.",
							"OK"))->Go();
						return;
					}
					sSystemFolderUnlocked.store(true);
					(new FinderWindow(ref, 0))->Show();
				}
			} else {
				BAlert* dialog = new BAlert("System Folder Access",
					"System Folder changes are currently unlocked in Finder. "
					"Lock them now?", "Keep Unlocked", "Lock");
				if (dialog->Go() == 1) {
					sSystemFolderUnlocked.store(false);
					(new BAlert("System Folder Access",
						"The System Folder is locked again.", "OK"))->Go();
				}
			}
			return;
		}
		if (message->what == kOSTenEmptyTrash) {
			EmptyFinderTrash();
			return;
		}
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
			|| message->what == kOSTenViewAsIcons
			|| message->what == kOSTenRename
			|| message->what == kOSTenGetInfo
			|| message->what == kOSTenArrangeName
			|| message->what == kOSTenArrangeKind
			|| message->what == kOSTenCleanUp
			|| message->what == kOSTenTogglePopupFolder) {
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
