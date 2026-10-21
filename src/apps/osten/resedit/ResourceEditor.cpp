/* SPDX-License-Identifier: MIT */
// OSTen Resource Editor: edit the same visible resources loaded by Finder,
// System and the Platinum window decorator. No executable patching.
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <InterfaceDefs.h>
#include <ListView.h>
#include <Message.h>
#include <Messenger.h>
#include "OSTenMessages.h"
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>
#include <Window.h>

#include <algorithm>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace {

enum {
    kLoadSystem = 'rSys', kLoadFinder = 'rFnd',
    kSelected = 'rSel', kApply = 'rApl', kSave = 'rSav',
    kDiscard = 'rDis', kMoveUp = 'rUp1', kMoveDown = 'rDn1',
    kRectangleChanged = 'rRct'
};

struct ResourceLine {
    std::string line;
    std::string key;
    std::string value;
    bool setting;
};

static bool
ParseRectangle(const std::string& value, BRect& rect)
{
    float l, t, r, b;
    char tail;
    if (sscanf(value.c_str(), "%f,%f,%f,%f%c",
            &l, &t, &r, &b, &tail) != 4
        || !isfinite(l) || !isfinite(t) || !isfinite(r) || !isfinite(b)
        || r <= l || b <= t || l < -8192 || t < -8192
        || r > 8192 || b > 8192)
        return false;
    rect.Set(l, t, r, b);
    return true;
}

static std::string
RectangleText(BRect rect)
{
    char text[160];
    snprintf(text, sizeof(text), "%.1f,%.1f,%.1f,%.1f",
        rect.left, rect.top, rect.right, rect.bottom);
    return text;
}

static std::vector<std::string>
Split(const std::string& value)
{
    std::vector<std::string> result;
    size_t first = 0;
    while (first <= value.size()) {
        size_t end = value.find(',', first);
        if (end == std::string::npos)
            end = value.size();
        if (end > first)
            result.push_back(value.substr(first, end - first));
        if (end == value.size())
            break;
        first = end + 1;
    }
    return result;
}

class RectanglePreview : public BView {
public:
    RectanglePreview(BRect frame)
        : BView(frame, "Resource layout preview", B_FOLLOW_ALL, B_WILL_DRAW),
        fValid(false), fDragging(false), fScale(1)
    {
        SetViewColor((rgb_color){245, 245, 242, 255});
    }

    void SetRectangle(bool valid, BRect value)
    {
        fValid = valid;
        fRect = value;
        fDragging = false;
        Invalidate();
    }

    void Draw(BRect) override
    {
        SetHighColor(210, 210, 210);
        StrokeRect(Bounds().InsetByCopy(1, 1));
        if (!fValid) {
            SetHighColor(70, 70, 70);
            DrawString("Select a rectangle resource to preview and drag.",
                BPoint(20, 37));
            return;
        }
        float maxX = std::max(fRect.right, fRect.Width());
        float maxY = std::max(fRect.bottom, fRect.Height());
        float minX = std::min(0.0f, fRect.left);
        float minY = std::min(0.0f, fRect.top);
        float extentX = std::max(200.0f, maxX - minX);
        float extentY = std::max(130.0f, maxY - minY);
        fScale = std::min(1.0f, std::min(
            (Bounds().Width() - 38) / extentX,
            (Bounds().Height() - 44) / extentY));
        fOrigin.Set(19 - minX * fScale, 22 - minY * fScale);
        BRect visual(fOrigin.x + fRect.left * fScale,
            fOrigin.y + fRect.top * fScale,
            fOrigin.x + fRect.right * fScale,
            fOrigin.y + fRect.bottom * fScale);
        fHit = visual;
        SetHighColor(184, 208, 231);
        FillRect(visual);
        SetHighColor(27, 87, 150);
        SetPenSize(2);
        StrokeRect(visual);
        SetPenSize(1);
        SetHighColor(15, 56, 102);
        DrawString("Drag to reposition", BPoint(visual.left + 8,
            std::min(visual.bottom - 5, visual.top + 21)));
    }

    void MouseDown(BPoint where) override
    {
        if (!fValid || !fHit.Contains(where))
            return;
        fDragging = true;
        fMouse = where;
        SetMouseEventMask(B_POINTER_EVENTS);
    }

