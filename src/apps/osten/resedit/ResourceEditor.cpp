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
#include "OSTenDialogLayout.h"
#include "OSTenResourceIndex.h"
#include "OSTenGlobalMenu.h"
#include "ResourceBrowser.h"
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
    std::string label;
    std::string filename;
    std::string path;
    int width;
    int height;
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
            SetHighColor(EditorResources().Color("editor.preview.hint_color",
                rgb_color{70, 70, 70, 255}));
            DrawString(EditorResources().String("editor.preview_hint", "Select a rectangle resource to preview and drag."),
                BPoint(EditorResources().Number("editor.preview.hint_x", 20),
                    EditorResources().Number("editor.preview.hint_y", 37)));
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
        SetHighColor(EditorResources().Color("editor.preview.drag_text_color",
            rgb_color{15, 56, 102, 255}));
        DrawString(EditorResources().String("editor.drag_hint", "Drag to reposition"),
            BPoint(visual.left + EditorResources().Number(
                "editor.preview.drag_x_offset", 8),
                std::min(visual.bottom - EditorResources().Number(
                    "editor.preview.drag_bottom_offset", 5),
                    visual.top + EditorResources().Number(
                        "editor.preview.drag_top_offset", 21))));
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

// The classic property inspector is itself a composed, Owner-editable
// interface. Custom widgets are registered factories, not hardcoded
// layout rules or special cases inside the common dialog engine.
static BView*
CreateResourceArtworkMenu(BRect frame, const char* id,
    const char* label, uint32, const OSTenResourceCatalog& catalog,
    const std::string&)
{
    return new BMenuField(frame, id, label,
        new BPopUpMenu(catalog.String("editor.artwork_menu_title", "Artwork")));
}

static BView*
CreateResourceRectanglePreview(BRect frame, const char*,
    const char*, uint32, const OSTenResourceCatalog&, const std::string&)
{
    return new RectanglePreview(frame);
}

template<typename T>
static T*
PropertyControl(const OSTenDialogControls& controls, const char* id,
    const char* expectedType)
{
    const std::string key = std::string("editor.inspector.control.") + id
        + ".type";
    if (strcmp(EditorResources().String(key.c_str(), ""), expectedType) != 0)
        return NULL;
    OSTenDialogControls::const_iterator found = controls.find(id);
    return found == controls.end() ? NULL : static_cast<T*>(found->second);
}

class EditorWindow : public BWindow {
public:
    EditorWindow(const std::string& resourceFile = "",
        const std::string& application = "",
        const std::string& selectedKey = "")
        : BWindow(EditorResources().Rect("editor.window",
                BRect(70, 65, 1025, 700)),
            EditorResources().String("editor.title", "OSTen Resource Editor"),
            B_TITLED_WINDOW, B_NORMAL_WINDOW_FEEL, 0),
        fKeys(NULL), fOrder(NULL), fKey(NULL), fValue(NULL),
        fPreview(NULL), fStatus(NULL), fDirty(false),
        fSelection(-1), fAssetIndex(-1), fPicker(NULL),
        fTargetPicker(NULL), fAssetMenu(NULL), fArtImport(NULL),
        fArtUndo(NULL), fCurrentTarget(NULL)
    {
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(EditorResources().Color("editor.background",
            rgb_color{237, 237, 233, 255}));
        AddChild(background);

        const OSTenDialogAction actions[] = {
            {"inspector.open", kChooseTarget},
            {"inspector.undo_save", kUndoSave},
            {"inspector.revert", kDiscard},
            {"inspector.save", kSave},
            {"inspector.selected", kSelected},
            {"inspector.import", kImportGraphic},
            {"inspector.undo_art", kUndoGraphic},
            {"inspector.apply", kApply},
            {"inspector.up", kMoveUp},
            {"inspector.down", kMoveDown}
        };
        OSTenDialogControlFactories factories;
        factories["artwork_menu"] = &CreateResourceArtworkMenu;
        factories["rectangle_preview"] = &CreateResourceRectanglePreview;
        OSTenDialogControls controls = OSTenCreateDialogControls(
            background, EditorResources(), "editor.inspector",
            actions, sizeof(actions) / sizeof(actions[0]), &factories);
        fKeys = PropertyControl<BListView>(controls, "keys", "list");
        fOrder = PropertyControl<BListView>(controls, "order", "list");
        fKey = PropertyControl<BTextControl>(controls, "key", "text");
        fValue = PropertyControl<BTextControl>(controls, "value", "text");
        fCurrentTarget = PropertyControl<BStringView>(
            controls, "current_target", "label");
        fPreview = PropertyControl<RectanglePreview>(
            controls, "preview", "rectangle_preview");
        fStatus = PropertyControl<BStringView>(controls, "status", "label");
        fArtImport = PropertyControl<BButton>(controls, "import", "button");
        fArtUndo = PropertyControl<BButton>(controls, "undo_art", "button");
        BMenuField* artwork = PropertyControl<BMenuField>(
            controls, "artwork", "artwork_menu");
        fAssetMenu = artwork == NULL ? NULL
            : static_cast<BPopUpMenu*>(artwork->Menu());
        if (fKeys != NULL)
            fKeys->SetSelectionMessage(new BMessage(kSelected));
        if (fKey != NULL) fKey->SetDivider(75);
        if (fValue != NULL) fValue->SetDivider(75);
        if (artwork != NULL) artwork->SetDivider(75);
        // The Owner can delete or change any control. No silent restore:
        // the inspector simply omits operations whose essential widgets
        // were intentionally removed, instead of dereferencing bad types.
        if (!_HasRequiredControls()) {
            // An Owner-edited partial interface still opens as authored.
            // Missing required controls disable editing rather than
            // resurrecting controls that the Owner intentionally removed.
            Show();
            return;
        }
        _RefreshAssets();
        Show();
        if (!resourceFile.empty()) {
            if (_LoadPath(resourceFile, application))
                _Populate(selectedKey);
        } else
            _ChooseTarget();
    }

