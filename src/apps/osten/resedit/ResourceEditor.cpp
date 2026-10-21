/* SPDX-License-Identifier: MIT */
// OSTen Resource Editor: edit the same visible resources loaded by Finder,
// System and the Platinum window decorator. No executable patching.
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Bitmap.h>
#include <Entry.h>
#include <FilePanel.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TranslationUtils.h>
#include <InterfaceDefs.h>
#include <ListView.h>
#include <Message.h>
#include <Path.h>
#include <Messenger.h>
#include "OSTenMessages.h"
#include "OSTenResources.h"
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
    kChooseTarget = 'rOpn', kTargetChosen = 'rTrg',
    kSelected = 'rSel', kApply = 'rApl', kSave = 'rSav',
    kDiscard = 'rDis', kMoveUp = 'rUp1', kMoveDown = 'rDn1',
    kRectangleChanged = 'rRct', kImportGraphic = 'rImp',
    kAssetSelected = 'rAst', kImageChosen = 'rImg',
    kUndoSave = 'rUnS', kUndoGraphic = 'rUnG'
};

static const OSTenResourceCatalog&
EditorResources()
{
    static OSTenResourceCatalog resources("ResourceEditor");
    return resources;
}

struct AssetDefinition {
    const char* application;
    const char* label;
    const char* filename;
    int width;
    int height;
};