    void MouseMoved(BPoint where, uint32 transit, const BMessage*) override
    {
        if (!fDragging)
            return;
        // Only movement is editable in this first visual editor. Dimensions
        // can be changed in the value field, using the same resource format.
        BPoint difference = where - fMouse;
        if (fabsf(difference.x) < 1 && fabsf(difference.y) < 1)
            return;
        fRect.OffsetBy(difference.x / fScale, difference.y / fScale);
        fMouse = where;
        Invalidate();
    }

    void MouseUp(BPoint) override
    {
        if (!fDragging)
            return;
        fDragging = false;
        BMessage moved(kRectangleChanged);
        std::string value = RectangleText(fRect);
        moved.AddString("value", value.c_str());
        Window()->PostMessage(&moved);
    }

private:
    BRect fRect;
    BRect fHit;
    BPoint fMouse;
    BPoint fOrigin;
    bool fValid;
    bool fDragging;
    float fScale;
};

class EditorWindow : public BWindow {
public:
    EditorWindow()
        : BWindow(BRect(70, 65, 1025, 700), "OSTen Resource Editor",
            B_TITLED_WINDOW, B_NORMAL_WINDOW_FEEL, B_QUIT_ON_WINDOW_CLOSE),
        fDirty(false), fSelection(-1)
    {
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor((rgb_color){237, 237, 233, 255});
        AddChild(background);

        background->AddChild(new BButton(BRect(14, 11, 112, 40),
            "system", "System", new BMessage(kLoadSystem)));
        background->AddChild(new BButton(BRect(119, 11, 216, 40),
            "finder", "Finder", new BMessage(kLoadFinder)));
        background->AddChild(new BButton(BRect(734, 11, 835, 40),
            "discard", "Revert", new BMessage(kDiscard)));
        background->AddChild(new BButton(BRect(844, 11, 942, 40),
            "save", "Save", new BMessage(kSave)));

        fKeys = new BListView(BRect(14, 65, 318, 567),
            "Resource keys", B_SINGLE_SELECTION_LIST);
        fKeys->SetSelectionMessage(new BMessage(kSelected));
        background->AddChild(new BScrollView("Resource list", fKeys,
            B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM, 0, false, true));

        background->AddChild(new BStringView(BRect(339, 52, 935, 74),
            "hint", "Select a resource. Apply edits, then Save."));
        fKey = new BTextControl(BRect(339, 80, 941, 108),
            "key", "Key:", "", NULL);
        fKey->SetDivider(75);
        background->AddChild(fKey);
        fValue = new BTextControl(BRect(339, 116, 941, 148),
            "value", "Value:", "", NULL);
        fValue->SetDivider(75);
        background->AddChild(fValue);
        background->AddChild(new BButton(BRect(842, 156, 942, 186),
            "apply", "Apply", new BMessage(kApply)));

        fPreview = new RectanglePreview(BRect(339, 202, 941, 445));
        background->AddChild(fPreview);
        background->AddChild(new BStringView(BRect(342, 456, 923, 480),
            "ordering", "Menu or module order (select an item to move it):"));
        fOrder = new BListView(BRect(339, 485, 752, 566),
            "Order", B_SINGLE_SELECTION_LIST);
        background->AddChild(new BScrollView("Order list", fOrder,
            B_FOLLOW_LEFT_TOP, 0, false, true));
        background->AddChild(new BButton(BRect(771, 493, 853, 522),
            "up", "Up", new BMessage(kMoveUp)));
        background->AddChild(new BButton(BRect(771, 530, 853, 559),
            "down", "Down", new BMessage(kMoveDown)));
        fStatus = new BStringView(BRect(14, 587, 940, 614),
            "status", "Choose a resource set.");
        background->AddChild(fStatus);
        _Load("System");
        Show();
    }

