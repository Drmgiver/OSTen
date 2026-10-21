/* SPDX-License-Identifier: MIT */
// Classic menu-bar controls, using Haiku's native roster and notifications.

#include "SystemExtras.h"
#include "OSTenResources.h"

#include "FinderSwitcherIcon.h"

#include <Alert.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Button.h>
#include <File.h>
#include <MimeType.h>
#include <Node.h>
#include <Font.h>
#include <List.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Messenger.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <View.h>
#include <Window.h>

#include <algorithm>
#include <stdio.h>
#include <string.h>


namespace {

struct AppChoice {
    BString name;
    team_id team;
    bool active;
};
// Bitmap uses BGRA byte order; palette entries are 0xRRGGBBAA.
// Keep a distinct copy for each menu item so ownership is unambiguous.
static BBitmap*
FinderFaceIcon()
{
    BBitmap* override = OSTenSystemResources().Bitmap("switcher-finder",
        kOSTenFinderSwitcherSize, kOSTenFinderSwitcherSize);
    if (override != NULL)
        return override;
    BBitmap* bitmap = new BBitmap(
        BRect(0, 0, kOSTenFinderSwitcherSize - 1,
            kOSTenFinderSwitcherSize - 1), B_RGBA32);
    if (bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return NULL;
    }

    const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8* pixels = (uint8*)bitmap->Bits();
    int32 stride = bitmap->BytesPerRow();
    for (int32 y = 0; y < kOSTenFinderSwitcherSize; ++y) {
        for (int32 x = 0; x < kOSTenFinderSwitcherSize; ++x) {
            const char* c = strchr(alphabet, kOSTenFinderSwitcherRows[y][x]);
            if (c == NULL) {
                delete bitmap;
                return NULL;
            }
            uint32 rgba = kOSTenFinderSwitcherPalette[c - alphabet];
            uint8* bgra = pixels + y * stride + x * 4;
            bgra[0] = (rgba >> 8) & 255;
            bgra[1] = (rgba >> 16) & 255;
            bgra[2] = (rgba >> 24) & 255;
            bgra[3] = rgba & 255;
        }
    }
    return bitmap;
}


// The Finder artwork is an OSTen-owned resource. Every other application
// supplies its own icon through Haiku's existing AppFileInfo/MIME database.
static BBitmap*
IconForApplication(team_id team, bool finder)
{
    if (finder)
        return FinderFaceIcon();

    BBitmap* bitmap = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
    if (bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return NULL;
    }

    BRoster roster;
    app_info app;
    if (roster.GetRunningAppInfo(team, &app) == B_OK) {
        BFile file(&app.ref, B_READ_ONLY);
        if (file.InitCheck() == B_OK) {
            BAppFileInfo fileInfo(&file);
            if (fileInfo.GetIcon(bitmap, B_MINI_ICON) == B_OK)
                return bitmap;
        }
        BMimeType mime(app.signature);
        if (mime.InitCheck() == B_OK
            && mime.GetIcon(bitmap, B_MINI_ICON) == B_OK)
            return bitmap;
    }

    delete bitmap;
    return NULL;
}


// An actual icon to the left of each application name in the dropdown.
class AppIconMenuItem : public BMenuItem {
public:
    AppIconMenuItem(const char* name, BMessage* message, BBitmap* icon)
        : BMenuItem(name, message), fIcon(icon)
    {
    }

    ~AppIconMenuItem() override
    {
        delete fIcon;
    }

protected:
    void GetContentSize(float* width, float* height) override
    {
        BMenuItem::GetContentSize(width, height);
        if (width != NULL)
            *width += 24;
        if (height != NULL && *height < 18)
            *height = 18;
    }

