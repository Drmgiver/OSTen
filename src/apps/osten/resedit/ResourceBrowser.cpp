/* SPDX-License-Identifier: MIT */
// OSTen's ResEdit-style document/type/resource browser. The .ostr settings
// catalog is a compatibility source until the native typed resource format
// is defined. Raw RGBA images are genuine editable resource entries.
#include "ResourceBrowser.h"
#include "OSTenMessages.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <FilePanel.h>
#include <InterfaceDefs.h>
#include <ListItem.h>
#include <ListView.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>

#include <algorithm>
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
    kOpenFile = 'rbOp', kFileChosen = 'rbFc', kOpenType = 'rbTy',
    kOpenEntry = 'rbEn', kRefresh = 'rbRf',
    kSavePixels = 'pxSv', kRevertPixels = 'pxRv',
    kUndoPixels = 'pxUn', kPixelsChanged = 'pxCh'
};

struct ResourceEntry {
    std::string type;
    std::string key;
    std::string label;
    std::string path;
    std::string application;
    int width;
    int height;
    bool artwork;
};

static bool
EndsWith(const std::string& text, const char* ending)
{
    size_t n = strlen(ending);
    return text.size() >= n && text.compare(text.size() - n, n, ending) == 0;
}

static std::string
Parent(const std::string& path)
{
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

static std::string
Leaf(const std::string& path)
{
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static bool
ExistingFile(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static bool
KnownDimensions(const std::string& name, int& width, int& height)
{
    struct Fixed { const char* name; int w; int h; };
    static const Fixed known[] = {
        {"disk", 48, 48}, {"trash-empty", 48, 48},
        {"trash-full", 48, 48}, {"folder", 40, 40},
        {"finder", 40, 40}, {"system", 40, 40},
        {"wordmark", 71, 20}, {"about-wordmark", 248, 69},
        {"switcher-finder", 22, 22}
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
        if (name == known[i].name) {
            width = known[i].w;
            height = known[i].h;
            return true;
        }
    }
    return false;
}

static bool
ReadPixels(const ResourceEntry& entry, std::vector<uint8>& bytes)
{
    if (!entry.artwork || entry.width <= 0 || entry.height <= 0
        || entry.width > 512 || entry.height > 512)
        return false;
    const size_t length = (size_t)entry.width * entry.height * 4;
    FILE* file = fopen(entry.path.c_str(), "rb");
    if (file == NULL)
        return false;
    bytes.resize(length);
    bool ok = fread(bytes.data(), 1, length, file) == length
        && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (!ok)
        bytes.clear();
    return ok;
}

static bool
OwnerMayWrite(const std::string& path)
{
    if (path.compare(0, strlen("/boot/System Folder/"),
            "/boot/System Folder/") != 0)
        return true;
    BMessenger finder("application/x-vnd.OSTen-Finder");
    BMessage query(kOSTenQuerySystemFolderAccess);
    BMessage reply;
    bool unlocked = false;
    if (finder.SendMessage(&query, &reply, 500000, 500000) != B_OK
        || reply.FindBool("unlocked", &unlocked) != B_OK || !unlocked) {
        (new BAlert("System Folder locked",
            "Unlock System Folder Access from Finder's Special menu first.",
            "OK"))->Go();
        return false;
    }
    return true;
}

static std::string
Classify(const std::string& key, const std::string& value)
{
    if (key.compare(0, 5, "menu.") == 0)
        return "MENU";
    if (key.compare(0, 9, "shortcut.") == 0)
        return "KEYS";
    if (key.find("font") != std::string::npos)
        return "FONT";
    float a, b, c, d;
    char trailing;
    if (sscanf(value.c_str(), "%f,%f,%f,%f%c",
            &a, &b, &c, &d, &trailing) == 4)
        return "RECT";
    int red, green, blue;
    if (sscanf(value.c_str(), "%d,%d,%d%c",
            &red, &green, &blue, &trailing) == 3
        && red >= 0 && red <= 255 && green >= 0 && green <= 255
        && blue >= 0 && blue <= 255)
        return "COLR";
    if (key.find("order") != std::string::npos)
        return "LIST";
    char* end = NULL;
    strtod(value.c_str(), &end);
    if (end != value.c_str() && *end == '\0')
        return "NUMB";
    return "TEXT";
}

// A thumbnail for real RGBA resources, not a fake static icon.
static BBitmap*
Thumbnail(const ResourceEntry& entry)
{
    std::vector<uint8> rgba;
    if (!ReadPixels(entry, rgba))
        return NULL;
    BBitmap* bitmap = new BBitmap(
        BRect(0, 0, entry.width - 1, entry.height - 1), B_RGBA32);
    if (bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return NULL;
    }
    uint8* dst = (uint8*)bitmap->Bits();
    const int row = bitmap->BytesPerRow();
    for (int y = 0; y < entry.height; ++y) {
        for (int x = 0; x < entry.width; ++x) {
            size_t index = ((size_t)y * entry.width + x) * 4;
            uint8* pixel = dst + (size_t)y * row + x * 4;
            pixel[0] = rgba[index + 2];
            pixel[1] = rgba[index + 1];
            pixel[2] = rgba[index];
            pixel[3] = rgba[index + 3];
        }
    }
    return bitmap;
}

class TypeItem : public BListItem {
public:
    TypeItem(const std::string& type, int count)
        : fType(type), fCount(count) {}

    void Update(BView* view, const BFont* font) override
    {
        BListItem::Update(view, font);
        SetHeight(60);
    }

    void DrawItem(BView* view, BRect rect, bool) override
    {
        view->SetHighColor(IsSelected() ? rgb_color{192, 217, 243, 255}
            : rgb_color{250, 250, 249, 255});
        view->FillRect(rect);
        BRect emblem(rect.left + 9, rect.top + 7,
            rect.left + 53, rect.top + 51);
        view->SetHighColor(225, 230, 236);
        view->FillRoundRect(emblem, 5, 5);
        view->SetHighColor(90, 100, 115);
        view->StrokeRoundRect(emblem, 5, 5);
        view->SetHighColor(25, 55, 85);
        view->DrawString(fType.c_str(),
            BPoint(rect.left + 13, rect.top + 33));
        view->SetHighColor(20, 20, 20);
        view->DrawString(fType.c_str(),
            BPoint(rect.left + 69, rect.top + 26));
        char details[72];
        snprintf(details, sizeof(details), "%d resources", fCount);
        view->SetHighColor(95, 95, 95);
        view->DrawString(details,
            BPoint(rect.left + 69, rect.top + 46));
    }

private:
    std::string fType;
    int fCount;
};

class EntryItem : public BListItem {
public:
    EntryItem(const ResourceEntry& entry)
        : fEntry(entry), fBitmap(entry.artwork ? Thumbnail(entry) : NULL) {}

    ~EntryItem() override { delete fBitmap; }

    void Update(BView* view, const BFont* font) override
    {
        BListItem::Update(view, font);
        SetHeight(64);
    }

    void DrawItem(BView* view, BRect rect, bool) override
    {
        view->SetHighColor(IsSelected() ? rgb_color{192, 217, 243, 255}
            : rgb_color{252, 252, 252, 255});
        view->FillRect(rect);
        if (fBitmap != NULL) {
            BRect picture(rect.left + 9, rect.top + 7,
                rect.left + 57, rect.top + 55);
            view->SetHighColor(233, 233, 233);
            view->FillRect(picture);
            view->SetDrawingMode(B_OP_ALPHA);
            view->DrawBitmap(fBitmap, fBitmap->Bounds(), picture);
            view->SetDrawingMode(B_OP_COPY);
        } else {
            BRect icon(rect.left + 14, rect.top + 10,
                rect.left + 49, rect.top + 49);
            view->SetHighColor(231, 234, 239);
            view->FillRect(icon);
            view->SetHighColor(100, 110, 120);
            view->StrokeRect(icon);
            view->DrawString("TXT", BPoint(rect.left + 19, rect.top + 34));
        }
        view->SetHighColor(25, 25, 25);
        view->DrawString(fEntry.label.c_str(),
            BPoint(rect.left + 75, rect.top + 29));
        char detail[100];
        if (fEntry.artwork)
            snprintf(detail, sizeof(detail), "%d x %d RGBA artwork",
                fEntry.width, fEntry.height);
        else
            snprintf(detail, sizeof(detail), "%s resource", fEntry.type.c_str());
        view->SetHighColor(95, 95, 95);
        view->DrawString(detail, BPoint(rect.left + 75, rect.top + 49));
    }

private:
    ResourceEntry fEntry;
    BBitmap* fBitmap;
};

static const rgb_color kPalette[] = {
    {0, 0, 0, 0}, {0, 0, 0, 255}, {255, 255, 255, 255},
    {220, 42, 47, 255}, {255, 140, 44, 255},
    {252, 214, 47, 255}, {76, 166, 72, 255},
    {38, 118, 188, 255}, {110, 67, 172, 255},
    {117, 80, 54, 255}, {180, 180, 180, 255},
    {88, 88, 88, 255}, {252, 190, 197, 255},
    {133, 206, 223, 255}, {105, 65, 110, 255},
    {222, 209, 175, 255}
};

class PixelCanvas : public BView {
public:
    PixelCanvas(BRect frame, const ResourceEntry& entry,
        const std::vector<uint8>& source)
        : BView(frame, "pixel-canvas", B_FOLLOW_ALL, B_WILL_DRAW),
          fEntry(entry), fPixels(source), fScale(1),
          fColor(1), fPainting(false), fDirty(false)
    {
        fScale = std::max(1, std::min(8, std::min(
            485 / entry.width, 480 / entry.height)));
        SetViewColor(245, 245, 245);
    }

    bool Dirty() const { return fDirty; }
    const std::vector<uint8>& Pixels() const { return fPixels; }

    void MarkSaved() { fDirty = false; }

    void Reload(const std::vector<uint8>& pixels)
    {
        fPixels = pixels;
        fDirty = false;
        Invalidate();
    }

    void Draw(BRect) override
    {
        // The canvas, a usable 1:1 preview, and a selectable color palette.
        SetHighColor(225, 225, 225);
        FillRect(Bounds());
        for (int y = 0; y < fEntry.height; ++y) {
            for (int x = 0; x < fEntry.width; ++x) {
                size_t i = ((size_t)y * fEntry.width + x) * 4;
                bool light = ((x / 4 + y / 4) % 2) == 0;
                uint8 bg = light ? 241 : 212;
                uint8 alpha = fPixels[i + 3];
                rgb_color c = {
                    (uint8)((fPixels[i] * alpha + bg * (255 - alpha)) / 255),
                    (uint8)((fPixels[i + 1] * alpha + bg * (255 - alpha)) / 255),
                    (uint8)((fPixels[i + 2] * alpha + bg * (255 - alpha)) / 255),
                    255
                };
                BRect cell(16 + x * fScale, 16 + y * fScale,
                    15 + (x + 1) * fScale, 15 + (y + 1) * fScale);
                SetHighColor(c);
                FillRect(cell);
                if (fScale >= 6) {
                    SetHighColor(125, 125, 125);
                    StrokeRect(cell);
                }
            }
        }
        SetHighColor(85, 85, 85);
        StrokeRect(BRect(15, 15, 16 + fEntry.width * fScale,
            16 + fEntry.height * fScale));
        SetHighColor(40, 40, 40);
        DrawString("Palette (click a color)", BPoint(545, 25));
        for (int i = 0; i < 16; ++i) {
            int x = 546 + (i % 4) * 34;
            int y = 39 + (i / 4) * 34;
            BRect swatch(x, y, x + 27, y + 27);
            SetHighColor(kPalette[i].alpha == 0
                ? rgb_color{248, 248, 248, 255} : kPalette[i]);
            FillRect(swatch);
            SetHighColor(i == fColor
                ? rgb_color{0, 95, 200, 255}
                : rgb_color{95, 95, 95, 255});
            SetPenSize(i == fColor ? 3 : 1);
            StrokeRect(swatch);
            SetPenSize(1);
            if (kPalette[i].alpha == 0) {
                SetHighColor(200, 40, 40);
                StrokeLine(BPoint(x + 2, y + 25),
                    BPoint(x + 25, y + 2));
            }
        }

        SetHighColor(40, 40, 40);
        DrawString("Actual pixels", BPoint(545, 224));
        int scale = std::max(1, std::min(2,
            std::min(198 / fEntry.width, 180 / fEntry.height)));
        for (int y = 0; y < fEntry.height; ++y) {
            for (int x = 0; x < fEntry.width; ++x) {
                size_t i = ((size_t)y * fEntry.width + x) * 4;
                if (fPixels[i + 3] == 0)
                    continue;
                SetHighColor(rgb_color{
                    fPixels[i], fPixels[i + 1], fPixels[i + 2], 255});
                FillRect(BRect(545 + x * scale, 237 + y * scale,
                    544 + (x + 1) * scale, 236 + (y + 1) * scale));
            }
        }
        SetHighColor(80, 80, 80);
        DrawString("Transparent swatch = eraser",
            BPoint(545, 460));
    }

    void MouseDown(BPoint where) override
    {
        if (where.x >= 546 && where.x < 546 + 4 * 34
            && where.y >= 39 && where.y < 39 + 4 * 34) {
            int col = (int)(where.x - 546) / 34;
            int row = (int)(where.y - 39) / 34;
            fColor = row * 4 + col;
            Invalidate();
            return;
        }
        if (_Paint(where)) {
            fPainting = true;
            SetMouseEventMask(B_POINTER_EVENTS);
        }
    }

    void MouseMoved(BPoint where, uint32, const BMessage*) override
    {
        if (fPainting)
            _Paint(where);
    }

    void MouseUp(BPoint) override { fPainting = false; }

private:
    bool _Paint(BPoint where)
    {
        int x = (int)(where.x - 16) / fScale;
        int y = (int)(where.y - 16) / fScale;
        if (where.x < 16 || where.y < 16
            || x < 0 || x >= fEntry.width || y < 0 || y >= fEntry.height)
            return false;
        size_t i = ((size_t)y * fEntry.width + x) * 4;
        const rgb_color& paint = kPalette[fColor];
        if (fPixels[i] == paint.red && fPixels[i + 1] == paint.green
            && fPixels[i + 2] == paint.blue && fPixels[i + 3] == paint.alpha)
            return true;
        fPixels[i] = paint.red;
        fPixels[i + 1] = paint.green;
        fPixels[i + 2] = paint.blue;
        fPixels[i + 3] = paint.alpha;
        fDirty = true;
        Invalidate();
        Window()->PostMessage(kPixelsChanged);
        return true;
    }

    ResourceEntry fEntry;
    std::vector<uint8> fPixels;
    int fScale;
    int fColor;
    bool fPainting;
    bool fDirty;
};

class PixelEditorWindow : public BWindow {
public:
    explicit PixelEditorWindow(const ResourceEntry& entry)
        : BWindow(BRect(135, 75, 905, 711),
              (entry.label + " — Pixel Editor").c_str(), B_TITLED_WINDOW, 0),
          fEntry(entry), fCanvas(NULL), fStatus(NULL)
    {
        std::vector<uint8> rgba;
        if (!ReadPixels(entry, rgba)) {
            (new BAlert("Cannot open artwork",
                "The RGBA image is missing or has the wrong dimensions.",
                "OK"))->Go();
            PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(239, 239, 236);
        AddChild(background);
        fCanvas = new PixelCanvas(BRect(10, 10, 758, 548), entry, rgba);
        background->AddChild(fCanvas);
        char heading[256];
        snprintf(heading, sizeof(heading),
            "%s  |  %d x %d RGBA  |  paint with mouse",
            entry.label.c_str(), entry.width, entry.height);
        fStatus = new BStringView(BRect(20, 557, 745, 584),
            "pixel-status", heading);
        background->AddChild(fStatus);
        background->AddChild(new BButton(BRect(390, 590, 505, 624),
            "undo-pixel", "Undo Save", new BMessage(kUndoPixels)));
        background->AddChild(new BButton(BRect(511, 590, 614, 624),
            "revert-pixel", "Revert", new BMessage(kRevertPixels)));
        background->AddChild(new BButton(BRect(626, 590, 744, 624),
            "save-pixel", "Save", new BMessage(kSavePixels)));
        Show();
    }

    bool QuitRequested() override
    {
        return fCanvas == NULL || !fCanvas->Dirty()
            || (new BAlert("Unsaved icon",
                "Discard the unsaved pixel changes?",
                "Cancel", "Discard"))->Go() == 1;
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case kPixelsChanged:
                fStatus->SetText("Pixel artwork changed — Save to write.");
                return;
            case kSavePixels: _Save(); return;
            case kRevertPixels: _Revert(); return;
            case kUndoPixels: _UndoSave(); return;
        }
        BWindow::MessageReceived(message);
    }

private:
    void _Revert()
    {
        std::vector<uint8> bytes;
        if (ReadPixels(fEntry, bytes)) {
            fCanvas->Reload(bytes);
            fStatus->SetText("Reloaded saved artwork.");
        } else
            fStatus->SetText("Unable to reload original artwork.");
    }

    void _UndoSave()
    {
        if (!OwnerMayWrite(fEntry.path))
            return;
        std::string backup = fEntry.path + ".bak";
        if (!ExistingFile(backup)) {
            fStatus->SetText("No saved backup exists.");
            return;
        }
        std::string swap = fEntry.path + ".undo.XXXXXX";
        std::vector<char> temp(swap.begin(), swap.end());
        temp.push_back('\0');
        int fd = mkstemp(temp.data());
        if (fd < 0) {
            fStatus->SetText("Could not prepare undo.");
            return;
        }
        close(fd);
        unlink(temp.data());
        if (rename(fEntry.path.c_str(), temp.data()) != 0
            || rename(backup.c_str(), fEntry.path.c_str()) != 0) {
            rename(temp.data(), fEntry.path.c_str());
            fStatus->SetText("Unable to restore backup.");
            return;
        }
        rename(temp.data(), backup.c_str());
        _Revert();
        fStatus->SetText("Previous artwork restored; restart the target app.");
    }

    void _Save()
    {
        if (!fCanvas->Dirty()) {
            fStatus->SetText("No pixel changes to save.");
            return;
        }
        if (!OwnerMayWrite(fEntry.path))
            return;
        const std::vector<uint8>& pixels = fCanvas->Pixels();
        std::string tempName = fEntry.path + ".tmp.XXXXXX";
        std::vector<char> temp(tempName.begin(), tempName.end());
        temp.push_back('\0');
        int fd = mkstemp(temp.data());
        if (fd < 0) {
            fStatus->SetText("Could not create temporary image file.");
            return;
        }
        size_t done = 0;
        bool ok = true;
        while (done < pixels.size()) {
            ssize_t n = write(fd, pixels.data() + done, pixels.size() - done);
            if (n <= 0) { ok = false; break; }
            done += (size_t)n;
        }
        if (fsync(fd) != 0)
            ok = false;
        if (close(fd) != 0)
            ok = false;
        if (!ok) {
            unlink(temp.data());
            fStatus->SetText("Could not write image; original preserved.");
            return;
        }
        std::string backup = fEntry.path + ".bak";
        if (rename(fEntry.path.c_str(), backup.c_str()) != 0) {
            unlink(temp.data());
            fStatus->SetText("Could not back up original artwork.");
            return;
        }
        if (rename(temp.data(), fEntry.path.c_str()) != 0) {
            rename(backup.c_str(), fEntry.path.c_str());
            unlink(temp.data());
            fStatus->SetText("Could not install image; original restored.");
            return;
        }
        fCanvas->MarkSaved();
        fStatus->SetText("Artwork saved. Restart the edited application.");
    }

    ResourceEntry fEntry;
    PixelCanvas* fCanvas;
    BStringView* fStatus;
};

class ResourceListWindow : public BWindow {
public:
    ResourceListWindow(const std::string& kind,
        const std::vector<ResourceEntry>& resources)
        : BWindow(BRect(125, 105, 705, 605),
              (kind + " — Resources").c_str(), B_TITLED_WINDOW, 0),
          fResources(resources)
    {
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(238, 238, 236);
        AddChild(background);
        background->AddChild(new BStringView(BRect(15, 8, 555, 36),
            "info", "Double-click a resource to open its editor"));
        fList = new BListView(BRect(12, 40, 554, 481),
            "resource-entries", B_SINGLE_SELECTION_LIST);
        fList->SetInvocationMessage(new BMessage(kOpenEntry));
        background->AddChild(new BScrollView("resource-scroll", fList,
            B_FOLLOW_ALL, 0, false, true));
        for (size_t i = 0; i < resources.size(); ++i)
            fList->AddItem(new EntryItem(resources[i]));
        Show();
    }

    void MessageReceived(BMessage* message) override
    {
        if (message->what == kOpenEntry) {
            int32 index = fList->CurrentSelection();
            if (index < 0 || (size_t)index >= fResources.size())
                return;
            const ResourceEntry& item = fResources[index];
            if (item.artwork)
                new PixelEditorWindow(item);
            else
                OpenOSTenResourceProperty(item.path, item.application,
                    item.key);
            return;
        }
        BWindow::MessageReceived(message);
    }

private:
    std::vector<ResourceEntry> fResources;
    BListView* fList;
};

class ResourceBrowserWindow : public BWindow {
public:
    ResourceBrowserWindow()
        : BWindow(BRect(90, 75, 620, 595),
              "OSTen ResEdit — Resource Types", B_TITLED_WINDOW,
              B_NORMAL_WINDOW_FEEL, B_QUIT_ON_WINDOW_CLOSE),
          fPicker(NULL)
    {
        BView* bg = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        bg->SetViewColor(239, 239, 237);
        AddChild(bg);
        bg->AddChild(new BButton(BRect(12, 10, 117, 43),
            "open", "Open...", new BMessage(kOpenFile)));
        bg->AddChild(new BButton(BRect(125, 10, 226, 43),
            "refresh", "Refresh", new BMessage(kRefresh)));
        fCaption = new BStringView(BRect(235, 11, 500, 41),
            "current", "No file opened");
        bg->AddChild(fCaption);
        bg->AddChild(new BStringView(BRect(18, 47, 502, 72),
            "hint", "Double-click a resource type to browse its contents"));
        fTypesList = new BListView(BRect(12, 77, 501, 486),
            "resource-types", B_SINGLE_SELECTION_LIST);
        fTypesList->SetInvocationMessage(new BMessage(kOpenType));
        bg->AddChild(new BScrollView("types-scroll", fTypesList,
            B_FOLLOW_ALL, 0, false, true));
        Show();
        _PickFile();
    }

    ~ResourceBrowserWindow() override { delete fPicker; }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case kOpenFile: _PickFile(); return;
            case kFileChosen: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK)
                    _Open(ref);
                Activate(true);
                return;
            }
            case B_CANCEL:
                Activate(true);
                return;
            case kRefresh: _Scan(); return;
            case kOpenType: {
                int32 selection = fTypesList->CurrentSelection();
                if (selection < 0 || (size_t)selection >= fTypeNames.size())
                    return;
                std::string type = fTypeNames[selection];
                std::vector<ResourceEntry> items;
                for (size_t i = 0; i < fEntries.size(); ++i) {
                    if (fEntries[i].type == type)
                        items.push_back(fEntries[i]);
                }
                new ResourceListWindow(type, items);
                return;
            }
        }
        BWindow::MessageReceived(message);
    }

