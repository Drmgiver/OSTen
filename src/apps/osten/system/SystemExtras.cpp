/* SPDX-License-Identifier: MIT */
// Classic menu-bar controls, using Haiku's native roster and notifications.

#include "SystemExtras.h"

#include <Alert.h>
#include <Button.h>
#include <Font.h>
#include <List.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuItem.h>
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
            || strcmp(info.signature, "application/x-vnd.OSTen-System") == 0)
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

class ApplicationsMenu : public BMenu {
public:
    ApplicationsMenu() : BMenu("Applications") {}

    bool AddDynamicItem(add_state state) override
    {
        if (state != B_INITIAL_ADD)
            return false;
        while (CountItems() != 0)
            delete RemoveItem((int32)0);
        std::vector<AppChoice> apps = GetApplications();
        if (apps.empty()) {
            BMenuItem* empty = new BMenuItem("No running applications", NULL);
            empty->SetEnabled(false);
            AddItem(empty);
        }
        for (size_t i = 0; i < apps.size(); ++i) {
            BMessage* action = new BMessage(kOSTenActivateApplication);
            action->AddInt32("team", apps[i].team);
            BMenuItem* item = new BMenuItem(apps[i].name.String(), action);
            item->SetMarked(apps[i].active);
            item->SetTarget(BMessenger("application/x-vnd.OSTen-System"));
            AddItem(item);
        }
        return false;
    }
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
        strftime(text, sizeof(text), "%a %I:%M %p", &clock);
        SetHighColor(24, 24, 24);
        SetFont(be_plain_font);
        DrawString(text, BPoint(Bounds().right - StringWidth(text) - 7, 17));
    }

    void MouseDown(BPoint where) override
    {
        time_t now = time(NULL);
        struct tm clock;
        localtime_r(&now, &clock);
        char text[128];
        strftime(text, sizeof(text), "%A, %B %d, %Y\n%I:%M:%S %p %Z",
            &clock);
        (new BAlert("OSTen Clock", text, "OK"))->Go(NULL);
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
        SetHighColor(120, 120, 120);
        StrokeLine(BPoint(0, 5), BPoint(0, 21));
        SetHighColor(24, 24, 24);
        SetFont(be_plain_font);
        BString label("Notices");
        if (fUnread > 0)
            label.SetToFormat("Notices (%" B_PRId32 ")", fUnread);
        DrawString(label.String(), BPoint(9, 17));
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
        : BWindow(frame, "Notification Center", B_TITLED_WINDOW,
            B_NORMAL_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE),
        fOwner(owner), fList(NULL)
    {
        BView* background = new BView(Bounds(), "Notices",
            B_FOLLOW_ALL, B_WILL_DRAW);
        background->SetViewColor(242, 242, 238);
        AddChild(background);
        fList = new BListView(BRect(12, 12, Bounds().right - 34,
            Bounds().bottom - 60), "Recent notifications",
            B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL);
        fList->SetViewColor(255, 255, 255);
        fList->SetInvocationMessage(new BMessage('osnd'));
        background->AddChild(new BScrollView("Notice history", fList,
            B_FOLLOW_ALL, 0, false, true));
        background->AddChild(new BButton(BRect(14, Bounds().bottom - 42,
            118, Bounds().bottom - 14), "Clear", "Clear All",
            new BMessage(kOSTenClearNotices)));
        background->AddChild(new BButton(BRect(Bounds().right - 166,
            Bounds().bottom - 42, Bounds().right - 14, Bounds().bottom - 14),
            "Test", "Test Notification", new BMessage(kOSTenTestNotice)));
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

BMenu* CreateOSTenApplicationMenu() { return new ApplicationsMenu(); }
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
