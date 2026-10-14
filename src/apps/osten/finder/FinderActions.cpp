/* SPDX-License-Identifier: MIT */

#include "FinderActions.h"
#include "FileOperations.h"
#include "OSTenMessages.h"

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <String.h>
#include <TextControl.h>
#include <Window.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>


namespace {

const uint32 kRenameAccept = 'osra';
const uint32 kRenameCancel = 'osrc';


class RenameWindow : public BWindow {
public:
    RenameWindow(const entry_ref& ref, bool ownerUnlocked)
        :
        BWindow(BRect(0, 0, 390, 155), "Rename",
            B_TITLED_WINDOW, B_MODAL_APP_WINDOW_FEEL,
            B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE),
        fRef(ref),
        fOwnerUnlocked(ownerUnlocked)
    {
        BEntry entry(&ref, false);
        char name[B_FILE_NAME_LENGTH] = "";
        entry.GetName(name);
        fName = new BTextControl(BRect(20, 27, 365, 57), "name",
            "Name:", name, NULL);
        fName->SetDivider(56);
        AddChild(fName);
        BButton* cancel = new BButton(BRect(185, 100, 268, 129),
            "cancel", "Cancel", new BMessage(kRenameCancel));
        BButton* rename = new BButton(BRect(278, 100, 370, 129),
            "rename", "Rename", new BMessage(kRenameAccept));
        AddChild(cancel);
        AddChild(rename);
        SetDefaultButton(rename);
        CenterOnScreen();
    }

    void MessageReceived(BMessage* message) override
    {
        if (message->what == kRenameCancel) {
            Quit();
            return;
        }
        if (message->what == kRenameAccept) {
            status_t error = RenameFinderEntry(fRef, fName->Text(),
                fOwnerUnlocked);
            if (error == B_OK) {
                be_app->PostMessage(kOSTenRefreshAll);
                Quit();
                return;
            }
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
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    entry_ref fRef;
    bool fOwnerUnlocked;
    BTextControl* fName;
};

} // namespace


void
ShowFinderRename(const entry_ref& ref, bool ownerUnlocked)
{
    (new RenameWindow(ref, ownerUnlocked))->Show();
}


void
ShowFinderInfo(const entry_ref& ref)
{
    BEntry entry(&ref, false);
    BPath path;
    struct stat information;
    if (entry.GetPath(&path) != B_OK || entry.GetStat(&information) != B_OK) {
        (new BAlert("Get Info", "Unable to read information for this item.",
            "OK"))->Go();
        return;
    }
    char name[B_FILE_NAME_LENGTH];
    if (entry.GetName(name) != B_OK)
        return;
    char modified[80] = "Unknown";
    struct tm date;
    if (localtime_r(&information.st_mtime, &date) != NULL)
        strftime(modified, sizeof(modified), "%Y-%m-%d %H:%M", &date);
    BString details;
    details << "Name: " << name << "\n"
        << "Kind: " << (S_ISDIR(information.st_mode) ? "Folder"
            : S_ISLNK(information.st_mode) ? "Alias / link" : "File")
        << "\nLocation: " << path.Path() << "\n";
    if (S_ISREG(information.st_mode))
        details << "Size: " << (int64)information.st_size << " bytes\n";
    details << "Modified: " << modified;
    (new BAlert("Get Info", details.String(), "OK"))->Go();
}


void
EmptyFinderTrash()
{
    BDirectory trash("/boot/trash");
    if (trash.InitCheck() != B_OK) {
        (new BAlert("Trash", "Trash is not available.", "OK"))->Go();
        return;
    }
    if (trash.CountEntries() == 0) {
        (new BAlert("Trash", "Trash is already empty.", "OK"))->Go();
        return;
    }
    if ((new BAlert("Empty Trash",
            "Permanently delete everything in Trash?\n"
            "This action cannot be undone.", "Cancel", "Empty Trash"))->Go()
            != 1)
        return;
    status_t error = EmptyFinderDirectory(trash);
    be_app->PostMessage(kOSTenRefreshAll);
    if (error != B_OK) {
        BString reason("Some items could not be deleted.\n\n");
        reason << strerror(error);
        (new BAlert("Empty Trash", reason.String(), "OK"))->Go();
    }
}