    void DrawContent() override
    {
        BMenu* menu = Menu();
        if (menu == NULL)
            return;
        BPoint where = ContentLocation();
        if (fIcon != NULL) {
            menu->SetDrawingMode(B_OP_ALPHA);
            menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            menu->DrawBitmap(fIcon, fIcon->Bounds(),
                BRect(where.x + 1, where.y,
                    where.x + 17, where.y + 16));
        } else {
            // Neutral fallback for apps without registered artwork.
            menu->SetDrawingMode(B_OP_COPY);
            menu->StrokeRect(BRect(where.x + 2, where.y + 2,
                where.x + 15, where.y + 14));
        }
        menu->SetDrawingMode(B_OP_OVER);
        BFont font;
        menu->GetFont(&font);
        font_height metrics;
        font.GetHeight(&metrics);
        menu->DrawString(Label(), BPoint(where.x + 24,
            where.y + metrics.ascent));
    }

private:
    BBitmap* fIcon;
};


static std::vector<AppChoice>
GetApplications()
{
    BRoster roster;
    BList teams;
    roster.GetAppList(&teams);
    app_info current;
    team_id active = roster.GetActiveAppInfo(&current) == B_OK
        ? current.team : -1;
    std::vector<AppChoice> apps;
    for (int32 i = 0; i < teams.CountItems(); ++i) {
        team_id id = (team_id)(addr_t)teams.ItemAt(i);
        app_info info;
        if (roster.GetRunningAppInfo(id, &info) != B_OK
            || (info.flags & B_BACKGROUND_APP)
            || strcmp(info.signature, "application/x-vnd.OSTen-System") == 0
            || strcmp(info.signature, "application/x-vnd.Be-TSKB") == 0)
            continue;
        AppChoice choice;
        choice.team = id;
        choice.active = id == active;
        if (strcmp(info.signature, "application/x-vnd.OSTen-Finder") == 0)
            choice.name = "Finder";
        else if (info.ref.name != NULL && *info.ref.name != '\0')
            choice.name = info.ref.name;
        else
            choice.name = "Application";
        apps.push_back(choice);
    }
    std::sort(apps.begin(), apps.end(),
        [](const AppChoice& a, const AppChoice& b) {
            int c = a.name.ICompare(b.name);
            return c != 0 ? c < 0 : a.team < b.team;
        });
    return apps;
}

// Right-most menu-bar status control, initially displaying Finder and then
// whichever non-background application is in front. The dropdown is built
// from the live roster, so there are no hardcoded application entries.
class AppSwitcherView : public BView {
public:
    AppSwitcherView(BRect frame)
        : BView(frame, "OSTen App Switcher",
            B_FOLLOW_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW),
        fLastTeam(-1), fIcon(FinderFaceIcon())
    {
        SetViewColor(ui_color(B_MENU_BACKGROUND_COLOR));
    }

    ~AppSwitcherView() override
    {
        delete fIcon;
    }

    void Draw(BRect update) override
    {
        _UpdateActiveIcon();
        SetHighColor(ui_color(B_MENU_BACKGROUND_COLOR));
        FillRect(Bounds());
        SetHighColor(OSTenSystemResources().Color("color.system_switcher.1", rgb_color{120, 120, 120, 255}));
        StrokeLine(BPoint(0, 5), BPoint(0, 21));

        if (fIcon != NULL) {
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            DrawBitmap(fIcon, fIcon->Bounds(), OSTenSystemResources().Rect(
                "switcher.icon_frame", BRect(4, 1, 25, 22)));
            SetDrawingMode(B_OP_COPY);
        } else {
            // An unregistered third-party app still gets a selectable icon.
            SetHighColor(OSTenSystemResources().Color("color.system_switcher.2", rgb_color{85, 85, 85, 255}));
            StrokeRect(BRect(6, 4, 23, 20));
            StrokeLine(BPoint(7, 8), BPoint(22, 8));
        }

        // Small drop arrow, no text label beside the icon.
        SetHighColor(OSTenSystemResources().Color("color.system_switcher.3", rgb_color{50, 50, 50, 255}));
        StrokeLine(BPoint(29, 11), BPoint(33, 15));
        StrokeLine(BPoint(33, 15), BPoint(37, 11));
    }

    void MouseDown(BPoint where) override
    {
        BPopUpMenu* popup = new BPopUpMenu(
            OSTenSystemResources().String("switcher.title", "Running Applications"),
            false, false);
        std::vector<AppChoice> apps = GetApplications();
        if (apps.empty()) {
            BMenuItem* empty = new BMenuItem(
                OSTenSystemResources().String("switcher.empty",
                    "No running applications"),
                NULL);
            empty->SetEnabled(false);
            popup->AddItem(empty);
        }
        for (size_t i = 0; i < apps.size(); ++i) {
            BMessage* action = new BMessage(kOSTenActivateApplication);
            action->AddInt32("team", apps[i].team);
            BBitmap* icon = IconForApplication(apps[i].team,
                apps[i].name == "Finder");
            BMenuItem* item = new AppIconMenuItem(
                apps[i].name.String(), action, icon);
            item->SetMarked(apps[i].active);
            item->SetTarget(BMessenger("application/x-vnd.OSTen-System"));
            popup->AddItem(item);
        }

        popup->SetAsyncAutoDestruct(true);
        BPoint start(Bounds().left, Bounds().bottom + 1);
        ConvertToScreen(&start);
        popup->Go(start, true, false, true);
    }

private:
    void _UpdateActiveIcon()
    {
        std::vector<AppChoice> apps = GetApplications();
        for (size_t i = 0; i < apps.size(); ++i) {
            if (!apps[i].active)
                continue;
            if (apps[i].team == fLastTeam)
                return;
            // Cache while the same app is active. The menu bar receives
            // a short periodic repaint, not a fresh icon read every tick.
            BBitmap* icon = IconForApplication(apps[i].team,
                apps[i].name == "Finder");
            delete fIcon;
            fIcon = icon;
            fLastTeam = apps[i].team;
            return;
        }

        // Floating OSTen System and alert windows can temporarily own
        // focus. Don't replace the last application's face with nothing.
    }

