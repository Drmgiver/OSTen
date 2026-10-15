/* SPDX-License-Identifier: MIT */

// OSTen's Classic Mac-inspired About This Computer window. Values are read
// from the running system, not hard-coded from the developer's build machine.

#include <Bitmap.h>
#include <Font.h>
#include <ListItem.h>
#include <ListView.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <OS.h>
#include <Roster.h>
#include <ScrollView.h>
#include <String.h>
#include <View.h>
#include <Window.h>

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "AboutComputer.h"
#include "OSTenWordmark.h"


namespace {

const uint32 kRefreshAbout = 'osar';
const uint32 kFocusAbout = 'osaf';

const rgb_color kPaper = { 242, 242, 238, 255 };
const rgb_color kInk = { 34, 34, 34, 255 };
const rgb_color kMuted = { 95, 95, 95, 255 };
const rgb_color kBlue = { 74, 113, 167, 255 };
const rgb_color kPaleBlue = { 226, 233, 243, 255 };
const rgb_color kBorder = { 155, 155, 155, 255 };
const rgb_color kWhite = { 255, 255, 255, 255 };

struct ApplicationUsage {
    team_id team;
    BString name;
    uint64 residentBytes;
};


static BString
FormatMemory(uint64 bytes)
{
    char value[64];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        snprintf(value, sizeof(value), "%.2f GB",
            (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024ULL) {
        snprintf(value, sizeof(value), "%.1f MB",
            (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(value, sizeof(value), "%.0f KB",
            (double)bytes / 1024.0);
    }
    return BString(value);
}


static BBitmap*
CreateWordmark()
{
    BBitmap* bitmap = new BBitmap(BRect(0, 0,
        kOSTenWordmarkWidth - 1, kOSTenWordmarkHeight - 1), B_RGBA32);
    if (bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return NULL;
    }

    const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8* pixels = (uint8*)bitmap->Bits();
    int32 stride = bitmap->BytesPerRow();
    for (int y = 0; y < kOSTenWordmarkHeight; ++y) {
        for (int x = 0; x < kOSTenWordmarkWidth; ++x) {
            const char* index = strchr(alphabet, kOSTenWordmarkPixels[y][x]);
            if (index == NULL) {
                delete bitmap;
                return NULL;
            }
            uint32 rgba = kOSTenWordmarkPalette[index - alphabet];
            uint8* bgra = pixels + y * stride + x * 4;
            bgra[0] = (rgba >> 8) & 255;
            bgra[1] = (rgba >> 16) & 255;
            bgra[2] = (rgba >> 24) & 255;
            bgra[3] = rgba & 255;
        }
    }
    return bitmap;
}


static uint64
ResidentAreaBytes(team_id team)
{
    // This is resident area memory for this team, not its requested VM size.
    // Shared pages can appear in more than one team's total. This estimate
    // must never be summed and presented as unique system-wide RAM usage.
    uint64 bytes = 0;
    ssize_t cookie = 0;
    area_info area;
    while (get_next_area_info(team, &cookie, &area) == B_OK)
        bytes += area.ram_size;
    return bytes;
}


static std::vector<ApplicationUsage>
RunningApplications()
{
    std::vector<ApplicationUsage> result;
    int32 cookie = 0;
    team_info team;
    BRoster roster;

    while (get_next_team_info(&cookie, &team) == B_OK) {
        app_info app;
        // Only teams registered as applications, not arbitrary OS daemons.
        if (roster.GetRunningAppInfo(team.team, &app) != B_OK)
            continue;

        ApplicationUsage usage;
        usage.team = team.team;
        if (strcmp(app.signature, "application/x-vnd.OSTen-Finder") == 0)
            usage.name = "Finder";
        else if (strcmp(app.signature,
                "application/x-vnd.OSTen-System") == 0) {
            usage.name = "OSTen System";
        } else if (app.ref.name != NULL && app.ref.name[0] != '\0')
            usage.name = app.ref.name;
        else
            usage.name = team.name;

        usage.residentBytes = ResidentAreaBytes(team.team);
        result.push_back(usage);
    }

    std::sort(result.begin(), result.end(),
        [](const ApplicationUsage& a, const ApplicationUsage& b) {
            int32 comparison = a.name.ICompare(b.name);
            if (comparison != 0)
                return comparison < 0;
            return a.team < b.team;
        });
    return result;
}


class ApplicationMemoryItem : public BListItem {
public:
    ApplicationMemoryItem(const ApplicationUsage& usage, uint64 largest)
        :
        fTeam(usage.team),
        fName(usage.name),
        fBytes(usage.residentBytes),
        fLargest(largest)
    {
        SetHeight(34);
    }

    team_id Team() const { return fTeam; }

    void Update(BView* owner, const BFont* font) override
    {
        BListItem::Update(owner, font);
        SetHeight(34);
    }

    void DrawItem(BView* owner, BRect frame, bool complete) override
    {
        owner->SetHighColor(IsSelected() ? kPaleBlue : kWhite);
        owner->FillRect(frame);
        owner->SetHighColor(kBorder);
        owner->StrokeLine(BPoint(frame.left, frame.bottom),
            BPoint(frame.right, frame.bottom));

        owner->SetHighColor(kInk);
        owner->SetFont(be_plain_font);
        BFont font;
        owner->GetFont(&font);
        BString label(fName);
        font.TruncateString(&label, B_TRUNCATE_END, 288);
        owner->DrawString(label.String(), BPoint(frame.left + 10, frame.top + 20));

        // App bars are scaled against the largest currently listed app.
        // They are not a partition of the computer's physical RAM.
        float left = frame.right - 181;
        BRect bar(left, frame.top + 11, left + 88, frame.top + 22);
        owner->SetHighColor(kBorder);
        owner->StrokeRect(bar);
        float fraction = fLargest > 0
            ? (float)((double)fBytes / (double)fLargest) : 0.0f;
        if (fraction > 1)
            fraction = 1;
        if (fraction > 0) {
            float width = std::max(1.0f, fraction * 86.0f);
            owner->SetHighColor(kBlue);
            owner->FillRect(BRect(bar.left + 1, bar.top + 1,
                bar.left + width, bar.bottom - 1));
        }

        BString memory = FormatMemory(fBytes);
        owner->SetHighColor(kInk);
        owner->DrawString(memory.String(),
            BPoint(frame.right - 83, frame.top + 20));
    }

private:
    team_id fTeam;
    BString fName;
    uint64 fBytes;
    uint64 fLargest;
};


class AboutOverviewView : public BView {
public:
    AboutOverviewView(BRect frame)
        :
        BView(frame, "About overview", B_FOLLOW_ALL, B_WILL_DRAW),
        fWordmark(CreateWordmark()),
        fHasInfo(false)
    {
        memset(&fInfo, 0, sizeof(fInfo));
        SetViewColor(kPaper);
    }

    ~AboutOverviewView() override
    {
        delete fWordmark;
    }

    void SetSystemInfo(const system_info* info)
    {
        fHasInfo = info != NULL;
        if (info != NULL)
            fInfo = *info;
        Invalidate();
    }

    void Draw(BRect update) override
    {
        SetHighColor(kPaper);
        FillRect(Bounds());
        SetHighColor(kInk);

        if (fWordmark != NULL) {
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            DrawBitmap(fWordmark, fWordmark->Bounds(), BRect(19, 13, 196, 62));
            SetDrawingMode(B_OP_COPY);
        } else {
            BFont title(be_bold_font);
            title.SetSize(24);
            SetFont(&title);
            DrawString("OSTen", BPoint(20, 46));
        }

        BFont caption(be_plain_font);
        caption.SetSize(11);
        SetFont(&caption);
        SetHighColor(kMuted);
        DrawString("Developer Preview", BPoint(224, 35));
        DrawString("Classic Macintosh spirit. BeOS foundation.",
            BPoint(224, 53));

        SetHighColor(kBorder);
        StrokeLine(BPoint(18, 78), BPoint(560, 78));

        BFont ordinary(be_plain_font);
        ordinary.SetSize(12);
        BFont bold(be_bold_font);
        bold.SetSize(12);

        uint64 installed = 0;
        uint64 inUse = 0;
        uint64 swapTotal = 0;
        uint64 swapUsed = 0;
        if (fHasInfo) {
            uint64 pageSize = (uint64)getpagesize();
            installed = fInfo.max_pages * pageSize;
            uint64 pagesInUse = std::min(fInfo.used_pages, fInfo.max_pages);
            inUse = pagesInUse * pageSize;
            swapTotal = fInfo.max_swap_pages * pageSize;
            swapUsed = (fInfo.max_swap_pages - std::min(fInfo.free_swap_pages,
                fInfo.max_swap_pages)) * pageSize;
        }

        _Label("Built-in Memory:", 20, 109, bold);
        _Value(fHasInfo ? FormatMemory(installed) : "Unavailable", 156,
            109, ordinary);
        _Label("Memory In Use:", 20, 135, bold);
        _Value(fHasInfo ? FormatMemory(inUse) : "Unavailable", 156,
            135, ordinary);
        _Label("Available Memory:", 20, 161, bold);
        _Value(fHasInfo ? FormatMemory(installed - inUse) : "Unavailable",
            156, 161, ordinary);

        _Label("Processors:", 311, 109, bold);
        BString processorCount;
        if (fHasInfo)
            processorCount.SetToFormat("%" B_PRIu32, fInfo.cpu_count);
        else
            processorCount = "Unavailable";
        _Value(processorCount, 427, 109, ordinary);

        _Label("Virtual Memory:", 311, 135, bold);
        BString swapText;
        if (fHasInfo && swapTotal > 0) {
            swapText = FormatMemory(swapUsed);
            swapText << " used";
        } else if (fHasInfo) {
            swapText = "Swap disabled";
        } else {
            swapText = "Unavailable";
        }
        _Value(swapText, 427, 135, ordinary);

        _Label("Swap Capacity:", 311, 161, bold);
        _Value(fHasInfo ? FormatMemory(swapTotal) : "Unavailable", 427,
            161, ordinary);

        BRect usageBar(20, 178, 560, 196);
        SetHighColor(kWhite);
        FillRect(usageBar);
        SetHighColor(kBorder);
        StrokeRect(usageBar);
        if (installed > 0) {
            float usedFraction = (float)((double)inUse / (double)installed);
            SetHighColor(kBlue);
            FillRect(BRect(usageBar.left + 1, usageBar.top + 1,
                usageBar.left + 1 + 538 * usedFraction, usageBar.bottom - 1));
        }

        SetHighColor(kMuted);
        SetFont(&caption);
        DrawString("Physical memory in use", BPoint(20, 212));

        SetHighColor(kBorder);
        StrokeLine(BPoint(18, 222), BPoint(560, 222));
        SetHighColor(kInk);
        SetFont(&bold);
        DrawString("Running Applications", BPoint(20, 244));
        SetFont(&caption);
        SetHighColor(kMuted);
        DrawString("Relative memory bar / approximate resident memory",
            BPoint(279, 244));

        SetHighColor(kMuted);
        DrawString("App memory counts shared areas and may not add up to the",
            BPoint(20, 490));
        DrawString("system total. Values refresh every two seconds.",
            BPoint(20, 505));
    }

private:
    void _Label(const char* title, float x, float y, const BFont& font)
    {
        SetHighColor(kInk);
        SetFont(&font);
        DrawString(title, BPoint(x, y));
    }

    void _Value(const BString& value, float x, float y, const BFont& font)
    {
        SetHighColor(kInk);
        SetFont(&font);
        DrawString(value.String(), BPoint(x, y));
    }

    BBitmap* fWordmark;
    bool fHasInfo;
    system_info fInfo;
};


class AboutComputerWindow : public BWindow {
public:
    AboutComputerWindow()
        :
        BWindow(BRect(145, 80, 725, 605), "About OSTen",
            B_TITLED_WINDOW, B_NORMAL_WINDOW_FEEL,
            B_NOT_ZOOMABLE | B_NOT_RESIZABLE),
        fOverview(NULL),
        fList(NULL),
        fRefreshRunner(NULL)
    {
        BRect frame = Bounds();
        fOverview = new AboutOverviewView(frame);
        AddChild(fOverview);

        fList = new BListView(BRect(19, 262, 542, 463),
            "Running applications", B_SINGLE_SELECTION_LIST,
            B_FOLLOW_LEFT_TOP);
        fList->SetViewColor(kWhite);
        BScrollView* listScroller = new BScrollView("Application memory",
            fList, B_FOLLOW_LEFT_TOP, 0, false, true);
        fOverview->AddChild(listScroller);

        _Refresh();
        BMessage refresh(kRefreshAbout);
        fRefreshRunner = new BMessageRunner(BMessenger(this),
            &refresh, 2000000);
    }

    ~AboutComputerWindow() override
    {
        delete fRefreshRunner;
        for (int32 i = 0; i < fList->CountItems(); ++i)
            delete fList->ItemAt(i);
        fList->MakeEmpty();
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case kFocusAbout:
                Activate(true);
                break;
            case kRefreshAbout:
                _Refresh();
                break;
            default:
                BWindow::MessageReceived(message);
        }
    }

private:
    void _Refresh()
    {
        system_info info;
        fOverview->SetSystemInfo(get_system_info(&info) == B_OK
            ? &info : NULL);

        std::vector<ApplicationUsage> apps = RunningApplications();
        uint64 largest = 0;
        for (size_t i = 0; i < apps.size(); ++i)
            largest = std::max(largest, apps[i].residentBytes);

        team_id selected = -1;
        int32 selectedIndex = fList->CurrentSelection();
        if (selectedIndex >= 0)
            selected = static_cast<ApplicationMemoryItem*>(
                fList->ItemAt(selectedIndex))->Team();
        float previousScroll = fList->Bounds().top;

        while (fList->CountItems() > 0)
            delete fList->RemoveItem((int32)0);

        for (size_t i = 0; i < apps.size(); ++i) {
            fList->AddItem(new ApplicationMemoryItem(apps[i], largest));
            if (apps[i].team == selected)
                fList->Select((int32)i);
        }
        fList->ScrollTo(0, previousScroll);
    }

    AboutOverviewView* fOverview;
    BListView* fList;
    BMessageRunner* fRefreshRunner;
};

} // namespace


BWindow*
CreateAboutComputerWindow()
{
    return new AboutComputerWindow();
}