    ~EditorWindow() override
    {
        delete fPicker;
        delete fTargetPicker;
    }

    bool QuitRequested() override
    {
        if (fDirty && (new BAlert(EditorResources().String("editor.alert.unsaved_title", "Unsaved resources"),
                EditorResources().String("editor.alert.unsaved_message", "Discard the unsaved resource changes?"),
                EditorResources().String("editor.alert.cancel", "Cancel"), EditorResources().String("editor.alert.discard", "Discard")))->Go() != 1)
            return false;
        return true;
    }

    void MessageReceived(BMessage* message) override
    {
        if (!_HasRequiredControls()) {
            BWindow::MessageReceived(message);
            return;
        }
        switch (message->what) {
            case kChooseTarget: _ChooseTarget(); return;
            case kTargetChosen: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK)
                    _OpenTarget(ref);
                // The file panel can leave the editor behind Finder when
                // its Open action completes. Bring the already-running
                // editor forward without requiring a second app launch.
                Activate(true);
                return;
            }
            case B_CANCEL: {
                // BFilePanel also sends B_CANCEL after hiding on a
                // successful Open; refocus after that hide completes.
                // Ignore cancellation from the artwork import panel.
                int32 previous = 0;
                if (message->FindInt32("old_what", &previous) == B_OK
                    && previous == (int32)kTargetChosen) {
                    Activate(true);
                    return;
                }
                break;
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
                    && index >= 0 && (size_t)index < fAssets.size()) {
                    fAssetIndex = index;
                    for (int32 i = 0; i < fAssetMenu->CountItems(); ++i)
                        fAssetMenu->ItemAt(i)->SetMarked(i == index);
                }
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
    bool _HasRequiredControls() const
    {
        return fKeys != NULL && fOrder != NULL && fKey != NULL
            && fValue != NULL && fPreview != NULL
            && fStatus != NULL && fCurrentTarget != NULL
            && fAssetMenu != NULL && fArtImport != NULL
            && fArtUndo != NULL;
    }

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
            _Status(EditorResources().String("editor.status.resolve_failed", "Could not resolve the selected file."));
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
                size_t slash = selected.rfind('/');
                if (slash != std::string::npos)
                    candidates.push_back(selected.substr(0, slash)
                        + "/Resources/Interface.ostr");
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
            if (fDirty && (new BAlert(EditorResources().String("editor.alert.unsaved_title", "Unsaved resources"),
                    EditorResources().String("editor.alert.unsaved_open", "Discard unsaved edits and open another resource file?"),
                    EditorResources().String("editor.alert.cancel", "Cancel"), EditorResources().String("editor.alert.discard", "Discard")))->Go() != 1)
                return;
            if (_LoadPath(candidates[i], application))
                return;
        }

        (new BAlert(EditorResources().String("editor.alert.unsupported_title", "Unsupported resource file"),
            EditorResources().String("editor.alert.unsupported_body",
                "No editable OSTen resource manifest was found for that selection.\n"
                "Choose an .ostr file or an OSTen application with an Interface.ostr "
                "manifest. Editing other applications' compiled binary resources "
                "is not implemented yet."),
            EditorResources().String("editor.alert.ok", "OK")))->Go();
        _Status(std::string(EditorResources().String("editor.status.no_manifest_prefix",
            "No supported resource manifest found for ")) + selected);
    }

    bool _LoadPath(const std::string& path, const std::string& application)
    {
        FILE* file = fopen(path.c_str(), "rb");
        if (file == NULL) {
            _Status(std::string(EditorResources().String("editor.status.cannot_open_prefix",
                "Cannot open ")) + path);
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
            _Status(std::string(EditorResources().String("editor.status.error_read_prefix",
                "Error reading ")) + path);
            return false;
        }
        fLines.swap(lines);
        fPath = path;
        fApplication = application;
        fDirty = false;
        _Populate("");
        _RefreshAssets();
        fCurrentTarget->SetText(application.c_str());
        _Status(std::string(EditorResources().String("editor.status.editing_prefix",
            "Editing ")) + fPath
            + EditorResources().String("editor.status.editing_suffix",
                " (restart the target app to reload)"));
        return true;
    }

    void _RefreshAssets()
    {
        while (fAssetMenu->CountItems() > 0)
            delete fAssetMenu->RemoveItem((int32)0);
        fAssets.clear();
        fAssetIndex = -1;

        // The same public index drives artwork imports for every native
        // OSTen application. Do not special-case Finder or System assets.
        size_t slash = fPath.rfind('/');
        if (slash != std::string::npos) {
            std::string directory = fPath.substr(0, slash);
            std::map<std::string, OSTenIndexedResource> index;
            if (OSTenReadResourceIndex(directory + "/ResourceIndex.osti",
                    index)) {
                for (std::map<std::string, OSTenIndexedResource>::const_iterator
                        item = index.begin(); item != index.end(); ++item) {
                    const OSTenIndexedResource& res = item->second;
                    if ((res.type != "ICON" && res.type != "IMAG")
                        || res.width <= 0 || res.height <= 0
                        || res.width > 512 || res.height > 512)
                        continue;
                    AssetDefinition asset;
                    asset.label = res.name;
                    asset.filename = res.key;
                    asset.path = directory + "/" + res.key + ".rgba";
                    asset.width = res.width;
                    asset.height = res.height;
                    struct stat st;
                    if (stat(asset.path.c_str(), &st) != 0
                        || !S_ISREG(st.st_mode)
                        || (size_t)st.st_size !=
                            (size_t)asset.width * asset.height * 4)
                        continue;
                    int32 selected = (int32)fAssets.size();
                    fAssets.push_back(asset);
                    BMessage* choose = new BMessage(kAssetSelected);
                    choose->AddInt32("index", selected);
                    fAssetMenu->AddItem(new BMenuItem(
                        asset.label.c_str(), choose));
                    if (fAssetIndex < 0)
                        fAssetIndex = selected;
                }
            }
        }
        if (fAssetIndex < 0) {
            BMenuItem* empty = new BMenuItem(
                EditorResources().String("editor.no_artwork",
                    "No indexed artwork"), NULL);
            empty->SetEnabled(false);
            fAssetMenu->AddItem(empty);
        } else {
            fAssetMenu->ItemAt(fAssetIndex)->SetMarked(true);
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
            _Status(EditorResources().String("editor.status.open_resource", "Open an application or .ostr resource file first."));
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
            _Status(EditorResources().String("editor.status.invalid_value", "Invalid key or value. Nothing was changed."));
            return;
        }
        for (size_t i = 0; i < fLines.size(); ++i) {
            if (fLines[i].setting && fLines[i].key == key
                && (int32)i != fSelection) {
                _Status(EditorResources().String("editor.status.duplicate_key", "Duplicate resource key. Nothing was changed."));
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
        _Status(std::string(EditorResources().String("editor.status.changed_prefix",
            "Changed ")) + key
            + EditorResources().String("editor.status.changed_suffix",
                ". Press Save to write."));
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
            _Status(EditorResources().String("editor.status.no_previous", "No previous version is available for that resource."));
            return false;
        }
        std::string temp = file + ".restore.XXXXXX";
        std::vector<char> name(temp.begin(), temp.end());
        name.push_back(0);
        int fd = mkstemp(&name[0]);
        if (fd < 0) {
            _Status(EditorResources().String("editor.status.restore_prepare", "Could not prepare the restore operation."));
            return false;
        }
        close(fd);
        unlink(&name[0]);
        if (rename(file.c_str(), &name[0]) != 0
            || rename(backup.c_str(), file.c_str()) != 0) {
            rename(&name[0], file.c_str());
            _Status(EditorResources().String("editor.status.restore_failed", "Restore failed. Existing resource kept."));
            return false;
        }
        if (rename(&name[0], backup.c_str()) != 0)
            _Status(EditorResources().String("editor.status.restore_backup_failed", "Restored, but the new backup could not be retained."));
        return true;
    }

    void _UndoSave()
    {
        if (fPath.empty() || !_OwnerAllowsChanges())
            return;
        if (_SwapBackup(fPath)) {
            _LoadPath(fPath, fApplication);
            _Status(EditorResources().String("editor.status.restore_interface", "Previous interface restored. Restart edited application."));
        }
    }

    void _UndoGraphic()
    {
        if (fAssetIndex < 0 || (size_t)fAssetIndex >= fAssets.size())
            return;
        const AssetDefinition& asset = fAssets[fAssetIndex];
        if (!_OwnerAllowsChanges(asset.path))
            return;
        if (_SwapBackup(asset.path))
            _Status(EditorResources().String("editor.status.restore_artwork", "Previous graphic restored. Restart the target application."));
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
        if (fAssetIndex < 0 || (size_t)fAssetIndex >= fAssets.size())
            return;
        const AssetDefinition& asset = fAssets[fAssetIndex];
        if (!_OwnerAllowsChanges(asset.path))
            return;
        BBitmap* bitmap = BTranslationUtils::GetBitmap(&ref);
        if (bitmap == NULL) {
            _Status(EditorResources().String("editor.status.image_unreadable", "Image translator could not load that file."));
            return;
        }
        const int width = (int)bitmap->Bounds().Width() + 1;
        const int height = (int)bitmap->Bounds().Height() + 1;
        if (width != asset.width || height != asset.height
            || (bitmap->ColorSpace() != B_RGBA32
                && bitmap->ColorSpace() != B_RGB32)) {
            delete bitmap;
            _Status(EditorResources().String("editor.status.image_dimensions", "Graphic must match the listed pixel dimensions and be 32-bit."));
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

        std::string path = asset.path;
        std::string temporary = path + ".tmp.XXXXXX";
        std::vector<char> filename(temporary.begin(), temporary.end());
        filename.push_back(0);
        int fd = mkstemp(&filename[0]);
        if (fd < 0) {
            _Status(EditorResources().String("editor.status.image_temp_failed", "Cannot save graphic. Check System Folder access."));
            return;
        }
        struct stat previousGraphic;
        bool hasGraphic = stat(path.c_str(), &previousGraphic) == 0;
        size_t offset = 0;
        bool success = !hasGraphic
            || fchmod(fd, previousGraphic.st_mode & 07777) == 0;
        while (success && offset < rgba.size()) {
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
            _Status(EditorResources().String("editor.status.image_write_failed", "Graphic import failed. Existing resource preserved."));
            return;
        }
        // Installing a resource does not require making a backup.
        std::string backup = path + ".bak";
        bool keptBackup = EditorResources().Number(
            "editor.keep_backup", 0, 0, 1) >= 0.5f
            && access(path.c_str(), F_OK) == 0
            && rename(path.c_str(), backup.c_str()) == 0;
        if (rename(&filename[0], path.c_str()) != 0) {
            if (keptBackup)
                rename(backup.c_str(), path.c_str());
            unlink(&filename[0]);
            _Status(EditorResources().String("editor.status.image_install_failed", "Cannot install graphic. Previous resource restored."));
            return;
        }
        _Status(std::string(EditorResources().String(
            "editor.status.imported_prefix", "Imported ")) + asset.label
            + EditorResources().String("editor.status.imported_suffix",
                ". Restart the target application."));
    }

    bool _OwnerAllowsChanges(const std::string& target = "")
    {
        // Only the actual target path decides whether OSTen's System
        // Folder access mechanism applies. Other native application
        // folders use their normal filesystem permissions.
        const std::string& path = target.empty() ? fPath : target;
        if (path.compare(0, strlen("/boot/System Folder/"),
                "/boot/System Folder/") != 0)
            return true;
        BMessenger finder("application/x-vnd.OSTen-Finder");
        BMessage query(kOSTenQuerySystemFolderAccess);
        BMessage answer;
        bool unlocked = false;
        if (finder.SendMessage(&query, &answer, 500000, 500000) != B_OK
            || answer.FindBool("unlocked", &unlocked) != B_OK || !unlocked) {
            _Status(EditorResources().String("editor.status.system_locked", "Unlock the System Folder in Finder's Special menu first."));
            return false;
        }
        return true;
    }

    void _Save()
    {
        if (fPath.empty()) {
            _Status(EditorResources().String("editor.status.open_resource", "Open an application or .ostr resource file first."));
            return;
        }
        if (!_OwnerAllowsChanges())
            return;
        // Apply any visible property edit before persisting the manifest.
        if (fSelection >= 0 && (fLines[fSelection].value != fValue->Text()
            || fLines[fSelection].key != fKey->Text()))
            _Apply();
        if (!fDirty) {
            _Status(EditorResources().String("editor.status.no_changes", "No changes to save."));
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
            _Status(EditorResources().String("editor.status.save_temp_failed", "Save failed. Unlock System Folder and check permissions."));
            return;
        }
        struct stat previousManifest;
        bool hasManifest = stat(fPath.c_str(), &previousManifest) == 0;
        size_t offset = 0;
        bool success = !hasManifest
            || fchmod(fd, previousManifest.st_mode & 07777) == 0;
        while (success && offset < body.size()) {
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
            _Status(EditorResources().String("editor.status.save_write_failed", "Could not write changes. Original resources untouched."));
            return;
        }

        // The Owner decides whether to keep backups. A backup failure
        // must not veto a permitted resource modification.
        std::string backup = fPath + ".bak";
        bool keptBackup = EditorResources().Number(
            "editor.keep_backup", 0, 0, 1) >= 0.5f
            && rename(fPath.c_str(), backup.c_str()) == 0;
        if (rename(&name[0], fPath.c_str()) != 0) {
            if (keptBackup)
                rename(backup.c_str(), fPath.c_str());
            unlink(&name[0]);
            _Status(EditorResources().String("editor.status.save_install_failed", "Could not install edited resources. Original preserved."));
            return;
        }
        fDirty = false;
        _Status(keptBackup
            ? EditorResources().String("editor.status.saved_backup",
                "Saved with backup. Restart the edited app.")
            : EditorResources().String("editor.status.saved",
                "Saved. Restart the edited app."));
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
    std::vector<AssetDefinition> fAssets;
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

    void ReadyToRun() override
    {
        BWindow* window = StartOSTenResourceBrowser();
        fBrowser = BMessenger(window);
    }

    void MessageReceived(BMessage* message) override
    {
        if (message->what == kOSTenGlobalMenuQuery) {
            OSTenAnswerGlobalMenuQuery(message, EditorResources());
            return;
        }
        if (message->what == kOSTenGlobalMenuInvoke) {
            const char* action = NULL;
            if (message->FindString("action", &action) != B_OK)
                return;
            if (strcmp(action, "open") == 0) {
                BMessage open('rbOp');
                fBrowser.SendMessage(&open);
            } else if (strcmp(action, "refresh") == 0) {
                BMessage refresh('rbRf');
                fBrowser.SendMessage(&refresh);
            } else if (strcmp(action, "quit") == 0)
                PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BApplication::MessageReceived(message);
    }
private:
    BMessenger fBrowser;
};
}

void
OpenOSTenResourceProperty(const std::string& file,
    const std::string& application, const std::string& key)
{
    new EditorWindow(file, application, key);
}

int
main()
{
    ResourceEditorApp application;
    application.Run();
    return 0;
}