private:
    void _PickFile()
    {
        if (fPicker == NULL) {
            BMessage open(kFileChosen);
            fPicker = new BFilePanel(B_OPEN_PANEL, new BMessenger(this),
                NULL, B_FILE_NODE | B_DIRECTORY_NODE, false, &open);
            BEntry boot("/boot");
            entry_ref bootRef;
            if (boot.GetRef(&bootRef) == B_OK)
                fPicker->SetPanelDirectory(&bootRef);
        }
        fPicker->Show();
    }

    void _Open(const entry_ref& ref)
    {
        BEntry entry(&ref, true);
        BPath selected;
        if (entry.GetPath(&selected) != B_OK)
            return;
        std::string path(selected.Path());
        std::string name(selected.Leaf());
        std::string application = name;
        std::vector<std::string> candidates;

        if (EndsWith(name, ".ostr"))
            candidates.push_back(path);
        else {
            if (entry.IsDirectory()) {
                candidates.push_back(path + "/Interface.ostr");
                candidates.push_back(path + "/Resources/Interface.ostr");
            } else {
                candidates.push_back(path + ".ostr");
                candidates.push_back(path + ".resources/Interface.ostr");
            }
            if (name == "Finder" || name == "OSTenFinder")
                application = "Finder";
            else if (name == "System" || name == "OSTenSystem")
                application = "System";
            else if (name == "Resource Editor" || name == "OSTenResourceEditor")
                application = "ResourceEditor";
            candidates.push_back("/boot/System Folder/Resources/"
                + application + "/Interface.ostr");
        }

        for (size_t i = 0; i < candidates.size(); ++i) {
            if (!ExistingFile(candidates[i]))
                continue;
            fPath = candidates[i];
            fApplication = EndsWith(name, ".ostr")
                ? Leaf(Parent(fPath)) : application;
            _Scan();
            return;
        }
        (new BAlert("Unsupported application",
            "This selection has no editable OSTen resource catalog.\n"
            "Choose a native OSTen app or an .ostr file. Editing arbitrary "
            "compiled Haiku resources is not implemented yet.",
            "OK"))->Go();
    }

    void _Scan()
    {
        if (fPath.empty())
            return;
        FILE* file = fopen(fPath.c_str(), "rb");
        if (file == NULL) {
            (new BAlert("Resource file unavailable",
                "Could not read the selected resource manifest.", "OK"))->Go();
            return;
        }
        std::vector<ResourceEntry> entries;
        char line[8192];
        while (fgets(line, sizeof(line), file) != NULL) {
            if (line[0] == '#' || line[0] == ';')
                continue;
            char* equals = strchr(line, '=');
            if (equals == NULL || equals == line)
                continue;
            *equals = '\0';
            char* value = equals + 1;
            value[strcspn(value, "\r\n")] = '\0';
            ResourceEntry resource;
            resource.type = Classify(line, value);
            resource.key = line;
            resource.label = line;
            resource.path = fPath;
            resource.application = fApplication;
            resource.width = 0;
            resource.height = 0;
            resource.artwork = false;
            entries.push_back(resource);
        }
        bool readOK = !ferror(file);
        fclose(file);
        if (!readOK) {
            (new BAlert("Resource read failed",
                "The resource manifest could not be read completely.",
                "OK"))->Go();
            return;
        }

        // Browse actual .rgba data in the document's resource directory.
        // The initial 6 Finder files are byte-for-byte approved source art.
        BDirectory directory(Parent(fPath).c_str());
        BEntry image;
        while (directory.GetNextEntry(&image, true) == B_OK) {
            if (!image.IsFile())
                continue;
            BPath imagePath;
            if (image.GetPath(&imagePath) != B_OK)
                continue;
            std::string resourceFile(imagePath.Path());
            if (!EndsWith(resourceFile, ".rgba"))
                continue;
            std::string basename = Leaf(resourceFile);
            basename.resize(basename.size() - strlen(".rgba"));
            struct stat st;
            if (stat(resourceFile.c_str(), &st) != 0
                || st.st_size <= 0 || st.st_size % 4 != 0)
                continue;
            int width = 0, height = 0;
            if (!KnownDimensions(basename, width, height)) {
                double side = sqrt((double)st.st_size / 4.0);
                width = (int)(side + 0.5);
                height = width;
            }
            if (width <= 0 || height <= 0 || width > 512 || height > 512
                || (size_t)st.st_size != (size_t)width * height * 4)
                continue;
            ResourceEntry resource;
            resource.type = width == height ? "ICON" : "IMAG";
            resource.key = basename;
            resource.label = basename;
            resource.path = resourceFile;
            resource.application = fApplication;
            resource.width = width;
            resource.height = height;
            resource.artwork = true;
            entries.push_back(resource);
        }

        fEntries.swap(entries);
        fTypeNames.clear();
        while (fTypesList->CountItems() > 0)
            delete fTypesList->RemoveItem((int32)0);
        for (size_t i = 0; i < fEntries.size(); ++i) {
            const std::string& type = fEntries[i].type;
            if (std::find(fTypeNames.begin(), fTypeNames.end(), type)
                    == fTypeNames.end())
                fTypeNames.push_back(type);
        }
        std::sort(fTypeNames.begin(), fTypeNames.end());
        for (size_t i = 0; i < fTypeNames.size(); ++i) {
            int count = 0;
            for (size_t j = 0; j < fEntries.size(); ++j) {
                if (fEntries[j].type == fTypeNames[i])
                    ++count;
            }
            fTypesList->AddItem(new TypeItem(fTypeNames[i], count));
        }
        fCaption->SetText(fApplication.c_str());
        SetTitle((fApplication + " — Resource Types").c_str());
    }

    BFilePanel* fPicker;
    BListView* fTypesList;
    BStringView* fCaption;
    std::string fPath;
    std::string fApplication;
    std::vector<ResourceEntry> fEntries;
    std::vector<std::string> fTypeNames;
};

} // namespace

void
StartOSTenResourceBrowser()
{
    new ResourceBrowserWindow();
}
