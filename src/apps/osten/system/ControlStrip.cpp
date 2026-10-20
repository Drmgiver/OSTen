/* SPDX-License-Identifier: MIT */
// OSTen classic Control Strip: independent, collapsible desktop accessory.
// Module descriptors are intentionally separate from rendering and hit testing.
#include <Application.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <String.h>
#include <View.h>
#include <Window.h>

#include "ControlStrip.h"

namespace {

const uint32 kToggleStrip = 'cstg';
const uint32 kCloseStrip = 'csch';

struct StripModule {
    const char* name;
    const char* shortLabel;
    const char* summary;
};

// Each entry is one independent module slot. Future native modules can replace
// the information menu without touching the bar's layout or collapse logic.
const StripModule kModules[] = {
    {"Volume", "Vol", "Audio controls will be connected to the media system."},
    {"Display", "Disp", "Resolution and brightness controls are not wired yet."},
    {"Network", "Net", "Network status and interfaces are not wired yet."},
    {"Power", "Pwr", "Battery and power information are not wired yet."},
    {"Bluetooth", "BT", "Bluetooth status and pairing are not wired yet."},
    {"Drives", "Disk", "Mounted volumes and removable media are not wired yet."},
    {"Settings", "Set", "Control Strip preferences will be added here."}
};
const int kModuleCount = sizeof(kModules) / sizeof(kModules[0]);
const float kHandleWidth = 23;
const float kModuleWidth = 42;
const float kStripHeight = 30;

class StripView : public BView {
public:
    explicit StripView(BRect frame)
        : BView(frame, "OSTen Control Strip", B_FOLLOW_ALL,
            B_WILL_DRAW | B_FRAME_EVENTS), fCollapsed(true)
    {
        SetViewColor(B_TRANSPARENT_COLOR);
    }

    void Draw(BRect) override
    {
        const BRect bounds = Bounds();
        SetHighColor(48, 48, 52);
        FillRect(bounds);
        SetHighColor(222, 222, 225);
        FillRect(bounds.InsetByCopy(1, 1));
        SetHighColor(255, 255, 255);
        StrokeLine(BPoint(2, 2), BPoint(bounds.right - 2, 2));
        SetHighColor(110, 110, 116);
        StrokeLine(BPoint(2, bounds.bottom - 2),
            BPoint(bounds.right - 2, bounds.bottom - 2));

        SetHighColor(25, 25, 28);
        SetFont(be_bold_font);
        DrawString(fCollapsed ? ">" : "<", BPoint(8, 20));
        if (fCollapsed)
            return;

        SetFont(be_plain_font);
        for (int i = 0; i < kModuleCount; ++i) {
            const float left = kHandleWidth + i * kModuleWidth;
            SetHighColor(130, 130, 134);
            StrokeLine(BPoint(left, 4), BPoint(left, bounds.bottom - 4));
            SetHighColor(250, 250, 250);
            StrokeLine(BPoint(left + 1, 4),
                BPoint(left + 1, bounds.bottom - 4));
            SetHighColor(20, 20, 25);
            const char* label = kModules[i].shortLabel;
            const float width = StringWidth(label);
            DrawString(label, BPoint(left + (kModuleWidth - width) / 2, 20));
        }
    }

    void MouseDown(BPoint point) override
    {
        if (point.x < kHandleWidth) {
            Window()->PostMessage(kToggleStrip);
            return;
        }
        if (fCollapsed)
            return;
        int index = (int)((point.x - kHandleWidth) / kModuleWidth);
        if (index < 0 || index >= kModuleCount)
            return;
        BPopUpMenu* menu = new BPopUpMenu(kModules[index].name, false, false);
        BMenuItem* title = new BMenuItem(kModules[index].name, NULL);
        title->SetEnabled(false);
        menu->AddItem(title);
        menu->AddSeparatorItem();
        BMenuItem* info = new BMenuItem(kModules[index].summary, NULL);
        info->SetEnabled(false);
        menu->AddItem(info);
        if (index == kModuleCount - 1) {
            menu->AddSeparatorItem();
            menu->AddItem(new BMenuItem("Collapse Control Strip",
                new BMessage(kToggleStrip)));
            menu->AddItem(new BMenuItem("Hide Control Strip",
                new BMessage(kCloseStrip)));
        }
        menu->SetTargetForItems(Window());
        menu->SetAsyncAutoDestruct(true);
        ConvertToScreen(&point);
        menu->Go(point, true, false, true);
    }

    void SetCollapsed(bool collapsed)
    {
        fCollapsed = collapsed;
        Invalidate();
    }

private:
    bool fCollapsed;
};

class ControlStripWindow : public BWindow {
public:
    ControlStripWindow()
        : BWindow(_Frame(true), "Control Strip",
            B_NO_BORDER_WINDOW_LOOK, B_FLOATING_ALL_WINDOW_FEEL,
            B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE
                | B_NOT_RESIZABLE | B_NOT_MOVABLE
                | B_WILL_ACCEPT_FIRST_CLICK),
          fView(NULL), fCollapsed(true)
    {
        fView = new StripView(Bounds());
        AddChild(fView);
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
            case kToggleStrip:
                fCollapsed = !fCollapsed;
                ResizeTo(_Width(fCollapsed), kStripHeight - 1);
                MoveTo(_Frame(fCollapsed).LeftTop());
                fView->SetCollapsed(fCollapsed);
                return;
            case kCloseStrip:
                Hide();
                return;
            default:
                BWindow::MessageReceived(message);
                return;
        }
    }

private:
    static float _Width(bool collapsed)
    {
        return collapsed ? kHandleWidth : kHandleWidth + kModuleCount * kModuleWidth;
    }

    static BRect _Frame(bool collapsed)
    {
        BRect screen = BScreen().Frame();
        const float width = _Width(collapsed);
        // Finder's first collapsed tab begins at screen.bottom - 20.
        // Position the Control Strip just above that row with a 2px gap,
        // retaining the classic fixed-left edge and rightward expansion.
        const float left = screen.left + 5;
        const float tabTop = screen.bottom - 20;
        const float top = tabTop - 2 - kStripHeight;
        return BRect(left, top, left + width - 1,
            top + kStripHeight - 1);
    }

    StripView* fView;
    bool fCollapsed;
};

} // namespace

BWindow*
CreateOSTenControlStrip()
{
    return new ControlStripWindow();
}