    bool QuitRequested() override
    {
        if (fDirty && (new BAlert("Unsaved resources",
                "Discard the unsaved resource changes?",
                "Cancel", "Discard"))->Go() != 1)
            return false;
        return true;
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case kLoadSystem:
            case kLoadFinder: {
                if (fDirty && (new BAlert("Unsaved resources",
                        "Discard unsaved edits before switching applications?",
                        "Cancel", "Discard"))->Go() != 1)
                    return;
                _Load(message->what == kLoadSystem ? "System" : "Finder");
                return;
            }
            case kSelected: _Select(); return;
            case kApply: _Apply(); return;
            case kDiscard: _Load(fApplication); return;
            case kSave: _Save(); return;
            case kMoveUp: _Move(-1); return;
            case kMoveDown: _Move(1); return;
            case kRectangleChanged: {
                const char* value = NULL;
                if (message->FindString("value", &value) == B_OK)
                    fValue->SetText(value);
                return;
            }
        }
        BWindow::MessageReceived(message);
    }

private:
    void _Load(const std::string& application)
    {
        fApplication = application;
        fPath = "/boot/System Folder/Resources/" + application
            + "/Interface.ostr";
        fLines.clear();
        FILE* file = fopen(fPath.c_str(), "rb");
        if (file == NULL) {
            _Status("Cannot open " + fPath);
            return;
        }
        char buffer[8192];
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            ResourceLine line;
            line.line = buffer;
            while (!line.line.empty() && (line.line.back() == '\n'
                || line.line.back() == '\r'))
                line.line.pop_back();
            size_t equals = line.line.find('=');
            line.setting = equals != std::string::npos && equals > 0
                && line.line[0] != '#' && line.line[0] != ';';
            if (line.setting) {
                line.key = line.line.substr(0, equals);
                line.value = line.line.substr(equals + 1);
            }
            fLines.push_back(line);
        }
        fclose(file);
        fDirty = false;
        _Populate("");
        _Status("Editing " + fPath + " (restart the target app to reload)");
    }

    void _Populate(const std::string& selectKey)
    {
        while (fKeys->CountItems() > 0)
            delete fKeys->RemoveItem((int32)0);
        fIndices.clear();
        for (size_t i = 0; i < fLines.size(); ++i) {
            if (!fLines[i].setting)
                continue;
            fIndices.push_back((int32)i);
            fKeys->AddItem(new BStringItem(fLines[i].key.c_str()));
        }
        fSelection = -1;
        fKey->SetText("");
        fValue->SetText("");
        _Preview("");
        for (size_t i = 0; i < fIndices.size(); ++i) {
            if (fLines[fIndices[i]].key == selectKey) {
                fKeys->Select((int32)i);
                return;
            }
        }
    }

    void _Select()
    {
        int32 selected = fKeys->CurrentSelection();
        if (selected < 0 || (size_t)selected >= fIndices.size())
            return;
        fSelection = fIndices[selected];
        fKey->SetText(fLines[fSelection].key.c_str());
        fValue->SetText(fLines[fSelection].value.c_str());
        _Preview(fLines[fSelection].value);
    }

    void _Preview(const std::string& value)
    {
        BRect rectangle(0, 0, 100, 60);
        fPreview->SetRectangle(ParseRectangle(value, rectangle), rectangle);
        while (fOrder->CountItems() > 0)
            delete fOrder->RemoveItem((int32)0);
        if (fSelection < 0)
            return;
        const std::string& key = fLines[fSelection].key;
        if (key.compare(0, 5, "menu.") != 0
            && key != "strip.module.order")
            return;
        std::vector<std::string> items = Split(value);
        for (size_t i = 0; i < items.size(); ++i)
            fOrder->AddItem(new BStringItem(items[i].c_str()));
    }

    void _Apply()
    {
        std::string key(fKey->Text());
        std::string value(fValue->Text());
        if (key.empty()
            || key.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-")
                != std::string::npos
            || key.size() > 128
            || value.find('\n') != std::string::npos
            || value.find('\r') != std::string::npos
            || value.size() > 7900) {
            _Status("Invalid key or value. Nothing was changed.");
            return;
        }
        for (size_t i = 0; i < fLines.size(); ++i) {
            if (fLines[i].setting && fLines[i].key == key
                && (int32)i != fSelection) {
                _Status("Duplicate resource key. Nothing was changed.");
                return;
            }
        }
        if (fSelection >= 0) {
            fLines[fSelection].key = key;
            fLines[fSelection].value = value;
            fLines[fSelection].line = key + "=" + value;
        } else {
            ResourceLine line;
            line.line = key + "=" + value;
            line.key = key;
            line.value = value;
            line.setting = true;
            fLines.push_back(line);
        }
        fDirty = true;
        _Populate(key);
        _Status("Changed " + key + ". Press Save to write.");
    }

    void _Move(int direction)
    {
        int32 selected = fOrder->CurrentSelection();
        int32 next = selected + direction;
        if (selected < 0 || next < 0 || next >= fOrder->CountItems())
            return;
        std::vector<std::string> items = Split(fValue->Text());
        if ((size_t)next >= items.size())
            return;
        std::swap(items[selected], items[next]);
        std::string value;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i != 0)
                value += ",";
            value += items[i];
        }
        fValue->SetText(value.c_str());
        _Preview(value);
        fOrder->Select(next);
    }

    bool _OwnerAllowsChanges()
    {
        BMessenger finder("application/x-vnd.OSTen-Finder");
        BMessage query(kOSTenQuerySystemFolderAccess);
        BMessage answer;
        bool unlocked = false;
        if (finder.SendMessage(&query, &answer, 500000, 500000) != B_OK
            || answer.FindBool("unlocked", &unlocked) != B_OK || !unlocked) {
            _Status("Unlock the System Folder in Finder's Special menu first.");
            return false;
        }
        return true;
    }

    void _Save()
    {
        if (!_OwnerAllowsChanges())
            return;
        // Apply any visible property edit before persisting the manifest.
        if (fSelection >= 0 && (fLines[fSelection].value != fValue->Text()
            || fLines[fSelection].key != fKey->Text()))
            _Apply();
        if (!fDirty) {
            _Status("No changes to save.");
            return;
        }
        std::string body;
        for (size_t i = 0; i < fLines.size(); ++i) {
            body += fLines[i].line;
            body += "\n";
        }
        std::string temporary = fPath + ".tmp.XXXXXX";
        std::vector<char> name(temporary.begin(), temporary.end());
        name.push_back(0);
        int fd = mkstemp(&name[0]);
        if (fd < 0) {
            _Status("Save failed. Unlock System Folder and check permissions.");
            return;
        }
        size_t offset = 0;
        bool success = true;
        while (offset < body.size()) {
            ssize_t written = write(fd, body.data() + offset,
                body.size() - offset);
            if (written <= 0) { success = false; break; }
            offset += (size_t)written;
        }
        if (fsync(fd) != 0)
            success = false;
        if (close(fd) != 0)
            success = false;
        if (!success) {
            unlink(&name[0]);
            _Status("Could not write changes. Original resources untouched.");
            return;
        }

        // Keep a revertible previous version, even when this is a system UI.
        // If final replacement fails, restore the original manifest.
        std::string backup = fPath + ".bak";
        if (rename(fPath.c_str(), backup.c_str()) != 0) {
            unlink(&name[0]);
            _Status("Could not back up original resources.");
            return;
        }
        if (rename(&name[0], fPath.c_str()) != 0) {
            rename(backup.c_str(), fPath.c_str());
            unlink(&name[0]);
            _Status("Could not install edited resources. Original restored.");
            return;
        }
        fDirty = false;
        _Status("Saved. Backup: " + backup + ". Restart the edited app.");
    }

    void _Status(const std::string& text)
    {
        fStatus->SetText(text.c_str());
    }

    std::string fApplication;
    std::string fPath;
    std::vector<ResourceLine> fLines;
    std::vector<int32> fIndices;
    BListView* fKeys;
    BListView* fOrder;
    BTextControl* fKey;
    BTextControl* fValue;
    RectanglePreview* fPreview;
    BStringView* fStatus;
    bool fDirty;
    int32 fSelection;
};

class ResourceEditorApp : public BApplication {
public:
    ResourceEditorApp()
        : BApplication("application/x-vnd.OSTen-ResourceEditor") {}

    void ReadyToRun() override { new EditorWindow(); }
};
}

int
main()
{
    ResourceEditorApp application;
    application.Run();
    return 0;
}
