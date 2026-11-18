/* SPDX-License-Identifier: MIT */
// OSTen's ResEdit-style document/type/resource browser. The .ostr settings
// catalog is a compatibility source until the native typed resource format
// is defined. Raw RGBA images are genuine editable resource entries.
#include "ResourceBrowser.h"
#include "OSTenMessages.h"
#include "OSTenResources.h"
#include "OSTenResourceIndex.h"
#include "OSTenDialogLayout.h"

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
#include <TextControl.h>
#include <Slider.h>
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
#include <map>
#include <set>

namespace {

static const OSTenResourceCatalog& BrowserUI()
{
    static OSTenResourceCatalog ui("ResourceEditor");
    return ui;
}

// The catalog is the authoritative control type, not the C++ pointer cast.
// If the Owner removes or retypes a required control, its editor stays open
// without dereferencing a view of the wrong type.
template<typename T>
static T*
BrowserComposedControl(const OSTenDialogControls& controls, const char* prefix,
    const char* id, const char* expectedType)
{
    const std::string key = std::string(prefix) + ".control." + id + ".type";
    if (strcmp(BrowserUI().String(key.c_str(), ""), expectedType) != 0)
        return NULL;
    OSTenDialogControls::const_iterator found = controls.find(id);
    return found == controls.end() ? NULL : static_cast<T*>(found->second);
}

enum {
    kOpenFile = 'rbOp', kFileChosen = 'rbFc', kOpenType = 'rbTy',
    kOpenEntry = 'rbEn', kRefresh = 'rbRf',
    kSavePixels = 'pxSv', kRevertPixels = 'pxRv',
    kUndoPixels = 'pxUn', kPixelsChanged = 'pxCh',
    kMenuSelected = 'mnSl', kMenuAdd = 'mnAd', kMenuRemove = 'mnRm',
    kMenuUp = 'mnUp', kMenuDown = 'mnDn', kMenuSave = 'mnSv',
    kMenuRevert = 'mnRv', kMenuSeparator = 'mnSp', kMenuReplace = 'mnRp',
    kColorChange = 'clCh', kColorSave = 'clSv', kColorRevert = 'clRv',
    kLayoutSave = 'lySv', kLayoutRevert = 'lyRv', kLayoutSelected = 'lySl',
    kLayoutApply = 'lyAp', kLayoutAdd = 'lyAd',
    kLayoutRemove = 'lyRm', kLayoutUp = 'lyUp', kLayoutDown = 'lyDn',
    kAssetImport = 'asIm', kAssetChosen = 'asCh'
};

struct ResourceEntry {
    uint32_t id;
    std::string type;
    std::string key;
    std::string label;
    std::string path;
    std::string application;
    int width;
    int height;
    bool artwork;
    bool binaryAsset;
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


// Native non-RGBA assets are file-backed resources with indexed stable IDs.
// Their bytes stay in the selected application's folder, not its executable.
// This list is an open format registry; unknown indexed types still use the
// ordinary generic property inspector rather than being misidentified.
static bool
IsNativeBinaryAsset(const std::string& suffix, const std::string& type)
{
    return (type == "SOUN"
            && (suffix == ".wav" || suffix == ".aiff"
                || suffix == ".ogg" || suffix == ".flac"))
        || (type == "CURS"
            && (suffix == ".cur" || suffix == ".cursor"))
        || (type == "VECT"
            && (suffix == ".hvif" || suffix == ".svg"))
        || (type == "FONT"
            && (suffix == ".ttf" || suffix == ".otf"));
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
        (new BAlert(BrowserUI().String("browser.alert.locked_title", "System Folder locked"),
            BrowserUI().String("browser.alert.locked_message", "Unlock System Folder Access from Finder's Special menu first."),
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
        SetHeight(BrowserUI().Number("browser.types.row_height",
            60, 1, 1024));
    }

    void DrawItem(BView* view, BRect rect, bool) override
    {
        const OSTenResourceCatalog& ui = BrowserUI();
        view->SetHighColor(IsSelected()
            ? ui.Color("browser.types.selected",
                rgb_color{192, 217, 243, 255})
            : ui.Color("browser.types.item_background",
                rgb_color{250, 250, 249, 255}));
        view->FillRect(rect);
        BRect emblem = ui.Rect("browser.types.emblem_frame",
            BRect(9, 7, 53, 51));
        emblem.OffsetBy(rect.left, rect.top);
        const float radius = ui.Number(
            "browser.types.emblem_radius", 5, 0, 64);
        view->SetHighColor(ui.Color("browser.types.emblem_fill",
            rgb_color{225, 230, 236, 255}));
        view->FillRoundRect(emblem, radius, radius);
        view->SetHighColor(ui.Color("browser.types.emblem_outline",
            rgb_color{90, 100, 115, 255}));
        view->StrokeRoundRect(emblem, radius, radius);
        view->SetHighColor(ui.Color("browser.types.emblem_text",
            rgb_color{25, 55, 85, 255}));
        const BPoint small = ui.Point(
            "browser.types.emblem_text_at", BPoint(13, 33));
        view->DrawString(fType.c_str(),
            BPoint(rect.left + small.x, rect.top + small.y));
        view->SetHighColor(ui.Color("browser.types.title_color",
            rgb_color{20, 20, 20, 255}));
        const BPoint title = ui.Point(
            "browser.types.title_at", BPoint(69, 26));
        view->DrawString(fType.c_str(),
            BPoint(rect.left + title.x, rect.top + title.y));
        char count[32];
        snprintf(count, sizeof(count), "%d", fCount);
        const std::string description = std::string(count) + ui.String(
            "browser.types.count_suffix", " resources");
        view->SetHighColor(ui.Color("browser.types.detail_color",
            rgb_color{95, 95, 95, 255}));
        const BPoint detail = ui.Point(
            "browser.types.detail_at", BPoint(69, 46));
        view->DrawString(description.c_str(),
            BPoint(rect.left + detail.x, rect.top + detail.y));
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
        SetHeight(BrowserUI().Number("browser.entries.row_height",
            64, 1, 1024));
    }

    void DrawItem(BView* view, BRect rect, bool) override
    {
        const OSTenResourceCatalog& ui = BrowserUI();
        view->SetHighColor(IsSelected()
            ? ui.Color("browser.entries.selected",
                rgb_color{192, 217, 243, 255})
            : ui.Color("browser.entries.item_background",
                rgb_color{252, 252, 252, 255}));
        view->FillRect(rect);
        if (fBitmap != NULL) {
            BRect picture = ui.Rect("browser.entries.preview_frame",
                BRect(9, 7, 57, 55));
            picture.OffsetBy(rect.left, rect.top);
            view->SetHighColor(ui.Color("browser.entries.preview_background",
                rgb_color{233, 233, 233, 255}));
            view->FillRect(picture);
            view->SetDrawingMode(B_OP_ALPHA);
            view->DrawBitmap(fBitmap, fBitmap->Bounds(), picture);
            view->SetDrawingMode(B_OP_COPY);
        } else {
            BRect icon = ui.Rect("browser.entries.icon_frame",
                BRect(14, 10, 49, 49));
            icon.OffsetBy(rect.left, rect.top);
            view->SetHighColor(ui.Color("browser.entries.icon_fill",
                rgb_color{231, 234, 239, 255}));
            view->FillRect(icon);
            view->SetHighColor(ui.Color("browser.entries.icon_outline",
                rgb_color{100, 110, 120, 255}));
            view->StrokeRect(icon);
            const BPoint text = ui.Point("browser.entries.icon_text_at",
                BPoint(19, 34));
            view->DrawString(ui.String(
                "browser.entries.generic_icon", "TXT"),
                BPoint(rect.left + text.x, rect.top + text.y));
        }
        view->SetHighColor(ui.Color("browser.entries.title_color",
            rgb_color{25, 25, 25, 255}));
        const BPoint title = ui.Point(
            "browser.entries.title_at", BPoint(75, 29));
        view->DrawString(fEntry.label.c_str(),
            BPoint(rect.left + title.x, rect.top + title.y));
        char dimensions[80];
        if (fEntry.artwork) {
            snprintf(dimensions, sizeof(dimensions), "%d x %d",
                fEntry.width, fEntry.height);
        } else
            snprintf(dimensions, sizeof(dimensions), "%s",
                fEntry.type.c_str());
        const std::string description = std::string(dimensions)
            + ui.String(fEntry.artwork ? "browser.entries.rgba_suffix"
                : "browser.entries.resource_suffix",
                fEntry.artwork ? " RGBA artwork" : " resource");
        char identifier[24];
        snprintf(identifier, sizeof(identifier), "%08lx",
            (unsigned long)fEntry.id);
        const std::string detail = description
            + ui.String("browser.entries.id_prefix", "  [ID ")
            + identifier + ui.String("browser.entries.id_suffix", "]");
        view->SetHighColor(ui.Color("browser.entries.detail_color",
            rgb_color{95, 95, 95, 255}));
        const BPoint detailAt = ui.Point(
            "browser.entries.detail_at", BPoint(75, 49));
        view->DrawString(detail.c_str(),
            BPoint(rect.left + detailAt.x, rect.top + detailAt.y));
    }

private:
    ResourceEntry fEntry;
    BBitmap* fBitmap;
};

static const rgb_color kDefaultPalette[] = {
    {0, 0, 0, 0}, {0, 0, 0, 255}, {255, 255, 255, 255},
    {220, 42, 47, 255}, {255, 140, 44, 255},
    {252, 214, 47, 255}, {76, 166, 72, 255},
    {38, 118, 188, 255}, {110, 67, 172, 255},
    {117, 80, 54, 255}, {180, 180, 180, 255},
    {88, 88, 88, 255}, {252, 190, 197, 255},
    {133, 206, 223, 255}, {105, 65, 110, 255},
    {222, 209, 175, 255}
};

static rgb_color
PaletteColor(int slot)
{
    if (slot <= 0) return rgb_color{0, 0, 0, 0};
    if (slot >= 16) return rgb_color{0, 0, 0, 255};
    char key[64];
    snprintf(key, sizeof(key), "browser.palette.%02d", slot);
    return BrowserUI().Color(key, kDefaultPalette[slot]);
}

class PixelCanvas : public BView {
public:
    PixelCanvas(BRect frame, const ResourceEntry& entry,
        const std::vector<uint8>& source, const char* identifier)
        : BView(frame, identifier, B_FOLLOW_ALL, B_WILL_DRAW),
          fEntry(entry), fPixels(source), fScale(1),
          fColor(1), fPainting(false), fDirty(false)
    {
        // The Owner chooses the drawing area and scaling limits; the
        // editor uses the same saved values for rendering and hit-testing.
        const float maximumWidth = BrowserUI().Number(
            "browser.pixel.paint_max_width", 485, 1, 4096);
        const float maximumHeight = BrowserUI().Number(
            "browser.pixel.paint_max_height", 480, 1, 4096);
        const int maximumScale = (int)BrowserUI().Number(
            "browser.pixel.paint_max_scale", 8, 1, 32);
        fScale = std::max(1, std::min(maximumScale, std::min(
            (int)(maximumWidth / entry.width),
            (int)(maximumHeight / entry.height))));
        SetViewColor(BrowserUI().Color("browser.pixel.canvas_background",
            rgb_color{245, 245, 245, 255}));
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
        const OSTenResourceCatalog& ui = BrowserUI();
        const BPoint origin = ui.Point("browser.pixel.paint_origin",
            BPoint(16, 16));
        const rgb_color checkerLight = ui.Color(
            "browser.pixel.checker_light", rgb_color{241, 241, 241, 255});
        const rgb_color checkerDark = ui.Color(
            "browser.pixel.checker_dark", rgb_color{212, 212, 212, 255});
        const int checkerSize = (int)ui.Number(
            "browser.pixel.checker_size", 4, 1, 128);
        const int gridMinScale = (int)ui.Number(
            "browser.pixel.grid_min_scale", 6, 1, 32);

        SetHighColor(ui.Color("browser.pixel.board_background",
            rgb_color{225, 225, 225, 255}));
        FillRect(Bounds());
        for (int y = 0; y < fEntry.height; ++y) {
            for (int x = 0; x < fEntry.width; ++x) {
                const size_t i = ((size_t)y * fEntry.width + x) * 4;
                const rgb_color back =
                    ((x / checkerSize + y / checkerSize) % 2) == 0
                    ? checkerLight : checkerDark;
                const uint8 alpha = fPixels[i + 3];
                const rgb_color composed = {
                    (uint8)((fPixels[i] * alpha + back.red * (255 - alpha)) / 255),
                    (uint8)((fPixels[i + 1] * alpha
                        + back.green * (255 - alpha)) / 255),
                    (uint8)((fPixels[i + 2] * alpha
                        + back.blue * (255 - alpha)) / 255),
                    255
                };
                BRect cell(origin.x + x * fScale,
                    origin.y + y * fScale,
                    origin.x - 1 + (x + 1) * fScale,
                    origin.y - 1 + (y + 1) * fScale);
                SetHighColor(composed);
                FillRect(cell);
                if (fScale >= gridMinScale) {
                    SetHighColor(ui.Color("browser.pixel.grid_color",
                        rgb_color{125, 125, 125, 255}));
                    StrokeRect(cell);
                }
            }
        }
        SetHighColor(ui.Color("browser.pixel.paint_border",
            rgb_color{85, 85, 85, 255}));
        StrokeRect(BRect(origin.x - 1, origin.y - 1,
            origin.x + fEntry.width * fScale,
            origin.y + fEntry.height * fScale));

        SetHighColor(ui.Color("browser.pixel.label_color",
            rgb_color{40, 40, 40, 255}));
        DrawString(ui.String("browser.pixel.palette_hint",
            "Palette (click a color)"),
            ui.Point("browser.pixel.palette_hint_point", BPoint(545, 25)));
        for (int i = 0; i < 16; ++i) {
            const BRect swatch = _SwatchRect(i);
            SetHighColor(i == 0
                ? ui.Color("browser.pixel.transparent_swatch",
                    rgb_color{248, 248, 248, 255})
                : PaletteColor(i));
            FillRect(swatch);
            SetHighColor(i == fColor
                ? ui.Color("browser.pixel.selected_border",
                    rgb_color{0, 95, 200, 255})
                : ui.Color("browser.pixel.swatch_border",
                    rgb_color{95, 95, 95, 255}));
            SetPenSize(i == fColor
                ? ui.Number("browser.pixel.selected_border_width", 3, 1, 12)
                : ui.Number("browser.pixel.swatch_border_width", 1, 1, 12));
            StrokeRect(swatch);
            SetPenSize(1);
            if (i == 0) {
                const float inset = ui.Number(
                    "browser.pixel.eraser_inset", 2, 0, 32);
                SetHighColor(ui.Color("browser.pixel.eraser_stroke",
                    rgb_color{200, 40, 40, 255}));
                StrokeLine(BPoint(swatch.left + inset,
                        swatch.bottom - inset),
                    BPoint(swatch.right - inset,
                        swatch.top + inset));
            }
        }

        SetHighColor(ui.Color("browser.pixel.label_color",
            rgb_color{40, 40, 40, 255}));
        DrawString(ui.String("browser.pixel.actual_hint",
            "Actual pixels"),
            ui.Point("browser.pixel.actual_hint_point", BPoint(545, 224)));
        const BPoint previewOrigin = ui.Point(
            "browser.pixel.preview_origin", BPoint(545, 237));
        const int previewScale = std::max(1, std::min(
            (int)ui.Number("browser.pixel.preview_max_scale", 2, 1, 32),
            std::min(
                (int)(ui.Number("browser.pixel.preview_max_width",
                    198, 1, 4096) / fEntry.width),
                (int)(ui.Number("browser.pixel.preview_max_height",
                    180, 1, 4096) / fEntry.height))));
        for (int y = 0; y < fEntry.height; ++y) {
            for (int x = 0; x < fEntry.width; ++x) {
                const size_t i = ((size_t)y * fEntry.width + x) * 4;
                if (fPixels[i + 3] == 0)
                    continue;
                SetHighColor(rgb_color{
                    fPixels[i], fPixels[i + 1], fPixels[i + 2], 255});
                FillRect(BRect(previewOrigin.x + x * previewScale,
                    previewOrigin.y + y * previewScale,
                    previewOrigin.x - 1 + (x + 1) * previewScale,
                    previewOrigin.y - 1 + (y + 1) * previewScale));
            }
        }
        SetHighColor(ui.Color("browser.pixel.footer_color",
            rgb_color{80, 80, 80, 255}));
        DrawString(ui.String("browser.pixel.transparent_hint",
            "Transparent swatch = eraser"),
            ui.Point("browser.pixel.transparent_hint_point",
                BPoint(545, 460)));
    }

    void MouseDown(BPoint where) override
    {
        // A swatch is clickable exactly where it is drawn. Moving or
        // resizing its resource geometry cannot strand the hit targets.
        for (int i = 0; i < 16; ++i) {
            if (_SwatchRect(i).Contains(where)) {
                fColor = i;
                Invalidate();
                return;
            }
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
    BRect _SwatchRect(int slot) const
    {
        const OSTenResourceCatalog& ui = BrowserUI();
        const BPoint origin = ui.Point(
            "browser.pixel.palette_origin", BPoint(546, 39));
        const int columns = (int)ui.Number(
            "browser.pixel.palette_columns", 4, 1, 16);
        const float spacing = ui.Number(
            "browser.pixel.palette_spacing", 34, 1, 512);
        const float size = ui.Number(
            "browser.pixel.palette_swatch_size", 27, 1, 256);
        const float x = origin.x + (slot % columns) * spacing;
        const float y = origin.y + (slot / columns) * spacing;
        return BRect(x, y, x + size, y + size);
    }

    bool _Paint(BPoint where)
    {
        const BPoint origin = BrowserUI().Point(
            "browser.pixel.paint_origin", BPoint(16, 16));
        if (where.x < origin.x || where.y < origin.y)
            return false;
        const int x = (int)((where.x - origin.x) / fScale);
        const int y = (int)((where.y - origin.y) / fScale);
        if (x < 0 || x >= fEntry.width || y < 0 || y >= fEntry.height)
            return false;
        const size_t i = ((size_t)y * fEntry.width + x) * 4;
        const rgb_color paint = PaletteColor(fColor);
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

struct PixelCanvasFactoryContext {
    const ResourceEntry* entry;
    const std::vector<uint8>* pixels;
};

static BView*
CreateBrowserPixelCanvas(BRect frame, const char* identifier, const char*,
    uint32, const OSTenResourceCatalog&, const std::string&, void* context)
{
    PixelCanvasFactoryContext* data
        = static_cast<PixelCanvasFactoryContext*>(context);
    if (data == NULL || data->entry == NULL || data->pixels == NULL)
        return NULL;
    return new PixelCanvas(frame, *data->entry, *data->pixels, identifier);
}

class PixelEditorWindow : public BWindow {
public:
    explicit PixelEditorWindow(const ResourceEntry& entry)
        : BWindow(BrowserUI().Rect("browser.pixel.window", BRect(135, 75, 905, 711)),
              (entry.label + BrowserUI().String("browser.pixel.title_suffix", " — Pixel Editor")).c_str(), B_TITLED_WINDOW, 0),
          fEntry(entry), fCanvas(NULL), fStatus(NULL)
    {
        std::vector<uint8> rgba;
        if (!ReadPixels(entry, rgba)) {
            (new BAlert(BrowserUI().String("browser.alert.cannot_open_title", "Cannot open artwork"),
                BrowserUI().String("browser.alert.cannot_open_message", "The RGBA image is missing or has the wrong dimensions."),
                "OK"))->Go();
            PostMessage(B_QUIT_REQUESTED);
            return;
        }
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(BrowserUI().Color("browser.pixel.background", rgb_color{239, 239, 236, 255}));
        AddChild(background);
        const OSTenDialogAction actions[] = {
            {"pixel.undo", kUndoPixels},
            {"pixel.revert", kRevertPixels},
            {"pixel.save", kSavePixels}
        };
        PixelCanvasFactoryContext state = {&entry, &rgba};
        OSTenDialogControlFactories factories;
        factories["pixel_canvas"] = &CreateBrowserPixelCanvas;
        OSTenDialogControls controls = OSTenCreateDialogControls(
            background, BrowserUI(), "browser.pixel", actions,
            sizeof(actions) / sizeof(actions[0]), &factories, &state);
        fCanvas = BrowserComposedControl<PixelCanvas>(
            controls, "browser.pixel", "canvas", "pixel_canvas");
        fStatus = BrowserComposedControl<BStringView>(
            controls, "browser.pixel", "status", "label");
        if (fStatus != NULL) {
            char heading[256];
            snprintf(heading, sizeof(heading),
                "%s  |  %d x %d RGBA  |  paint with mouse",
                entry.label.c_str(), entry.width, entry.height);
            fStatus->SetText(heading);
        }
        Show();
    }

    bool QuitRequested() override
    {
        return fCanvas == NULL || !fCanvas->Dirty()
            || (new BAlert(BrowserUI().String("browser.alert.unsaved_title", "Unsaved icon"),
                BrowserUI().String("browser.alert.unsaved_message", "Discard the unsaved pixel changes?"),
                BrowserUI().String("browser.alert.cancel", "Cancel"), BrowserUI().String("browser.alert.discard", "Discard")))->Go() == 1;
    }

    void MessageReceived(BMessage* message) override
    {
        if (!_Ready()) {
            BWindow::MessageReceived(message);
            return;
        }
        switch (message->what) {
            case kPixelsChanged:
                fStatus->SetText(BrowserUI().String("browser.pixel.changed", "Pixel artwork changed — Save to write."));
                return;
            case kSavePixels: _Save(); return;
            case kRevertPixels: _Revert(); return;
            case kUndoPixels: _UndoSave(); return;
        }
        BWindow::MessageReceived(message);
    }

private:
    bool _Ready() const { return fCanvas != NULL && fStatus != NULL; }

    void _Revert()
    {
        std::vector<uint8> bytes;
        if (ReadPixels(fEntry, bytes)) {
            fCanvas->Reload(bytes);
            fStatus->SetText(BrowserUI().String("browser.pixel.reloaded", "Reloaded saved artwork."));
        } else
            fStatus->SetText(BrowserUI().String("browser.pixel.reload_failed", "Unable to reload original artwork."));
    }

    void _UndoSave()
    {
        if (!OwnerMayWrite(fEntry.path))
            return;
        std::string backup = fEntry.path + ".bak";
        if (!ExistingFile(backup)) {
            fStatus->SetText(BrowserUI().String("browser.pixel.no_backup", "No saved backup exists."));
            return;
        }
        std::string swap = fEntry.path + ".undo.XXXXXX";
        std::vector<char> temp(swap.begin(), swap.end());
        temp.push_back('\0');
        int fd = mkstemp(temp.data());
        if (fd < 0) {
            fStatus->SetText(BrowserUI().String("browser.pixel.undo_failed", "Could not prepare undo."));
            return;
        }
        close(fd);
        unlink(temp.data());
        if (rename(fEntry.path.c_str(), temp.data()) != 0
            || rename(backup.c_str(), fEntry.path.c_str()) != 0) {
            rename(temp.data(), fEntry.path.c_str());
            fStatus->SetText(BrowserUI().String("browser.pixel.restore_failed", "Unable to restore backup."));
            return;
        }
        rename(temp.data(), backup.c_str());
        _Revert();
        fStatus->SetText(BrowserUI().String("browser.pixel.restored", "Previous artwork restored; restart the target app."));
    }

    void _Save()
    {
        if (!fCanvas->Dirty()) {
            fStatus->SetText(BrowserUI().String("browser.pixel.no_changes", "No pixel changes to save."));
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
            fStatus->SetText(BrowserUI().String("browser.pixel.tmp_failed", "Could not create temporary image file."));
            return;
        }
        struct stat previousArtwork;
        bool hasArtwork = stat(fEntry.path.c_str(),
            &previousArtwork) == 0;
        size_t done = 0;
        bool ok = !hasArtwork
            || fchmod(fd, previousArtwork.st_mode & 07777) == 0;
        while (ok && done < pixels.size()) {
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
            fStatus->SetText(BrowserUI().String("browser.pixel.write_failed", "Could not write image; original preserved."));
            return;
        }
        // Backups are opt-in. Never require one before an Owner edit.
        // Rename the completed temporary file over the original atomically.
        std::string backup = fEntry.path + ".bak";
        bool keptBackup = BrowserUI().Number(
            "browser.pixel.keep_backup", 0, 0, 1) >= 0.5f
            && rename(fEntry.path.c_str(), backup.c_str()) == 0;
        if (rename(temp.data(), fEntry.path.c_str()) != 0) {
            if (keptBackup)
                rename(backup.c_str(), fEntry.path.c_str());
            unlink(temp.data());
            fStatus->SetText(BrowserUI().String("browser.pixel.install_failed", "Could not install image; original restored."));
            return;
        }
        fCanvas->MarkSaved();
        fStatus->SetText(BrowserUI().String("browser.pixel.saved", "Artwork saved. Restart the edited application."));
    }

    ResourceEntry fEntry;
    PixelCanvas* fCanvas;
    BStringView* fStatus;
};


class BinaryAssetEditorWindow : public BWindow {
public:
    explicit BinaryAssetEditorWindow(const ResourceEntry& entry)
        : BWindow(BrowserUI().Rect("browser.asset.window",
            BRect(180, 120, 850, 400)),
            (entry.label + BrowserUI().String("browser.asset.title_suffix",
                " — Asset Editor")).c_str(), B_TITLED_WINDOW, 0),
          fEntry(entry), fDetails(NULL), fStatus(NULL), fPicker(NULL)
    {
        BView* panel = new BView(Bounds(), "asset-editor",
            B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(BrowserUI().Color("browser.asset.background",
            rgb_color{239, 239, 236, 255}));
        AddChild(panel);
        const OSTenDialogAction actions[] = {
            {"asset.import", kAssetImport}
        };
        const OSTenDialogControls controls = OSTenCreateDialogControls(
            panel, BrowserUI(), "browser.asset",
            actions, sizeof(actions) / sizeof(actions[0]));
        fDetails = BrowserComposedControl<BStringView>(
            controls, "browser.asset", "details", "label");
        fStatus = BrowserComposedControl<BStringView>(
            controls, "browser.asset", "status", "label");
        if (fDetails != NULL) {
            struct stat st;
            const bool exists = stat(fEntry.path.c_str(), &st) == 0;
            char bytes[64];
            snprintf(bytes, sizeof(bytes), "%lld",
                exists ? (long long)st.st_size : 0LL);
            const std::string label = fEntry.type + "  |  "
                + Leaf(fEntry.path) + "  |  " + bytes + " bytes";
            fDetails->SetText(label.c_str());
        }
        Show();
    }

    ~BinaryAssetEditorWindow() override { delete fPicker; }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case kAssetImport:
                _Pick();
                return;
            case kAssetChosen: {
                entry_ref ref;
                if (message->FindRef("refs", &ref) == B_OK)
                    _Replace(ref);
                Activate(true);
                return;
            }
            case B_CANCEL:
                Activate(true);
                return;
        }
        BWindow::MessageReceived(message);
    }

private:
    void _Report(const char* key, const char* fallback)
    {
        if (fStatus != NULL)
            fStatus->SetText(BrowserUI().String(key, fallback));
    }

    void _Pick()
    {
        if (fPicker == NULL) {
            BMessenger receiver(this);
            BMessage selected(kAssetChosen);
            fPicker = new BFilePanel(B_OPEN_PANEL, &receiver,
                NULL, B_FILE_NODE, false, &selected);
        }
        fPicker->Show();
    }

    void _Replace(const entry_ref& ref)
    {
        BEntry selected(&ref, true);
        BPath path;
        if (selected.GetPath(&path) != B_OK)
            return;
        const std::string sourcePath(path.Path());
        if (sourcePath == fEntry.path) {
            _Report("browser.asset.unchanged",
                "Selected file is already the current asset.");
            return;
        }
        if (!OwnerMayWrite(fEntry.path))
            return;
        FILE* input = fopen(sourcePath.c_str(), "rb");
        if (input == NULL) {
            _Report("browser.asset.failed", "Unable to replace this asset.");
            return;
        }
        struct stat sourceMetadata;
        const bool sourceValid = fstat(fileno(input), &sourceMetadata) == 0
            && S_ISREG(sourceMetadata.st_mode)
            && sourceMetadata.st_size >= 0
            && sourceMetadata.st_size <= (off_t)BrowserUI().Number(
                "browser.asset.max_bytes", 134217728, 1, 1073741824);
        if (!sourceValid) {
            fclose(input);
            _Report("browser.asset.invalid",
                "Selected asset is unreadable or exceeds the size limit.");
            return;
        }
        std::string temporary = fEntry.path + ".tmp.XXXXXX";
        std::vector<char> name(temporary.begin(), temporary.end());
        name.push_back('\0');
        int fd = mkstemp(name.data());
        if (fd < 0) {
            fclose(input);
            _Report("browser.asset.failed", "Unable to replace this asset.");
            return;
        }
        struct stat current;
        const bool exists = stat(fEntry.path.c_str(), &current) == 0;
        bool ok = !exists || fchmod(fd, current.st_mode & 07777) == 0;
        uint8 buffer[32768];
        while (ok) {
            const size_t amount = fread(buffer, 1, sizeof(buffer), input);
            if (amount == 0) {
                if (ferror(input)) ok = false;
                break;
            }
            size_t sent = 0;
            while (ok && sent < amount) {
                ssize_t count = write(fd, buffer + sent, amount - sent);
                if (count <= 0) {
                    ok = false;
                    break;
                }
                sent += (size_t)count;
            }
        }
        if (fclose(input) != 0) ok = false;
        if (ok && fsync(fd) != 0) ok = false;
        if (close(fd) != 0) ok = false;
        if (ok && rename(name.data(), fEntry.path.c_str()) == 0) {
            _Report("browser.asset.saved",
                "Asset replaced. Restart the edited application to reload.");
            if (fDetails != NULL) {
                char bytes[64];
                snprintf(bytes, sizeof(bytes), "%lld",
                    (long long)sourceMetadata.st_size);
                const std::string label = fEntry.type + "  |  "
                    + Leaf(fEntry.path) + "  |  " + bytes + " bytes";
                fDetails->SetText(label.c_str());
            }
            return;
        }
        unlink(name.data());
        _Report("browser.asset.failed", "Unable to replace this asset.");
    }

    ResourceEntry fEntry;
    BStringView* fDetails;
    BStringView* fStatus;
    BFilePanel* fPicker;
};

// A real hierarchical menu-order editor. This edits the stable command-ID
// sequence, not executable commands. Labels live in the target's own catalog.
// Preserve all unrelated lines exactly. The completed temporary file is
// atomically renamed over the selected manifest; the Owner owns backups.
static bool
WriteResourceValues(const std::string& path,
    const std::map<std::string, std::string>& replacements,
    bool allowNewKeys = false)
{
    if (replacements.empty()) return false;
    FILE* input = fopen(path.c_str(), "rb");
    if (input == NULL) return false;
    struct stat previous;
    bool haveMode = fstat(fileno(input), &previous) == 0;
    std::string output;
    char row[8192];
    std::set<std::string> found;
    while (fgets(row, sizeof(row), input) != NULL) {
        std::string line(row);
        size_t equals = line.find('=');
        std::string key = equals == std::string::npos
            ? "" : line.substr(0, equals);
        std::map<std::string, std::string>::const_iterator changed
            = replacements.find(key);
        if (changed != replacements.end()) {
            output += key + "=" + changed->second + "\n";
            found.insert(key);
        } else {
            output += line;
        }
    }
    bool readOK = !ferror(input);
    fclose(input);
    if (!readOK || (!allowNewKeys && found.size() != replacements.size()))
        return false;
    if (allowNewKeys) {
        for (std::map<std::string, std::string>::const_iterator it
                = replacements.begin(); it != replacements.end(); ++it) {
            if (found.find(it->first) == found.end())
                output += it->first + "=" + it->second + "\n";
        }
    }
    std::string temporary = path + ".tmp.XXXXXX";
    std::vector<char> name(temporary.begin(), temporary.end());
    name.push_back('\0');
    int fd = mkstemp(name.data());
    if (fd < 0) return false;
    // An atomic replacement must preserve the original file's permissions.
    bool ok = !haveMode || fchmod(fd, previous.st_mode & 07777) == 0;
    size_t written = 0;
    while (ok && written < output.size()) {
        ssize_t count = write(fd, output.data() + written,
            output.size() - written);
        if (count <= 0) { ok = false; break; }
        written += (size_t)count;
    }
    if (ok && fsync(fd) != 0) ok = false;
    if (close(fd) != 0) ok = false;
    if (ok && rename(name.data(), path.c_str()) == 0)
        return true;
    unlink(name.data());
    return false;
}

// Preserve typed stable IDs when the Owner adds a control at runtime.
// Append only newly introduced keys; no existing index entries are rewritten.
// Failure is reported, rather than pretending the index and manifest agree.
static bool
IndexNewResourceKeys(const ResourceEntry& parent,
    const std::map<std::string, std::string>& additions)
{
    if (additions.empty()) return true;
    const std::string indexPath = Parent(parent.path) + "/ResourceIndex.osti";
    if (!ExistingFile(indexPath))
        return true; // Unindexed legacy applications remain supported.
    std::map<std::string, OSTenIndexedResource> items;
    if (!OSTenReadResourceIndex(indexPath, items))
        return false;
    FILE* input = fopen(indexPath.c_str(), "rb");
    if (input == NULL) return false;
    struct stat metadata;
    const bool haveMode = fstat(fileno(input), &metadata) == 0;
    std::string body;
    char line[8192];
    while (fgets(line, sizeof(line), input) != NULL)
        body += line;
    bool ok = !ferror(input);
    fclose(input);
    if (!ok) return false;
    std::set<uint32_t> identities;
    for (std::map<std::string, OSTenIndexedResource>::const_iterator it
            = items.begin(); it != items.end(); ++it)
        identities.insert(it->second.id);
    for (std::map<std::string, std::string>::const_iterator it
            = additions.begin(); it != additions.end(); ++it) {
        if (items.find(it->first) != items.end())
            continue;
        const uint32 id = OSTenStableResourceID(parent.application, it->first);
        if (!identities.insert(id).second)
            return false;
        char number[20];
        snprintf(number, sizeof(number), "0x%08lx", (unsigned long)id);
        // Index each new field by its actual role, not by the type of
        // its parent dialog. Newly authored control frames are layouts;
        // captions and action bindings are normal editable text.
        std::string type = "TEXT";
        if (EndsWith(it->first, ".frame") || EndsWith(it->first, ".frame_key")
            || EndsWith(it->first, ".window_frame_key"))
            type = "WLAY";
        else if (EndsWith(it->first, ".controls"))
            type = "DLOG";
        else if (EndsWith(it->first, ".background"))
            type = "COLR";
        else if (EndsWith(it->first, ".order"))
            type = "MENU";
        body += std::string(number) + "|" + type + "|"
            + it->first + "|" + it->first + "|0|0\n";
    }
    std::string temporary = indexPath + ".tmp.XXXXXX";
    std::vector<char> name(temporary.begin(), temporary.end());
    name.push_back('\0');
    int fd = mkstemp(name.data());
    if (fd < 0) return false;
    ok = !haveMode || fchmod(fd, metadata.st_mode & 07777) == 0;
    size_t written = 0;
    while (ok && written < body.size()) {
        ssize_t amount = write(fd, body.data() + written,
            body.size() - written);
        if (amount <= 0) { ok = false; break; }
        written += (size_t)amount;
    }
    if (ok && fsync(fd) != 0) ok = false;
    if (close(fd) != 0) ok = false;
    if (ok && rename(name.data(), indexPath.c_str()) == 0)
        return true;
    unlink(name.data());
    return false;
}

static bool
WriteResourceValue(const ResourceEntry& item, const std::string& value)
{
    std::map<std::string, std::string> changes;
    changes[item.key] = value;
    return WriteResourceValues(item.path, changes);
}

class MenuEditorWindow : public BWindow {
public:
    explicit MenuEditorWindow(const ResourceEntry& entry)
        : BWindow(BrowserUI().Rect("browser.menu.window",
            BRect(180, 110, 780, 620)),
            (entry.label + BrowserUI().String("browser.menu.title_suffix",
                " — Menu Editor")).c_str(), B_TITLED_WINDOW, 0),
          fEntry(entry), fItems(NULL), fAction(NULL), fStatus(NULL), fDirty(false)
    {
        BView* pane = new BView(Bounds(), "menu-editor",
            B_FOLLOW_ALL, B_WILL_DRAW);
        pane->SetViewColor(BrowserUI().Color("browser.menu.background",
            rgb_color{239, 239, 236, 255}));
        AddChild(pane);
        const OSTenDialogAction actions[] = {
            {"menu.selected", kMenuSelected},
            {"menu.add", kMenuAdd},
            {"menu.separator", kMenuSeparator},
            {"menu.replace", kMenuReplace},
            {"menu.remove", kMenuRemove},
            {"menu.up", kMenuUp},
            {"menu.down", kMenuDown},
            {"menu.revert", kMenuRevert},
            {"menu.save", kMenuSave}
        };
        OSTenDialogControls controls = OSTenCreateDialogControls(
            pane, BrowserUI(), "browser.menu", actions,
            sizeof(actions) / sizeof(actions[0]));
        fItems = BrowserComposedControl<BListView>(
            controls, "browser.menu", "items", "list");
        fAction = BrowserComposedControl<BTextControl>(
            controls, "browser.menu", "action", "text");
        fStatus = BrowserComposedControl<BStringView>(
            controls, "browser.menu", "status", "label");
        if (fItems != NULL)
            fItems->SetSelectionMessage(new BMessage(kMenuSelected));
        if (_Ready())
            _Load();
        Show();
    }

    void MessageReceived(BMessage* message) override
    {
        if (!_Ready()) {
            BWindow::MessageReceived(message);
            return;
        }
        const int32 selection = fItems->CurrentSelection();
        const bool valid = selection >= 0 && (size_t)selection < fCommands.size();
        switch (message->what) {
            case kMenuSelected:
                if (valid)
                    fAction->SetText(fCommands[selection].c_str());
                return;
            case kMenuAdd: {
                std::string value(fAction->Text());
                if (!_ValidateID(value)) return;
                fCommands.insert(fCommands.begin() + (valid ? selection + 1
                    : (int32)fCommands.size()), value);
                _Populate(valid ? selection + 1 : (int32)fCommands.size() - 1);
                return;
            }
            case kMenuSeparator:
                fCommands.insert(fCommands.begin() + (valid ? selection + 1
                    : (int32)fCommands.size()), "separator");
                _Populate(valid ? selection + 1 : (int32)fCommands.size() - 1);
                return;
            case kMenuReplace:
                if (valid) {
                    std::string value(fAction->Text());
                    if (!_ValidateID(value)) return;
                    fCommands[selection] = value;
                    _Populate(selection);
                }
                return;
            case kMenuRemove:
                if (valid) {
                    fCommands.erase(fCommands.begin() + selection);
                    _Populate((int32)std::min((size_t)selection,
                        fCommands.size() ? fCommands.size() - 1 : (size_t)0));
                }
                return;
            case kMenuUp:
            case kMenuDown:
                if (valid) {
                    int32 next = selection + (message->what == kMenuUp ? -1 : 1);
                    if (next >= 0 && (size_t)next < fCommands.size()) {
                        std::swap(fCommands[selection], fCommands[next]);
                        _Populate(next);
                    }
                }
                return;
            case kMenuRevert: _Load(); return;
            case kMenuSave: _Save(); return;
        }
        BWindow::MessageReceived(message);
    }

private:
    bool _Ready() const
    {
        return fItems != NULL && fAction != NULL && fStatus != NULL;
    }

    static bool _ValidateID(const std::string& value)
    {
        return !value.empty() && value.size() <= 128
            && value.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                "0123456789._-") == std::string::npos;
    }

    static std::string _Join(const std::vector<std::string>& tokens)
    {
        std::string output;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i != 0) output += ',';
            output += tokens[i];
        }
        return output;
    }

    void _Populate(int32 selected = -1)
    {
        while (fItems->CountItems() != 0)
            delete fItems->RemoveItem((int32)0);
        for (size_t i = 0; i < fCommands.size(); ++i) {
            std::string label = fCommands[i] == "separator"
                ? BrowserUI().String("browser.menu.separator_name",
                    "-------- separator --------") : fCommands[i];
            fItems->AddItem(new BStringItem(label.c_str()));
        }
        if (selected >= 0 && (size_t)selected < fCommands.size())
            fItems->Select(selected);
        fDirty = true;
    }

    void _Load()
    {
        FILE* input = fopen(fEntry.path.c_str(), "rb");
        if (!input) {
            fStatus->SetText(BrowserUI().String("browser.menu.read_failure",
                "Cannot open resource document"));
            return;
        }
        char line[8192];
        std::string value;
        bool found = false;
        while (fgets(line, sizeof(line), input) != NULL) {
            std::string entry(line);
            if (entry.compare(0, fEntry.key.size(), fEntry.key) != 0
                || entry.size() <= fEntry.key.size()
                || entry[fEntry.key.size()] != '=')
                continue;
            value = entry.substr(fEntry.key.size() + 1);
            while (!value.empty() && (value.back() == '\n'
                || value.back() == '\r'))
                value.pop_back();
            found = true;
            break;
        }
        fclose(input);
        if (!found) {
            fStatus->SetText(BrowserUI().String("browser.menu.not_found",
                "Resource no longer exists"));
            return;
        }
        fCommands.clear();
        size_t start = 0;
        for (size_t i = 0; i <= value.size(); ++i) {
            if (i == value.size() || value[i] == ',') {
                if (i != start)
                    fCommands.push_back(value.substr(start, i - start));
                start = i + 1;
            }
        }
        _Populate();
        fDirty = false;
        fStatus->SetText(BrowserUI().String("browser.menu.ready",
            "Command IDs retain their actions"));
    }

    void _Save()
    {
        if (!fDirty) return;
        if (!OwnerMayWrite(fEntry.path)) return;
        if (!WriteResourceValue(fEntry, _Join(fCommands))) {
            fStatus->SetText(BrowserUI().String("browser.menu.save_failure",
                "Save failed. Resource was not replaced"));
            return;
        }
        fDirty = false;
        fStatus->SetText(BrowserUI().String("browser.menu.saved",
            "Saved. Restart the application to reload"));
    }

    ResourceEntry fEntry;
    BListView* fItems;
    BTextControl* fAction;
    BStringView* fStatus;
    std::vector<std::string> fCommands;
    bool fDirty;
};

static bool
IsMenuSequence(const ResourceEntry& entry)
{
    // Control Strip module ordering is another stable-ID sequence, not a
    // compiled list of slots. Use the same visual sequence editor.
    if (entry.key == "strip.module.order") return true;
    if (entry.type != "MENU") return false;
    return entry.key.compare(0, 10, "menu.title") != 0
        && entry.key.compare(0, 9, "menu.item") != 0;
}

// RGB editor with live preview. The app's own geometry, titles and labels
// are catalog resources just like the color value being edited.
class ColorSample : public BView {
public:
    ColorSample(BRect rect, const char* identifier) : BView(rect, identifier,
        B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW)
    {
        fColor = rgb_color{255, 255, 255, 255};
        SetViewColor(B_TRANSPARENT_COLOR);
    }
    void SetColor(rgb_color value) { fColor = value; Invalidate(); }
    void Draw(BRect) override
    {
        SetHighColor(fColor);
        FillRect(Bounds());
        SetHighColor(40, 40, 40);
        StrokeRect(Bounds());
    }
private:
    rgb_color fColor;
};

static BView*
CreateBrowserColorSample(BRect frame, const char* identifier, const char*,
    uint32, const OSTenResourceCatalog&, const std::string&, void*)
{
    return new ColorSample(frame, identifier);
}

class ColorEditorWindow : public BWindow {
public:
    explicit ColorEditorWindow(const ResourceEntry& item)
        : BWindow(BrowserUI().Rect("browser.color.window",
            BRect(190, 145, 700, 510)),
            (item.label + BrowserUI().String("browser.color.title_suffix",
                " — Color Editor")).c_str(), B_TITLED_WINDOW, 0),
          fEntry(item), fSample(NULL), fStatus(NULL)
    {
        for (int i = 0; i < 3; ++i)
            fSlider[i] = NULL;
        BView* panel = new BView(Bounds(), "color-editor",
            B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(BrowserUI().Color("browser.color.background",
            rgb_color{239, 239, 236, 255}));
        AddChild(panel);
        const OSTenDialogAction actions[] = {
            {"color.changed", kColorChange},
            {"color.revert", kColorRevert},
            {"color.save", kColorSave}
        };
        OSTenDialogControlFactories factories;
        factories["color_sample"] = &CreateBrowserColorSample;
        OSTenDialogControls controls = OSTenCreateDialogControls(
            panel, BrowserUI(), "browser.color", actions,
            sizeof(actions) / sizeof(actions[0]), &factories);
        const char* names[] = {"red", "green", "blue"};
        for (int i = 0; i < 3; ++i) {
            fSlider[i] = BrowserComposedControl<BSlider>(
                controls, "browser.color", names[i], "slider");
            if (fSlider[i] != NULL)
                fSlider[i]->SetTarget(this);
        }
        fSample = BrowserComposedControl<ColorSample>(
            controls, "browser.color", "preview", "color_sample");
        fStatus = BrowserComposedControl<BStringView>(
            controls, "browser.color", "status", "label");
        if (_Ready())
            _Load();
        Show();
    }

    void MessageReceived(BMessage* message) override
    {
        if (!_Ready()) {
            BWindow::MessageReceived(message);
            return;
        }
        switch (message->what) {
            case kColorChange: _Preview(); return;
            case kColorRevert: _Load(); return;
            case kColorSave: _Save(); return;
        }
        BWindow::MessageReceived(message);
    }

private:
    bool _Ready() const
    {
        return fSlider[0] != NULL && fSlider[1] != NULL
            && fSlider[2] != NULL && fSample != NULL && fStatus != NULL;
    }

    void _Preview()
    {
        fSample->SetColor(rgb_color{(uint8)fSlider[0]->Value(),
            (uint8)fSlider[1]->Value(), (uint8)fSlider[2]->Value(), 255});
    }

    void _Load()
    {
        FILE* file = fopen(fEntry.path.c_str(), "rb");
        if (file == NULL) return;
        char row[8192];
        int red = 255, green = 255, blue = 255;
        while (fgets(row, sizeof(row), file) != NULL) {
            std::string line(row);
            if (line.compare(0, fEntry.key.size(), fEntry.key) != 0
                || line.size() <= fEntry.key.size()
                || line[fEntry.key.size()] != '=')
                continue;
            char remaining;
            if (sscanf(row + fEntry.key.size() + 1, "%d,%d,%d %c",
                    &red, &green, &blue, &remaining) != 3
                || red < 0 || green < 0 || blue < 0
                || red > 255 || green > 255 || blue > 255) {
                fStatus->SetText(BrowserUI().String("browser.color.invalid",
                    "This resource is not a valid RGB triple"));
                fclose(file);
                return;
            }
            for (int i = 0; i < 3; ++i)
                fSlider[i]->SetValue(i == 0 ? red : i == 1 ? green : blue);
            _Preview();
            fStatus->SetText(BrowserUI().String("browser.color.ready",
                "The preview reflects your selected values"));
            fclose(file);
            return;
        }
        fclose(file);
    }

    void _Save()
    {
        if (!OwnerMayWrite(fEntry.path)) return;
        char text[64];
        snprintf(text, sizeof(text), "%ld,%ld,%ld",
            (long)fSlider[0]->Value(), (long)fSlider[1]->Value(),
            (long)fSlider[2]->Value());
        bool ok = WriteResourceValue(fEntry, text);
        fStatus->SetText(BrowserUI().String(ok ? "browser.color.saved"
            : "browser.color.failed", ok
                ? "Saved. Restart the application to reload"
                : "Could not save the selected RGB resource"));
    }

    ResourceEntry fEntry;
    BSlider* fSlider[3];
    ColorSample* fSample;
    BStringView* fStatus;
};

// Structured dialog/window layout designer. The preview controls are
// dragged or resized as resource data; executable handler code is untouched.
struct LayoutHandle {
    std::string id;
    std::string frameKey;
    std::string captionKey;
    std::string caption;
    std::string type;
    std::string action;
    BRect frame;
};

class LayoutCanvas : public BView {
public:
    LayoutCanvas(BRect bounds, std::vector<LayoutHandle>* handles,
        BRect* windowFrame, const char* identifier)
        : BView(bounds, identifier, B_FOLLOW_ALL, B_WILL_DRAW),
          fHandles(handles), fWindowFrame(windowFrame),
          fSelected(-1), fTracking(false), fResize(false),
          fWindowTracking(false), fWindowResize(false)
    {
        SetViewColor(BrowserUI().Color("browser.layout.paper",
            rgb_color{252, 252, 251, 255}));
    }

    int32 Selected() const { return fSelected; }
    void Select(int32 index)
    {
        fSelected = index;
        Invalidate();
    }

    void Draw(BRect) override
    {
        const OSTenResourceCatalog& ui = BrowserUI();
        SetHighColor(ui.Color("browser.layout.frame_color",
            rgb_color{80, 90, 103, 255}));
        StrokeRect(Bounds());
        if (fWindowFrame != NULL && fWindowFrame->IsValid()) {
            const BRect window = _WindowRect();
            SetHighColor(ui.Color("browser.layout.window_outline",
                rgb_color{42, 68, 99, 255}));
            StrokeRect(window);
            const float titleHeight = ui.Number(
                "browser.layout.window_title_height", 20, 0, 256);
            StrokeLine(BPoint(window.left, window.top + titleHeight),
                BPoint(window.right, window.top + titleHeight));
            SetHighColor(ui.Color("browser.layout.window_handle_fill",
                rgb_color{42, 68, 99, 255}));
            FillRect(_WindowResizeHandle(window));
        }
        for (size_t i = 0; i < fHandles->size(); ++i) {
            const BRect rect = _ControlRect(i);
            SetHighColor(i == (size_t)fSelected
                ? ui.Color("browser.layout.selected",
                    rgb_color{191, 220, 250, 255})
                : ui.Color("browser.layout.control",
                    rgb_color{234, 234, 232, 255}));
            FillRect(rect);
            SetHighColor(ui.Color("browser.layout.frame_color",
                rgb_color{80, 90, 103, 255}));
            StrokeRect(rect);
            const BPoint textOffset = ui.Point(
                "browser.layout.control_label_offset", BPoint(6, 18));
            MovePenTo(rect.left + textOffset.x,
                rect.top + textOffset.y);
            SetHighColor(ui.Color("browser.layout.control_label_color",
                rgb_color{80, 90, 103, 255}));
            const LayoutHandle& item = (*fHandles)[i];
            const std::string label = item.caption.empty() ? item.id
                : item.caption;
            DrawString(label.c_str());
            if (i == (size_t)fSelected) {
                SetHighColor(ui.Color("browser.layout.control_handle_fill",
                    rgb_color{80, 90, 103, 255}));
                FillRect(_ControlResizeHandle(rect));
            }
        }
    }

    void MouseDown(BPoint where) override
    {
        fSelected = -1;
        fTracking = false;
        fWindowTracking = false;
        const OSTenResourceCatalog& ui = BrowserUI();
        for (int32 i = (int32)fHandles->size() - 1; i >= 0; --i) {
            const BRect rect = _ControlRect((size_t)i);
            if (rect.Contains(where)) {
                fSelected = i;
                fOrigin = where;
                fInitial = (*fHandles)[i].frame;
                BRect hit = _ControlResizeHandle(rect);
                const float padding = ui.Number(
                    "browser.layout.control_handle_hit_padding",
                    2, 0, 128);
                hit.left -= padding;
                hit.top -= padding;
                fResize = hit.Contains(where);
                fTracking = true;
                SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
                break;
            }
        }
        if (!fTracking && fWindowFrame != NULL && fWindowFrame->IsValid()) {
            const BRect window = _WindowRect();
            BRect hit = _WindowResizeHandle(window);
            const float inner = ui.Number(
                "browser.layout.window_handle_hit_inner", 2, 0, 128);
            const float outer = ui.Number(
                "browser.layout.window_handle_hit_outer", 5, 0, 128);
            hit.left -= inner;
            hit.top -= inner;
            hit.right += outer;
            hit.bottom += outer;
            if (hit.Contains(where)) {
                fWindowTracking = true;
                fWindowResize = true;
            } else {
                const float titleHeight = ui.Number(
                    "browser.layout.window_title_height", 20, 0, 256);
                if (BRect(window.left, window.top, window.right,
                        window.top + titleHeight).Contains(where)) {
                    fWindowTracking = true;
                    fWindowResize = false;
                }
            }
            if (fWindowTracking) {
                fOrigin = where;
                fInitial = *fWindowFrame;
                SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
            }
        }
        Invalidate();
        Window()->PostMessage(kLayoutSelected);
    }

    void MouseMoved(BPoint where, uint32, const BMessage*) override
    {
        if (fWindowTracking && fWindowFrame != NULL) {
            BRect frame = fInitial;
            float dx = where.x - fOrigin.x;
            float dy = where.y - fOrigin.y;
            if (fWindowResize) {
                frame.right = std::max(frame.left + BrowserUI().Number(
                    "browser.layout.min_window_width", 40, 1, 8192),
                    frame.right + dx);
                frame.bottom = std::max(frame.top + BrowserUI().Number(
                    "browser.layout.min_window_height", 35, 1, 8192),
                    frame.bottom + dy);
            } else frame.OffsetBy(dx, dy);
            *fWindowFrame = frame;
            Invalidate();
            Window()->PostMessage(kLayoutSelected);
            return;
        }
        if (!fTracking || fSelected < 0) return;
        BRect frame = fInitial;
        const float dx = where.x - fOrigin.x;
        const float dy = where.y - fOrigin.y;
        if (fResize) {
            frame.right = std::max(frame.left + BrowserUI().Number(
                "browser.layout.min_control_width", 20, 1, 8192),
                frame.right + dx);
            frame.bottom = std::max(frame.top + BrowserUI().Number(
                "browser.layout.min_control_height", 15, 1, 8192),
                frame.bottom + dy);
        } else frame.OffsetBy(dx, dy);
        (*fHandles)[fSelected].frame = frame;
        Invalidate();
    }

    void MouseUp(BPoint) override
    {
        fTracking = false;
        fWindowTracking = false;
    }
private:
    BPoint _PreviewOrigin() const
    {
        return BrowserUI().Point("browser.layout.preview_origin",
            BPoint(20, 20));
    }

    BRect _WindowRect() const
    {
        const BPoint origin = _PreviewOrigin();
        return BRect(origin.x, origin.y,
            origin.x + fWindowFrame->Width(),
            origin.y + fWindowFrame->Height());
    }

    BRect _ControlRect(size_t index) const
    {
        BRect frame = (*fHandles)[index].frame;
        const BPoint origin = _PreviewOrigin();
        frame.OffsetBy(origin.x, origin.y);
        return frame;
    }

    BRect _WindowResizeHandle(const BRect& frame) const
    {
        const float size = BrowserUI().Number(
            "browser.layout.window_handle_size", 11, 1, 256);
        return BRect(frame.right - size, frame.bottom - size,
            frame.right, frame.bottom);
    }

    BRect _ControlResizeHandle(const BRect& frame) const
    {
        const float size = BrowserUI().Number(
            "browser.layout.control_handle_size", 10, 1, 256);
        return BRect(frame.right - size, frame.bottom - size,
            frame.right, frame.bottom);
    }

    std::vector<LayoutHandle>* fHandles;
    BRect* fWindowFrame;
    int32 fSelected;
    bool fTracking;
    bool fResize;
    bool fWindowTracking;
    bool fWindowResize;
    BPoint fOrigin;
    BRect fInitial;
};


struct LayoutCanvasFactoryContext {
    std::vector<LayoutHandle>* handles;
    BRect* frame;
};

static BView*
CreateBrowserLayoutCanvas(BRect frame, const char* identifier, const char*,
    uint32, const OSTenResourceCatalog&, const std::string&, void* context)
{
    LayoutCanvasFactoryContext* data
        = static_cast<LayoutCanvasFactoryContext*>(context);
    if (data == NULL || data->handles == NULL || data->frame == NULL)
        return NULL;
    return new LayoutCanvas(frame, data->handles, data->frame, identifier);
}

class LayoutEditorWindow : public BWindow {
public:
    explicit LayoutEditorWindow(const ResourceEntry& item)
        : BWindow(BrowserUI().Rect("browser.layout.window",
            BRect(120, 70, 1265, 690)),
            (item.label + BrowserUI().String("browser.layout.title_suffix",
                " — Dialog Layout Editor")).c_str(), B_TITLED_WINDOW, 0),
          fEntry(item), fCanvas(NULL), fStatus(NULL), fID(NULL),
          fType(NULL), fCaption(NULL), fAction(NULL), fWindowGeometry(NULL)
    {
        fWindowFrame = BRect(0, 0, -1, -1);
        BView* panel = new BView(Bounds(), "layout-editor",
            B_FOLLOW_ALL, B_WILL_DRAW);
        panel->SetViewColor(BrowserUI().Color("browser.layout.background",
            rgb_color{239, 239, 236, 255}));
        AddChild(panel);

        const OSTenDialogAction actions[] = {
            {"layout.apply", kLayoutApply},
            {"layout.add", kLayoutAdd},
            {"layout.remove", kLayoutRemove},
            {"layout.up", kLayoutUp},
            {"layout.down", kLayoutDown},
            {"layout.revert", kLayoutRevert},
            {"layout.save", kLayoutSave}
        };
        LayoutCanvasFactoryContext context = {&fHandles, &fWindowFrame};
        OSTenDialogControlFactories factories;
        factories["layout_canvas"] = &CreateBrowserLayoutCanvas;
        const OSTenDialogControls controls = OSTenCreateDialogControls(
            panel, BrowserUI(), "browser.layout",
            actions, sizeof(actions) / sizeof(actions[0]),
            &factories, &context);
        fCanvas = BrowserComposedControl<LayoutCanvas>(
            controls, "browser.layout", "canvas", "layout_canvas");
        fID = BrowserComposedControl<BTextControl>(
            controls, "browser.layout", "id", "text");
        fType = BrowserComposedControl<BTextControl>(
            controls, "browser.layout", "type", "text");
        fCaption = BrowserComposedControl<BTextControl>(
            controls, "browser.layout", "caption", "text");
        fAction = BrowserComposedControl<BTextControl>(
            controls, "browser.layout", "action", "text");
        fWindowGeometry = BrowserComposedControl<BTextControl>(
            controls, "browser.layout", "window_geometry", "text");
        fStatus = BrowserComposedControl<BStringView>(
            controls, "browser.layout", "status", "label");
        if (_Ready())
            _Load();
        Show();
    }

    void MessageReceived(BMessage* message) override
    {
        if (!_Ready()) {
            BWindow::MessageReceived(message);
            return;
        }
        switch (message->what) {
            case kLayoutSelected: {
                int32 selected = fCanvas->Selected();
                _Select(selected);
                return;
            }
            case kLayoutRevert: _Load(); return;
            case kLayoutApply: _Apply(); return;
            case kLayoutAdd: _Add(); return;
            case kLayoutRemove: _Remove(); return;
            case kLayoutUp: _Move(-1); return;
            case kLayoutDown: _Move(1); return;
            case kLayoutSave: _Save(); return;
        }
        BWindow::MessageReceived(message);
    }

private:
    bool _Ready() const
    {
        return fCanvas != NULL && fStatus != NULL
            && fID != NULL && fType != NULL && fCaption != NULL
            && fAction != NULL && fWindowGeometry != NULL;
    }

    void _Load()
    {
        FILE* input = fopen(fEntry.path.c_str(), "rb");
        if (input == NULL) return;
        std::map<std::string, std::string> fields;
        char row[8192];
        while (fgets(row, sizeof(row), input) != NULL) {
            std::string line(row);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string value = line.substr(eq + 1);
            while (!value.empty() && (value.back() == '\n'
                || value.back() == '\r'))
                value.pop_back();
            fields[line.substr(0, eq)] = value;
        }
        fclose(input);
        fHandles.clear();
        fValues = fields;
        if (!EndsWith(fEntry.key, ".controls")) return;
        const std::string prefix = fEntry.key.substr(0,
            fEntry.key.size() - strlen(".controls"));
        fWindowKey.clear();
        const std::string alias = prefix + ".window_frame_key";
        if (fields.find(alias) != fields.end())
            fWindowKey = fields[alias];
        else if (fields.find(prefix + ".frame") != fields.end())
            fWindowKey = prefix + ".frame";
        else if (fields.find(prefix + ".window") != fields.end())
            fWindowKey = prefix + ".window";
        fWindowFrame = BRect(0, 0, -1, -1);
        if (!fWindowKey.empty()) {
            std::map<std::string, std::string>::const_iterator found
                = fields.find(fWindowKey);
            if (found != fields.end()) {
                float l, t, r, b;
                char extra = 0;
                if (sscanf(found->second.c_str(), "%f,%f,%f,%f%c",
                        &l, &t, &r, &b, &extra) == 4
                    && isfinite(l) && isfinite(t)
                    && isfinite(r) && isfinite(b) && r > l && b > t)
                    fWindowFrame = BRect(l, t, r, b);
            }
        }
        std::map<std::string, std::string>::const_iterator source
            = fields.find(fEntry.key);
        if (source == fields.end()) return;
        const std::string& order = source->second;
        size_t previous = 0;
        for (size_t i = 0; i <= order.size(); ++i) {
            if (i != order.size() && order[i] != ',') continue;
            std::string id = order.substr(previous, i - previous);
            previous = i + 1;
            if (id.empty()) continue;
            const std::string base = prefix + ".control." + id + ".";
            const std::string aliasKey = base + "frame_key";
            const std::string frameKey = fields.find(aliasKey) == fields.end()
                ? base + "frame" : fields[aliasKey];
            std::map<std::string, std::string>::const_iterator frame
                = fields.find(frameKey);
            if (frame == fields.end()) continue;
            float l, t, r, b;
            char tail;
            if (sscanf(frame->second.c_str(), "%f,%f,%f,%f%c",
                    &l, &t, &r, &b, &tail) != 4) continue;
            LayoutHandle handle;
            handle.id = id;
            handle.frameKey = frameKey;
            handle.frame = BRect(l, t, r, b);
            const std::string captionKey = base + "caption_key";
            const std::string key = fields.find(captionKey) == fields.end()
                ? base + "caption" : fields[captionKey];
            handle.captionKey = key.empty() ? base + "caption" : key;
            std::map<std::string, std::string>::const_iterator caption
                = fields.find(handle.captionKey);
            handle.caption = caption != fields.end() ? caption->second : id;
            handle.type = fields[base + "type"];
            handle.action = fields[base + "action"];
            fHandles.push_back(handle);
        }
        fCanvas->Select(-1);
        _Select(-1);
        fCanvas->Invalidate();
        fStatus->SetText(BrowserUI().String("browser.layout.ready",
            "Control frames loaded from the application's resources"));
    }


    static bool _ValidID(const std::string& value)
    {
        return !value.empty() && value.size() <= 128
            && value.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                "0123456789._-") == std::string::npos;
    }

    void _Select(int32 index)
    {
        if (!fWindowKey.empty() && fWindowFrame.IsValid()) {
            char coords[144];
            snprintf(coords, sizeof(coords), "%.0f,%.0f,%.0f,%.0f",
                fWindowFrame.left, fWindowFrame.top,
                fWindowFrame.right, fWindowFrame.bottom);
            fWindowGeometry->SetText(coords);
        } else fWindowGeometry->SetText("");
        if (index < 0 || (size_t)index >= fHandles.size()) {
            fID->SetText("");
            fType->SetText("");
            fCaption->SetText("");
            fAction->SetText("");
            return;
        }
        const LayoutHandle& handle = fHandles[index];
        fID->SetText(handle.id.c_str());
        fType->SetText(handle.type.c_str());
        fCaption->SetText(handle.caption.c_str());
        fAction->SetText(handle.action.c_str());
        fStatus->SetText(handle.id.c_str());
    }

    void _Apply()
    {
        if (!fWindowKey.empty() && fWindowGeometry->Text()[0] != 0) {
            float l, t, r, b;
            char extra = 0;
            if (sscanf(fWindowGeometry->Text(), "%f,%f,%f,%f%c",
                    &l, &t, &r, &b, &extra) == 4
                && isfinite(l) && isfinite(t) && isfinite(r) && isfinite(b)
                && r > l && b > t)
                fWindowFrame = BRect(l, t, r, b);
        }
        int32 index = fCanvas->Selected();
        if (index < 0 || (size_t)index >= fHandles.size()) {
            fCanvas->Invalidate();
            return;
        }
        // IDs are stable resource identities: change the caption, not the ID.
        LayoutHandle& item = fHandles[index];
        const std::string type = fType->Text();
        const std::string action = fAction->Text();
        if (!_ValidID(type) || (!action.empty() && !_ValidID(action))) {
            fStatus->SetText(BrowserUI().String("browser.layout.invalid",
                "Type and action must be resource identifiers"));
            return;
        }
        item.type = type;
        item.action = action;
        item.caption = fCaption->Text();
        fCanvas->Invalidate();
        fStatus->SetText(BrowserUI().String("browser.layout.applied",
            "Properties applied. Save to write the manifest"));
    }

    void _Add()
    {
        const std::string id = fID->Text();
        if (!_ValidID(id)) {
            fStatus->SetText(BrowserUI().String("browser.layout.invalid",
                "Type and action must be resource identifiers"));
            return;
        }
        for (size_t i = 0; i < fHandles.size(); ++i) {
            if (fHandles[i].id == id) {
                fStatus->SetText(BrowserUI().String("browser.layout.duplicate",
                    "A control with this ID already exists"));
                return;
            }
        }
        const std::string type = fType->Text();
        const std::string action = fAction->Text();
        if (!_ValidID(type) || (!action.empty() && !_ValidID(action))) {
            fStatus->SetText(BrowserUI().String("browser.layout.invalid",
                "Type and action must be resource identifiers"));
            return;
        }
        const std::string prefix = fEntry.key.substr(0,
            fEntry.key.size() - strlen(".controls"));
        LayoutHandle item;
        item.id = id;
        item.frameKey = prefix + ".control." + id + ".frame";
        item.captionKey = prefix + ".control." + id + ".caption";
        item.type = type;
        item.action = action;
        item.caption = fCaption->Text();
        const int32 current = fCanvas->Selected();
        item.frame = BRect(25, 25, 175, 61);
        if (current >= 0 && (size_t)current < fHandles.size())
            item.frame.OffsetBy(20, 20);
        const int32 next = current < 0 ? (int32)fHandles.size() : current + 1;
        fHandles.insert(fHandles.begin() + next, item);
        fCanvas->Select(next);
        fCanvas->Invalidate();
        _Select(next);
    }

    void _Remove()
    {
        int32 index = fCanvas->Selected();
        if (index < 0 || (size_t)index >= fHandles.size()) return;
        // The saved control order determines visibility. Definitions remain
        // in the catalog for future reuse rather than silently being deleted.
        fHandles.erase(fHandles.begin() + index);
        int32 next = index < (int32)fHandles.size() ? index
            : (int32)fHandles.size() - 1;
        fCanvas->Select(next);
        fCanvas->Invalidate();
        _Select(next);
    }

    void _Move(int32 delta)
    {
        int32 index = fCanvas->Selected();
        int32 next = index + delta;
        if (index < 0 || next < 0
            || (size_t)index >= fHandles.size()
            || (size_t)next >= fHandles.size()) return;
        std::swap(fHandles[index], fHandles[next]);
        fCanvas->Select(next);
        fCanvas->Invalidate();
        _Select(next);
    }

    void _Save()
    {
        if (!OwnerMayWrite(fEntry.path)) return;
        std::map<std::string, std::string> edits;
        for (size_t i = 0; i < fHandles.size(); ++i) {
            const LayoutHandle& handle = fHandles[i];
            char coords[160];
            snprintf(coords, sizeof(coords), "%.0f,%.0f,%.0f,%.0f",
                handle.frame.left, handle.frame.top,
                handle.frame.right, handle.frame.bottom);
            edits[handle.frameKey] = coords;
            const std::string base = fEntry.key.substr(0,
                fEntry.key.size() - strlen(".controls"))
                + ".control." + handle.id + ".";
            edits[base + "type"] = handle.type;
            edits[base + "caption_key"] = handle.captionKey;
            edits[handle.captionKey] = handle.caption;
            edits[base + "action"] = handle.action;
        }
        std::string order;
        for (size_t i = 0; i < fHandles.size(); ++i) {
            if (i != 0) order += ",";
            order += fHandles[i].id;
        }
        edits[fEntry.key] = order;
        if (!fWindowKey.empty() && fWindowFrame.IsValid()) {
            char window[144];
            snprintf(window, sizeof(window), "%.0f,%.0f,%.0f,%.0f",
                fWindowFrame.left, fWindowFrame.top,
                fWindowFrame.right, fWindowFrame.bottom);
            edits[fWindowKey] = window;
        }
        // Track newly authored resource fields so they receive stable IDs
        // in the same public index used by the browser and native clients.
        std::map<std::string, std::string> additions;
        for (std::map<std::string, std::string>::const_iterator it
                = edits.begin(); it != edits.end(); ++it) {
            if (fValues.find(it->first) == fValues.end())
                additions[it->first] = it->second;
        }
        bool saved = WriteResourceValues(fEntry.path, edits, true);
        bool indexed = saved && IndexNewResourceKeys(fEntry, additions);
        if (saved) {
            for (std::map<std::string, std::string>::const_iterator it
                    = edits.begin(); it != edits.end(); ++it)
                fValues[it->first] = it->second;
        }
        if (saved && !indexed) {
            fStatus->SetText(BrowserUI().String("browser.layout.index_failed",
                "Layout saved but resource index could not be updated"));
            return;
        }
        fStatus->SetText(BrowserUI().String(saved
                ? "browser.layout.saved" : "browser.layout.failed", saved
                ? "Layout saved. Restart the application to reload"
                : "Layout save failed; original document unchanged"));
    }

    ResourceEntry fEntry;
    std::string fWindowKey;
    BRect fWindowFrame;
    std::map<std::string, std::string> fValues;
    std::vector<LayoutHandle> fHandles;
    LayoutCanvas* fCanvas;
    BStringView* fStatus;
    BTextControl* fID;
    BTextControl* fType;
    BTextControl* fCaption;
    BTextControl* fAction;
    BTextControl* fWindowGeometry;
};

class ResourceListWindow : public BWindow {
public:
    ResourceListWindow(const std::string& kind,
        const std::vector<ResourceEntry>& resources)
        : BWindow(BrowserUI().Rect("browser.entries.window", BRect(125, 105, 705, 605)),
              (kind + BrowserUI().String("browser.entries.title_suffix", " — Resources")).c_str(), B_TITLED_WINDOW, 0),
          fResources(resources), fList(NULL)
    {
        BView* background = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(BrowserUI().Color("browser.entries.background", rgb_color{238, 238, 236, 255}));
        AddChild(background);
        const OSTenDialogAction actions[] = {
            {"browser.open_entry", kOpenEntry}
        };
        OSTenDialogControls controls = OSTenCreateDialogControls(
            background, BrowserUI(), "browser.entries", actions,
            sizeof(actions) / sizeof(actions[0]));
        OSTenDialogControls::const_iterator list = controls.find("resource_entries");
        if (list != controls.end()
            && strcmp(BrowserUI().String(
                "browser.entries.control.resource_entries.type", ""),
                "list") == 0)
            fList = static_cast<BListView*>(list->second);
        if (fList != NULL) {
            for (size_t i = 0; i < resources.size(); ++i)
                fList->AddItem(new EntryItem(resources[i]));
        }
        Show();
    }

    void MessageReceived(BMessage* message) override
    {
        if (message->what == kOpenEntry) {
            if (fList == NULL)
                return;
            int32 index = fList->CurrentSelection();
            if (index < 0 || (size_t)index >= fResources.size())
                return;
            const ResourceEntry& item = fResources[index];
            if (item.artwork)
                new PixelEditorWindow(item);
            else if (item.binaryAsset)
                new BinaryAssetEditorWindow(item);
            else if (IsMenuSequence(item))
                new MenuEditorWindow(item);
            else if (EndsWith(item.key, ".controls")
                && (item.type == "DLOG" || item.type == "NTFY"
                    || item.type == "WLAY"))
                new LayoutEditorWindow(item);
            else if (item.type == "COLR")
                new ColorEditorWindow(item);
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
        : BWindow(BrowserUI().Rect("browser.types.window", BRect(90, 75, 620, 595)),
              BrowserUI().String("browser.types.title", "OSTen ResEdit — Resource Types"), B_TITLED_WINDOW,
              B_NORMAL_WINDOW_FEEL, B_QUIT_ON_WINDOW_CLOSE),
          fPicker(NULL), fTypesList(NULL), fCaption(NULL)
    {
        BView* bg = new BView(Bounds(), "background",
            B_FOLLOW_ALL, B_WILL_DRAW);
        bg->SetViewColor(BrowserUI().Color("browser.types.background", rgb_color{239, 239, 237, 255}));
        AddChild(bg);
        // Even the Resource Editor's own browser is a structured OSTen
        // resource. The Owner may reorder, resize, replace or delete its
        // controls through the same Resource Editor used for Finder/System.
        const OSTenDialogAction actions[] = {
            {"browser.open", kOpenFile},
            {"browser.refresh", kRefresh},
            {"browser.open_type", kOpenType}
        };
        OSTenDialogControls controls = OSTenCreateDialogControls(
            bg, BrowserUI(), "browser.types", actions,
            sizeof(actions) / sizeof(actions[0]));
        OSTenDialogControls::const_iterator caption = controls.find("current");
        fCaption = caption == controls.end()
            || strcmp(BrowserUI().String(
                "browser.types.control.current.type", ""), "label") != 0
            ? NULL : static_cast<BStringView*>(caption->second);
        OSTenDialogControls::const_iterator types = controls.find("resource_types");
        fTypesList = types == controls.end()
            || strcmp(BrowserUI().String(
                "browser.types.control.resource_types.type", ""), "list") != 0
            ? NULL : static_cast<BListView*>(types->second);
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
                if (fTypesList == NULL)
                    return;
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
                candidates.push_back(Parent(path) + "/Resources/Interface.ostr");
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
        (new BAlert(BrowserUI().String("browser.alert.unsupported_title", "Unsupported application"),
            BrowserUI().String("browser.alert.unsupported_message",
                "This selection has no editable OSTen resource catalog.\n"
                "Choose a native OSTen app or an .ostr file. Editing arbitrary "
                "compiled Haiku resources is not implemented yet."),
            "OK"))->Go();
    }

    void _Scan()
    {
        if (fPath.empty() || fTypesList == NULL)
            return;
        FILE* file = fopen(fPath.c_str(), "rb");
        if (file == NULL) {
            (new BAlert(BrowserUI().String("browser.alert.file_unavailable_title", "Resource file unavailable"),
                BrowserUI().String("browser.alert.file_unavailable_message", "Could not read the selected resource manifest."), "OK"))->Go();
            return;
        }
        std::vector<ResourceEntry> entries;
        std::map<std::string, OSTenIndexedResource> index;
        OSTenReadResourceIndex(Parent(fPath) + "/ResourceIndex.osti", index);
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
            resource.key = line;
            std::map<std::string, OSTenIndexedResource>::const_iterator
                found = index.find(resource.key);
            resource.type = found == index.end()
                ? Classify(line, value) : found->second.type;
            resource.label = found == index.end()
                ? resource.key : found->second.name;
            resource.id = found == index.end()
                ? OSTenStableResourceID(fApplication, resource.key)
                : found->second.id;
            resource.path = fPath;
            resource.application = fApplication;
            resource.width = 0;
            resource.height = 0;
            resource.artwork = false;
            resource.binaryAsset = false;
            entries.push_back(resource);
        }
        bool readOK = !ferror(file);
        fclose(file);
        if (!readOK) {
            (new BAlert(BrowserUI().String("browser.alert.read_failed_title", "Resource read failed"),
                BrowserUI().String("browser.alert.read_failed_message", "The resource manifest could not be read completely."),
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
            std::map<std::string, OSTenIndexedResource>::const_iterator
                artInfo = index.find(basename);
            int width = 0, height = 0;
            if (artInfo != index.end() && artInfo->second.width > 0
                && artInfo->second.height > 0) {
                width = artInfo->second.width;
                height = artInfo->second.height;
            } else if (!KnownDimensions(basename, width, height)) {
                double side = sqrt((double)st.st_size / 4.0);
                width = (int)(side + 0.5);
                height = width;
            }
            if (width <= 0 || height <= 0 || width > 512 || height > 512
                || (size_t)st.st_size != (size_t)width * height * 4)
                continue;
            ResourceEntry resource;
            resource.type = artInfo == index.end()
                ? (width == height ? "ICON" : "IMAG")
                : artInfo->second.type;
            resource.key = basename;
            resource.label = artInfo == index.end()
                ? basename : artInfo->second.name;
            resource.id = artInfo == index.end()
                ? OSTenStableResourceID(fApplication, basename)
                : artInfo->second.id;
            resource.path = resourceFile;
            resource.application = fApplication;
            resource.width = width;
            resource.height = height;
            resource.artwork = true;
            resource.binaryAsset = false;
            entries.push_back(resource);
        }


        // Binary assets are native indexed files and never arbitrary binary
        // executables. Only recognized type/extension pairs are exposed.
        // Adding an asset is a matter of adding its typed index entry and
        // sibling file, not modifying or recompiling Resource Editor.
        directory.Rewind();
        while (directory.GetNextEntry(&image, true) == B_OK) {
            if (!image.IsFile())
                continue;
            BPath assetPath;
            if (image.GetPath(&assetPath) != B_OK)
                continue;
            const std::string nativePath(assetPath.Path());
            const std::string basename = Leaf(nativePath);
            const size_t dot = basename.rfind('.');
            if (dot == std::string::npos || dot == 0)
                continue;
            const std::string key = basename.substr(0, dot);
            const std::string suffix = basename.substr(dot);
            std::map<std::string, OSTenIndexedResource>::const_iterator
                resourceIndex = index.find(key);
            if (resourceIndex == index.end()
                || !IsNativeBinaryAsset(suffix, resourceIndex->second.type)
                || resourceIndex->second.width != 0
                || resourceIndex->second.height != 0)
                continue;
            bool duplicate = false;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (entries[i].key == key) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
                continue;
            ResourceEntry resource;
            resource.id = resourceIndex->second.id;
            resource.type = resourceIndex->second.type;
            resource.key = key;
            resource.label = resourceIndex->second.name;
            resource.path = nativePath;
            resource.application = fApplication;
            resource.width = 0;
            resource.height = 0;
            resource.artwork = false;
            resource.binaryAsset = true;
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
        if (fCaption != NULL)
            fCaption->SetText(fApplication.c_str());
        SetTitle((fApplication + BrowserUI().String("browser.types.title_suffix", " — Resource Types")).c_str());
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

BWindow*
StartOSTenResourceBrowser()
{
    return new ResourceBrowserWindow();
}