    team_id fLastTeam;
    BBitmap* fIcon;
};


class ClockView : public BView {
public:
    ClockView(BRect frame)
        : BView(frame, "OSTen Clock", B_FOLLOW_RIGHT | B_FOLLOW_TOP,
            B_WILL_DRAW)
    {
        SetViewColor(ui_color(B_MENU_BACKGROUND_COLOR));
    }

    void Draw(BRect update) override
    {
        SetHighColor(ui_color(B_MENU_BACKGROUND_COLOR));
        FillRect(Bounds());
        time_t now = time(NULL);
        struct tm clock;
        localtime_r(&now, &clock);
        char text[64];
        strftime(text, sizeof(text),
            OSTenSystemResources().String("clock.format", "%a %I:%M %p"), &clock);
        SetHighColor(OSTenSystemResources().Color("color.system_clock.1", rgb_color{24, 24, 24, 255}));
        BFont font = OSTenSystemResources().Font(
            "font.system_clock", be_plain_font);
        SetFont(&font);
        DrawString(text, BPoint(
            Bounds().right - StringWidth(text)
                - OSTenSystemResources().Number("clock.text_right_padding",
                    7, 0, 50),
            OSTenSystemResources().Number("clock.text_baseline", 17, 10, 35)));
    }

    void MouseDown(BPoint where) override
    {
        time_t now = time(NULL);
        struct tm clock;
        localtime_r(&now, &clock);
        char text[128];
        strftime(text, sizeof(text),
            OSTenSystemResources().String("clock.long_format",
                "%A, %B %d, %Y\\n%I:%M:%S %p %Z"),
            &clock);
        (new BAlert(OSTenSystemResources().String("dialog.clock.title", "OSTen Clock"), text,
            OSTenSystemResources().String("dialog.ok", "OK")))->Go(NULL);
    }
};


class NoticeButton : public BView {
public:
    NoticeButton(BRect frame)
        : BView(frame, "OSTen Notices", B_FOLLOW_RIGHT | B_FOLLOW_TOP,
            B_WILL_DRAW), fUnread(0)
    {
        SetViewColor(ui_color(B_MENU_BACKGROUND_COLOR));
    }

    void SetUnread(int32 value)
    {
        fUnread = value;
        Invalidate();
    }

    void Draw(BRect update) override
    {
        SetHighColor(ui_color(B_MENU_BACKGROUND_COLOR));
        FillRect(Bounds());
        SetHighColor(OSTenSystemResources().Color("color.system_notices.1", rgb_color{120, 120, 120, 255}));
        StrokeLine(BPoint(0, 5), BPoint(0, 21));
        SetHighColor(OSTenSystemResources().Color("color.system_notices.2", rgb_color{24, 24, 24, 255}));
        BFont font = OSTenSystemResources().Font(
            "font.system_notices", be_plain_font);
        SetFont(&font);
        BString label(OSTenSystemResources().String("notification.notices", "Notices"));
        if (fUnread > 0)
            label.SetToFormat(OSTenSystemResources().String(
                "notification.notices_count", "Notices (%" B_PRId32 ")"),
                fUnread);
        DrawString(label.String(), BPoint(
            OSTenSystemResources().Number("notification.text_left", 9, 0, 60),
            OSTenSystemResources().Number("notification.text_baseline", 17, 10, 35)));
    }

