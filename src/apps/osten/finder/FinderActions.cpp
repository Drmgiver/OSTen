/* SPDX-License-Identifier: MIT */

#include "FinderActions.h"
#include "FileOperations.h"
#include "OSTenResources.h"
#include "OSTenDialogLayout.h"
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
        // Use the look/feel constructor: the window_type overload would
        // mistake our modal feel for flags and our flags for workspaces.
        BWindow(OSTenFinderResources().Rect("dialog.rename.frame",
                BRect(0, 0, 390, 155)),
            OSTenFinderResources().String("dialog.rename.title", "Rename"),
            B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
            B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE),
        fRef(ref),
        fOwnerUnlocked(ownerUnlocked), fName(NULL)
    {
        const OSTenResourceCatalog& ui = OSTenFinderResources();
        BEntry entry(&ref, false);
        char name[B_FILE_NAME_LENGTH] = "";
        entry.GetName(name);
        // All visible controls are constructed from Finder's dialog
        // declaration, not a compiled list of buttons and rectangles.
        const OSTenDialogAction actions[] = {
            {"rename.cancel", kRenameCancel},
            {"rename.accept", kRenameAccept}
        };
        BView* background = new BView(Bounds(), "rename-interface",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(ui.Color("dialog.rename.background",
            rgb_color{239, 239, 236, 255}));
        AddChild(background);
        OSTenDialogControls controls = OSTenCreateDialogControls(
            background, ui, "dialog.rename", actions,
            sizeof(actions) / sizeof(actions[0]));
        OSTenDialogControls::const_iterator input = controls.find("name");
        if (input != controls.end()) {
            fName = static_cast<BTextControl*>(input->second);
            fName->SetText(name);
        }
        CenterOnScreen();
    }

    void MessageReceived(BMessage* message) override
    {
        if (message->what == kRenameCancel) {
            Quit();
            return;
        }
        if (message->what == kRenameAccept) {
            if (fName == NULL)
                return;
            status_t error = RenameFinderEntry(fRef, fName->Text(),
                fOwnerUnlocked);
            if (error == B_OK) {
                be_app->PostMessage(kOSTenRefreshAll);
                Quit();
                return;
            }
            BString details(OSTenFinderResources().String("dialog.rename.error_intro", "The item could not be renamed.\n\n"));
            if (error == B_BAD_VALUE)
                details << OSTenFinderResources().String("dialog.rename.error_invalid", "Choose a nonempty name without slashes.");
            else if (error == B_PERMISSION_DENIED)
                details << OSTenFinderResources().String("dialog.rename.error_locked", "The System Folder is locked. Unlock it first.");
            else if (error == B_FILE_EXISTS)
                details << OSTenFinderResources().String("dialog.rename.error_exists", "An item with this name already exists.");
            else
                details << strerror(error);
            (new BAlert(OSTenFinderResources().String("dialog.rename.title", "Rename"), details.String(), OSTenFinderResources().String("dialog.common.ok", "OK")))->Go();
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
        (new BAlert("Get Info", OSTenFinderResources().String("dialog.info.error_message", "Unable to read information for this item."),
            OSTenFinderResources().String("dialog.common.ok", "OK")))->Go();
        return;
    }
    char name[B_FILE_NAME_LENGTH];
    if (entry.GetName(name) != B_OK)
        return;
    char modified[80];
    snprintf(modified, sizeof(modified), "%s",
        OSTenFinderResources().String("dialog.info.unknown", "Unknown"));
    struct tm date;
    if (localtime_r(&information.st_mtime, &date) != NULL)
        strftime(modified, sizeof(modified), "%Y-%m-%d %H:%M", &date);
    BString details;
    details << OSTenFinderResources().String("dialog.info.name", "Name: ") << name << "\n"
        << OSTenFinderResources().String("dialog.info.kind", "Kind: ") << (S_ISDIR(information.st_mode) ? OSTenFinderResources().String("dialog.info.folder", "Folder")
            : S_ISLNK(information.st_mode) ? OSTenFinderResources().String("dialog.info.alias", "Alias / link") : OSTenFinderResources().String("dialog.info.file", "File"))
        << OSTenFinderResources().String("dialog.info.location", "\nLocation: ") << path.Path() << "\n";
    if (S_ISREG(information.st_mode))
        details << OSTenFinderResources().String("dialog.info.size", "Size: ") << (int64)information.st_size << OSTenFinderResources().String("dialog.info.size_suffix", " bytes\n");
    details << OSTenFinderResources().String("dialog.info.modified", "Modified: ") << modified;
    (new BAlert(OSTenFinderResources().String("dialog.info.title", "Get Info"), details.String(), OSTenFinderResources().String("dialog.common.ok", "OK")))->Go();
}


void
EmptyFinderTrash()
{
    BDirectory trash("/boot/trash");
    if (trash.InitCheck() != B_OK) {
        (new BAlert(OSTenFinderResources().String("dialog.trash.title", "Trash"), OSTenFinderResources().String("dialog.trash.unavailable", "Trash is not available."), OSTenFinderResources().String("dialog.common.ok", "OK")))->Go();
        return;
    }
    if (trash.CountEntries() == 0) {
        (new BAlert(OSTenFinderResources().String("dialog.trash.title", "Trash"), OSTenFinderResources().String("dialog.trash.empty", "Trash is already empty."), OSTenFinderResources().String("dialog.common.ok", "OK")))->Go();
        return;
    }
    if ((new BAlert(OSTenFinderResources().String("dialog.trash.empty_title", "Empty Trash"),
            OSTenFinderResources().String("dialog.trash.confirm", "Permanently delete everything in Trash?\nThis action cannot be undone."), OSTenFinderResources().String("dialog.common.cancel", "Cancel"), OSTenFinderResources().String("dialog.trash.empty_title", "Empty Trash")))->Go()
            != 1)
        return;
    status_t error = EmptyFinderDirectory(trash);
    be_app->PostMessage(kOSTenRefreshAll);
    if (error != B_OK) {
        BString reason(OSTenFinderResources().String("dialog.trash.error", "Some items could not be deleted.\n\n"));
        reason << strerror(error);
        (new BAlert(OSTenFinderResources().String("dialog.trash.empty_title", "Empty Trash"), reason.String(), OSTenFinderResources().String("dialog.common.ok", "OK")))->Go();
    }
}
