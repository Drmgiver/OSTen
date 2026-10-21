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
#include "OSTenResources.h"
#include <stdio.h>
#include <sstream>
#include <set>

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
static float _HandleWidth() {
    return OSTenSystemResources().Number("strip.handle.width", 23, 18, 60);
}
static float _ModuleWidth() {
    return OSTenSystemResources().Number("strip.module.width", 42, 28, 100);
}
static float _StripHeight() {
    return OSTenSystemResources().Number("layout.control_strip.height", 30, 24, 60);
}

// Resource editor may rearrange slots, but slot IDs never change their meaning.
// Invalid or duplicate slot lists revert to the approved original order.
static int _ModuleForSlot(int position)
{
    static int order[kModuleCount];
    static bool initialized = false;
    if (!initialized) {
        for (int i = 0; i < kModuleCount; ++i)
            order[i] = i;
        const char* raw = OSTenSystemResources().String(
            "strip.module.order", "0,1,2,3,4,5,6");
        std::stringstream source(raw);
        std::string token;
        std::set<int> used;
        int proposed[kModuleCount];
        int count = 0;
        while (std::getline(source, token, ',') && count < kModuleCount) {
            char* end = NULL;
            long number = strtol(token.c_str(), &end, 10);
            if (end == token.c_str() || *end != '\0'
                || number < 0 || number >= kModuleCount
                || !used.insert((int)number).second)
                break;
            proposed[count++] = (int)number;
        }
        if (count == kModuleCount && source.eof())
            for (int i = 0; i < kModuleCount; ++i)
                order[i] = proposed[i];
        initialized = true;
    }
    return order[position];
}

static const char* _ModuleText(int index, const char* suffix,
    const char* fallback)
{
    char key[64];
    snprintf(key, sizeof(key), "strip.module.%d.%s", index, suffix);
    return OSTenSystemResources().String(key, fallback);
}


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
            const float left = _HandleWidth() + i * _ModuleWidth();
            SetHighColor(130, 130, 134);
            StrokeLine(BPoint(left, 4), BPoint(left, bounds.bottom - 4));
            SetHighColor(250, 250, 250);
            StrokeLine(BPoint(left + 1, 4),
                BPoint(left + 1, bounds.bottom - 4));
            SetHighColor(20, 20, 25);
            const int module = _ModuleForSlot(i);
            const char* label = _ModuleText(module, "label",
                kModules[module].shortLabel);
            const float width = StringWidth(label);
            DrawString(label, BPoint(left + (_ModuleWidth() - width) / 2, 20));
        }
    }

    void MouseDown(BPoint point) override
    {
        if (point.x < _HandleWidth()) {
            Window()->PostMessage(kToggleStrip);
            return;
        }
        if (fCollapsed)
            return;
        int index = (int)((point.x - _HandleWidth()) / _ModuleWidth());
        if (index < 0 || index >= kModuleCount)
            return;
        index = _ModuleForSlot(index);
        const char* name = _ModuleText(index, "name", kModules[index].name);
        BPopUpMenu* menu = new BPopUpMenu(name, false, false);
        BMenuItem* title = new BMenuItem(name, NULL);
        title->SetEnabled(false);
        menu->AddItem(title);
        menu->AddSeparatorItem();
        BMenuItem* info = new BMenuItem(_ModuleText(index,
            "summary", kModules[index].summary), NULL);
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
        : BWindow(_Frame(true),
            OSTenSystemResources().String("strip.title", "Control Strip"),
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
                ResizeTo(_Width(fCollapsed), _StripHeight() - 1);
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
        return collapsed ? _HandleWidth() : _HandleWidth() + kModuleCount * _ModuleWidth();
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
        const float top = tabTop - OSTenSystemResources().Number(
            "layout.control_strip.gap", 2, 0, 20) - _StripHeight();
        return BRect(left, top, left + width - 1,
            top + _StripHeight() - 1);
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