    void MouseDown(BPoint where) override
    {
        Window()->PostMessage(kOSTenShowNotices);
    }
private:
    int32 fUnread;
};


// Native window: newest notices first, double-click to see the full message.
class NoticesWindow : public BWindow {
public:
    NoticesWindow(const BMessenger& owner, BRect frame)
        : BWindow(frame, OSTenSystemResources().String(
                "notification.title", "Notification Center"), B_TITLED_WINDOW,
            B_NORMAL_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE),
        fOwner(owner), fList(NULL)
    {
        BView* background = new BView(Bounds(), "Notices",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(OSTenSystemResources().Color("color.system_notifications_window.1", rgb_color{242, 242, 238, 255}));
        AddChild(background);
        fList = new BListView(OSTenSystemResources().Rect(
                "notification.list_frame",
                BRect(12, 12, Bounds().right - 34,
                    Bounds().bottom - 60)), "Recent notifications",
            B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL);
        fList->SetViewColor(OSTenSystemResources().Color("color.system_notifications_window.2", rgb_color{255, 255, 255, 255}));
        fList->SetInvocationMessage(new BMessage('osnd'));
        background->AddChild(new BScrollView("Notice history", fList,
            B_FOLLOW_ALL, 0, false, true));
        background->AddChild(new BButton(OSTenSystemResources().Rect(
                "notification.clear_frame",
                BRect(14, Bounds().bottom - 42,
                    118, Bounds().bottom - 14)), "Clear",
            OSTenSystemResources().String("notification.clear", "Clear All"),
            new BMessage(kOSTenClearNotices)));
        background->AddChild(new BButton(OSTenSystemResources().Rect(
                "notification.test_frame",
                BRect(Bounds().right - 166, Bounds().bottom - 42,
                    Bounds().right - 14, Bounds().bottom - 14)),
            "Test", OSTenSystemResources().String("notification.test",
                "Test Notification"), new BMessage(kOSTenTestNotice)));
    }

    bool QuitRequested() override
    {
        Hide();
        return false;
    }

    void MessageReceived(BMessage* message) override
    {
        if (message->what == kOSTenClearNotices
            || message->what == kOSTenTestNotice) {
            fOwner.SendMessage(message);
            return;
        }
        if (message->what == 'osnd') {
            int32 index = fList->CurrentSelection();
            if (index >= 0 && (size_t)index < fDisplayed.size()) {
                const OSTenNotice& n = fDisplayed[index];
                (new BAlert(n.title.String(),
                    n.content.IsEmpty() ? "No additional details."
                    : n.content.String(), "OK"))->Go(NULL);
            }
            return;
        }
        BWindow::MessageReceived(message);
    }

    void SetEntries(const std::vector<OSTenNotice>& notices)
    {
        fDisplayed.assign(notices.rbegin(), notices.rend());
        while (fList->CountItems() > 0)
            delete fList->RemoveItem((int32)0);
        for (size_t i = 0; i < fDisplayed.size(); ++i) {
            const OSTenNotice& n = fDisplayed[i];
            struct tm local;
            localtime_r(&n.when, &local);
            char when[32];
            strftime(when, sizeof(when), "%I:%M %p", &local);
            BString text(when);
            text << "  " << n.title;
            if (!n.content.IsEmpty())
                text << " - " << n.content;
            fList->AddItem(new BStringItem(text.String()));
        }
        if (fDisplayed.empty()) {
            BStringItem* empty = new BStringItem("No notifications yet.");
            empty->SetEnabled(false);
            fList->AddItem(empty);
        }
    }

private:
    BMessenger fOwner;
    BListView* fList;
    std::vector<OSTenNotice> fDisplayed;
};
} // namespace

BView* CreateOSTenApplicationSwitcher(BRect frame)
{
    return new AppSwitcherView(frame);
}
void RefreshOSTenApplicationSwitcher(BView* view)
{
    if (view != NULL) view->Invalidate();
}
BView* CreateOSTenClockView(BRect frame) { return new ClockView(frame); }
BView* CreateOSTenNoticeButton(BRect frame) { return new NoticeButton(frame); }
void RefreshOSTenClock(BView* clock) { if (clock) clock->Invalidate(); }
void SetOSTenUnreadCount(BView* button, int32 count)
{
    if (button) static_cast<NoticeButton*>(button)->SetUnread(count);
}
BWindow* CreateOSTenNotificationCenter(const BMessenger& owner, BRect frame)
{
    return new NoticesWindow(owner, frame);
}
void UpdateOSTenNotifications(BWindow* window,
    const std::vector<OSTenNotice>& notices)
{
    if (window != NULL && window->Lock()) {
        static_cast<NoticesWindow*>(window)->SetEntries(notices);
        window->Unlock();
    }
}