static const AssetDefinition kAssets[] = {
    {"Finder", "Disk (48 x 48)", "disk", 48, 48},
    {"Finder", "Trash empty (48 x 48)", "trash-empty", 48, 48},
    {"Finder", "Trash full (48 x 48)", "trash-full", 48, 48},
    {"Finder", "Folder (40 x 40)", "folder", 40, 40},
    {"Finder", "Finder (40 x 40)", "finder", 40, 40},
    {"Finder", "System (40 x 40)", "system", 40, 40},
    {"System", "Menu wordmark (71 x 20)", "wordmark", 71, 20},
    {"System", "About wordmark (248 x 69)", "about-wordmark", 248, 69},
    {"System", "Finder switcher (22 x 22)", "switcher-finder", 22, 22}
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
        SetViewColor(EditorResources().Color("editor.preview_background",
            rgb_color{245, 245, 242, 255}));
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
        SetHighColor(EditorResources().Color("editor.preview_border", rgb_color{210, 210, 210, 255}));
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
        SetHighColor(EditorResources().Color("editor.preview_fill", rgb_color{184, 208, 231, 255}));
        FillRect(visual);
        SetHighColor(EditorResources().Color("editor.preview_outline", rgb_color{27, 87, 150, 255}));
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
        : BWindow(EditorResources().Rect("editor.window",
                BRect(70, 65, 1025, 700)),
            EditorResources().String("editor.title", "OSTen Resource Editor"),
            B_TITLED_WINDOW, B_NORMAL_WINDOW_FEEL, B_QUIT_ON_WINDOW_CLOSE),
        fDirty(false), fSelection(-1), fAssetIndex(-1), fPicker(NULL),
        fTargetPicker(NULL), fAssetMenu(NULL), fArtImport(NULL),
        fArtUndo(NULL), fCurrentTarget(NULL)
    {
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(EditorResources().Color("editor.background",
            rgb_color{237, 237, 233, 255}));
        AddChild(background);

        background->AddChild(new BButton(EditorResources().Rect(
                "editor.geometry.01", BRect(14, 11, 112, 40)),
            "open", EditorResources().String("editor.open", "Open..."),
            new BMessage(kChooseTarget)));
        fCurrentTarget = new BStringView(EditorResources().Rect(
                "editor.geometry.current", BRect(125, 11, 610, 40)),
            "current_target", EditorResources().String(
                "editor.no_file", "No resource file open"));
        background->AddChild(fCurrentTarget);
        background->AddChild(new BButton(EditorResources().Rect(
                "editor.geometry.undo_save", BRect(624, 11, 727, 40)),
            "undo_save", EditorResources().String(
                "editor.undo_save", "Undo Save"), new BMessage(kUndoSave)));
        background->AddChild(new BButton(EditorResources().Rect("editor.geometry.03", BRect(734, 11, 835, 40)),
            "discard", EditorResources().String("editor.revert", "Revert"), new BMessage(kDiscard)));
        background->AddChild(new BButton(EditorResources().Rect("editor.geometry.04", BRect(844, 11, 942, 40)),
            "save", EditorResources().String("editor.save", "Save"), new BMessage(kSave)));

        fKeys = new BListView(EditorResources().Rect("editor.geometry.05", BRect(14, 65, 318, 567)),
            "Resource keys", B_SINGLE_SELECTION_LIST);
        fKeys->SetSelectionMessage(new BMessage(kSelected));
        background->AddChild(new BScrollView("Resource list", fKeys,
            B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM, 0, false, true));

        background->AddChild(new BStringView(EditorResources().Rect("editor.geometry.06", BRect(339, 52, 935, 74)),
            "hint", "Select a resource. Apply edits, then Save."));
        fKey = new BTextControl(EditorResources().Rect("editor.geometry.07", BRect(339, 80, 941, 108)),
            "key", EditorResources().String("editor.key", "Key:"), "", NULL);
        fKey->SetDivider(75);
        background->AddChild(fKey);
        fValue = new BTextControl(EditorResources().Rect("editor.geometry.08", BRect(339, 116, 941, 148)),
            "value", EditorResources().String("editor.value", "Value:"), "", NULL);
        fValue->SetDivider(75);
        background->AddChild(fValue);
        fAssetMenu = new BPopUpMenu("Artwork");
        BMenuField* assets = new BMenuField(EditorResources().Rect(
                "editor.geometry.09", BRect(339, 155, 626, 189)),
            "artwork", "Artwork:", fAssetMenu);
        assets->SetDivider(75);
        background->AddChild(assets);
        fArtImport = new BButton(EditorResources().Rect(
                "editor.geometry.10", BRect(639, 156, 735, 186)),
            "import", EditorResources().String("editor.import", "Import..."),
            new BMessage(kImportGraphic));
        background->AddChild(fArtImport);
        fArtUndo = new BButton(EditorResources().Rect(
                "editor.geometry.undo_graphic", BRect(743, 156, 834, 186)),
            "undo_graphic", EditorResources().String(
                "editor.undo_graphic", "Undo Art"), new BMessage(kUndoGraphic));
        background->AddChild(fArtUndo);
        background->AddChild(new BButton(EditorResources().Rect("editor.geometry.11", BRect(842, 156, 942, 186)),
            "apply", EditorResources().String("editor.apply", "Apply"), new BMessage(kApply)));

        fPreview = new RectanglePreview(EditorResources().Rect("editor.geometry.12", BRect(339, 202, 941, 445)));
        background->AddChild(fPreview);
        background->AddChild(new BStringView(EditorResources().Rect("editor.geometry.13", BRect(342, 456, 923, 480)),
            "ordering", "Menu or module order (select an item to move it):"));
        fOrder = new BListView(EditorResources().Rect("editor.geometry.14", BRect(339, 485, 752, 566)),
            "Order", B_SINGLE_SELECTION_LIST);
        background->AddChild(new BScrollView("Order list", fOrder,
            B_FOLLOW_LEFT_TOP, 0, false, true));
        background->AddChild(new BButton(EditorResources().Rect("editor.geometry.15", BRect(771, 493, 853, 522)),
            "up", EditorResources().String("editor.up", "Up"), new BMessage(kMoveUp)));
        background->AddChild(new BButton(EditorResources().Rect("editor.geometry.16", BRect(771, 530, 853, 559)),
            "down", EditorResources().String("editor.down", "Down"), new BMessage(kMoveDown)));
        fStatus = new BStringView(EditorResources().Rect("editor.geometry.17", BRect(14, 587, 940, 614)),
            "status", "Choose a resource set.");
        background->AddChild(fStatus);
        _RefreshAssets();
        Show();
        _ChooseTarget();
    }

    ~EditorWindow() override
    {
        delete fPicker;
        delete fTargetPicker;
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
            case kChooseTarget: _ChooseTarget(); return;
            case kTargetChosen: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK)
                    _OpenTarget(ref);
                return;
            }
            case kSelected: _Select(); return;
            case kApply: _Apply(); return;
            case kDiscard:
                if (!fPath.empty()) _LoadPath(fPath, fApplication);
                return;
            case kSave: _Save(); return;
            case kMoveUp: _Move(-1); return;
            case kMoveDown: _Move(1); return;
            case kAssetSelected: {
                int32 index = -1;
                if (message->FindInt32("index", &index) == B_OK
                    && index >= 0 && (size_t)index
                        < sizeof(kAssets) / sizeof(kAssets[0])
                    && fPath == std::string("/boot/System Folder/Resources/")
                        + kAssets[index].application + "/Interface.ostr")
                    fAssetIndex = index;
                return;
            }
            case kImportGraphic: _PickGraphic(); return;
            case kUndoSave: _UndoSave(); return;
            case kUndoGraphic: _UndoGraphic(); return;
            case kImageChosen: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK)
                    _ImportGraphic(ref);
                return;
            }
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
    void _ChooseTarget()
    {
        if (fTargetPicker == NULL) {
            BMessenger target(this);
            BMessage chosen(kTargetChosen);
            fTargetPicker = new BFilePanel(B_OPEN_PANEL, &target, NULL,
                B_FILE_NODE | B_DIRECTORY_NODE, false, &chosen);
            BEntry boot("/boot");
            entry_ref bootRef;
            if (boot.GetRef(&bootRef) == B_OK)
                fTargetPicker->SetPanelDirectory(&bootRef);
        }
        fTargetPicker->Show();
    }

    // Open an .ostr directly or resolve an application's associated manifest.
    // Never patch an executable when we do not understand its resource format.
    void _OpenTarget(const entry_ref& ref)
    {
        BEntry entry(&ref, true);
        BPath path;
        if (entry.GetPath(&path) != B_OK) {
            _Status("Could not resolve the selected file.");
            return;
        }
        std::string selected(path.Path());
        std::string name(path.Leaf());
        std::string application = name;
        std::vector<std::string> candidates;
        bool manifest = entry.IsFile() && name.size() >= 5
            && name.compare(name.size() - 5, 5, ".ostr") == 0;

        if (manifest) {
            candidates.push_back(selected);
            size_t slash = selected.rfind('/');
            if (slash != std::string::npos) {
                std::string parent = selected.substr(0, slash);
                slash = parent.rfind('/');
                application = parent.substr(slash == std::string::npos
                    ? 0 : slash + 1);
            }
        } else {
            if (entry.IsDirectory()) {
                candidates.push_back(selected + "/Interface.ostr");
                candidates.push_back(selected + "/Resources/Interface.ostr");
            } else if (entry.IsFile()) {
                candidates.push_back(selected + ".ostr");
                candidates.push_back(selected + ".resources/Interface.ostr");
            }
            if (name == "Resource Editor" || name == "OSTenResourceEditor")
                application = "ResourceEditor";
            else if (name == "OSTenSystem")
                application = "System";
            else if (name == "OSTenFinder")
                application = "Finder";
            candidates.push_back("/boot/System Folder/Resources/"
                + application + "/Interface.ostr");
            if (application != name)
                candidates.push_back("/boot/System Folder/Resources/"
                    + name + "/Interface.ostr");
            std::string compact = name;
            compact.erase(std::remove(compact.begin(), compact.end(), ' '),
                compact.end());
            if (compact != name && compact != application)
                candidates.push_back("/boot/System Folder/Resources/"
                    + compact + "/Interface.ostr");
        }

        for (size_t i = 0; i < candidates.size(); ++i) {
            struct stat details;
            if (stat(candidates[i].c_str(), &details) != 0
                || !S_ISREG(details.st_mode))
                continue;
            if (fDirty && (new BAlert("Unsaved resources",
                    "Discard unsaved edits and open another resource file?",
                    "Cancel", "Discard"))->Go() != 1)
                return;
            if (_LoadPath(candidates[i], application))
                return;
        }

        (new BAlert("Unsupported resource file",
            "No editable OSTen resource manifest was found for that selection.\n"
            "Choose an .ostr file or an OSTen application with an Interface.ostr "
            "manifest. Editing other applications' compiled binary resources "
            "is not implemented yet.", "OK"))->Go();
        _Status("No supported resource manifest found for " + selected);
    }

    bool _LoadPath(const std::string& path, const std::string& application)
    {
        FILE* file = fopen(path.c_str(), "rb");
        if (file == NULL) {
            _Status("Cannot open " + path);
            return false;
        }
        std::vector<ResourceLine> lines;
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
            lines.push_back(line);
        }
        bool success = !ferror(file);
        fclose(file);
        if (!success) {
            _Status("Error reading " + path);
            return false;
        }
        fLines.swap(lines);
        fPath = path;
        fApplication = application;
        fDirty = false;
        _Populate("");
        _RefreshAssets();
        fCurrentTarget->SetText(application.c_str());
        _Status("Editing " + fPath + " (restart the target app to reload)");
        return true;
    }

    void _RefreshAssets()
    {
        while (fAssetMenu->CountItems() > 0)
            delete fAssetMenu->RemoveItem((int32)0);
        fAssetIndex = -1;
        // Only known System and Finder RGBA dimensions are safe to offer.
        for (size_t i = 0; i < sizeof(kAssets) / sizeof(kAssets[0]); ++i) {
            if (fPath != std::string("/boot/System Folder/Resources/")
                    + kAssets[i].application + "/Interface.ostr")
                continue;
            BMessage* choose = new BMessage(kAssetSelected);
            choose->AddInt32("index", (int32)i);
            BMenuItem* item = new BMenuItem(kAssets[i].label, choose);
            fAssetMenu->AddItem(item);
            if (fAssetIndex < 0) {
                fAssetIndex = (int32)i;
                item->SetMarked(true);
            }
        }
        if (fAssetIndex < 0) {
            BMenuItem* item = new BMenuItem("No known artwork", NULL);
            item->SetEnabled(false);
            fAssetMenu->AddItem(item);
        }
        fAssetMenu->SetTargetForItems(this);
        fArtImport->SetEnabled(fAssetIndex >= 0);
        fArtUndo->SetEnabled(fAssetIndex >= 0);
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
        if (fPath.empty()) {
            _Status("Open an application or .ostr resource file first.");
            return;
        }
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

    bool _SwapBackup(const std::string& file)
    {
        std::string backup = file + ".bak";
        if (access(backup.c_str(), R_OK) != 0) {
            _Status("No previous version is available for that resource.");
            return false;
        }
        std::string temp = file + ".restore.XXXXXX";
        std::vector<char> name(temp.begin(), temp.end());
        name.push_back(0);
        int fd = mkstemp(&name[0]);
        if (fd < 0) {
            _Status("Could not prepare the restore operation.");
            return false;
        }
        close(fd);
        unlink(&name[0]);
        if (rename(file.c_str(), &name[0]) != 0
            || rename(backup.c_str(), file.c_str()) != 0) {
            rename(&name[0], file.c_str());
            _Status("Restore failed. Existing resource kept.");
            return false;
        }
        if (rename(&name[0], backup.c_str()) != 0)
            _Status("Restored, but the new backup could not be retained.");
        return true;
    }

    void _UndoSave()
    {
        if (fPath.empty() || !_OwnerAllowsChanges())
            return;
        if (_SwapBackup(fPath)) {
            _Load(fApplication);
            _Status("Previous interface restored. Restart edited application.");
        }
    }

    void _UndoGraphic()
    {
        if (fAssetIndex < 0 || !_OwnerAllowsChanges(true))
            return;
        const AssetDefinition& asset = kAssets[fAssetIndex];
        const std::string path = std::string("/boot/System Folder/Resources/")
            + asset.application + "/" + asset.filename + ".rgba";
        if (_SwapBackup(path))
            _Status("Previous graphic restored. Restart the target application.");
    }

    void _PickGraphic()
    {
        if (!fPicker) {
            BMessenger target(this);
            BMessage selected(kImageChosen);
            fPicker = new BFilePanel(B_OPEN_PANEL, &target, NULL,
                B_FILE_NODE, false, &selected);
        }
        fPicker->Show();
    }

    void _ImportGraphic(const entry_ref& ref)
    {
        if (fAssetIndex < 0 || !_OwnerAllowsChanges(true))
            return;
        const AssetDefinition& asset = kAssets[fAssetIndex];
        BBitmap* bitmap = BTranslationUtils::GetBitmap(&ref);
        if (bitmap == NULL) {
            _Status("Image translator could not load that file.");
            return;
        }
        const int width = (int)bitmap->Bounds().Width() + 1;
        const int height = (int)bitmap->Bounds().Height() + 1;
        if (width != asset.width || height != asset.height
            || (bitmap->ColorSpace() != B_RGBA32
                && bitmap->ColorSpace() != B_RGB32)) {
            delete bitmap;
            _Status("Graphic must match the listed pixel dimensions and be 32-bit.");
            return;
        }

        const uint8* pixels = (const uint8*)bitmap->Bits();
        const int stride = bitmap->BytesPerRow();
        const bool alpha = bitmap->ColorSpace() == B_RGBA32;
        std::vector<uint8> rgba((size_t)width * height * 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const uint8* pixel = pixels + y * stride + x * 4;
                uint8* target = &rgba[((size_t)y * width + x) * 4];
                target[0] = pixel[2];
                target[1] = pixel[1];
                target[2] = pixel[0];
                target[3] = alpha ? pixel[3] : 255;
            }
        }
        delete bitmap;

        std::string path = std::string("/boot/System Folder/Resources/")
            + asset.application + "/" + asset.filename + ".rgba";
        std::string temporary = path + ".tmp.XXXXXX";
        std::vector<char> filename(temporary.begin(), temporary.end());
        filename.push_back(0);
        int fd = mkstemp(&filename[0]);
        if (fd < 0) {
            _Status("Cannot save graphic. Check System Folder access.");
            return;
        }
        size_t offset = 0;
        bool success = true;
        while (offset < rgba.size()) {
            ssize_t written = write(fd, &rgba[offset], rgba.size() - offset);
            if (written <= 0) { success = false; break; }
            offset += (size_t)written;
        }
        if (fsync(fd) != 0)
            success = false;
        if (close(fd) != 0)
            success = false;
        if (!success) {
            unlink(&filename[0]);
            _Status("Graphic import failed. Existing resource preserved.");
            return;
        }
        std::string backup = path + ".bak";
        if (access(path.c_str(), F_OK) == 0) {
            if (rename(path.c_str(), backup.c_str()) != 0) {
                unlink(&filename[0]);
                _Status("Cannot back up previous graphic.");
                return;
            }
        }
        if (rename(&filename[0], path.c_str()) != 0) {
            rename(backup.c_str(), path.c_str());
            unlink(&filename[0]);
            _Status("Cannot install graphic. Previous resource restored.");
            return;
        }
        _Status(std::string("Imported ") + asset.label
            + ". Restart the target application.");
    }

    bool _OwnerAllowsChanges(bool protectedArtwork = false)
    {
        // Respect Finder Owner access for installed system files; ordinary
        // .ostr files elsewhere use normal filesystem permissions.
        if (!protectedArtwork && fPath.compare(0,
                strlen("/boot/System Folder/"), "/boot/System Folder/") != 0)
            return true;
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
        if (fPath.empty()) {
            _Status("Open an application or .ostr resource file first.");
            return;
        }
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
    int32 fAssetIndex;
    BFilePanel* fPicker;
    BFilePanel* fTargetPicker;
    BPopUpMenu* fAssetMenu;
    BButton* fArtImport;
    BButton* fArtUndo;
    BStringView* fCurrentTarget;
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
