/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_DIALOG_LAYOUT_H
#define OSTEN_DIALOG_LAYOUT_H

// OSTen universal, resource-defined native UI controls, revision 1.
// The application supplies only command ID -> handler message bindings.
// Appearance, control type, ordering, caption, and geometry come from
// the application's public Interface.ostr resource catalog.
#include "OSTenResources.h"

#include <Button.h>
#include <CheckBox.h>
#include <Slider.h>
#include <ListView.h>
#include <Message.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <View.h>
#include <Window.h>

#include <map>
#include <sstream>
#include <string>

struct OSTenDialogAction {
    const char* id;
    uint32 message;
};

typedef std::map<std::string, BView*> OSTenDialogControls;

// Application-specific widgets can be registered without changing ResEdit
// or extending the OS's built-in control switch. The registered factory
// creates a native view. The Owner still controls its public resource keys.
typedef BView* (*OSTenDialogControlFactory)(BRect frame,
    const char* identifier, const char* caption, uint32 action,
    const OSTenResourceCatalog& catalog, const std::string& resourcePrefix,
    void* context);

typedef std::map<std::string, OSTenDialogControlFactory>
    OSTenDialogControlFactories;

inline uint32
OSTenDialogActionMessage(const char* action, const OSTenDialogAction* bindings,
    size_t count)
{
    if (action == NULL || bindings == NULL)
        return 0;
    for (size_t i = 0; i < count; ++i) {
        if (bindings[i].id != NULL && strcmp(bindings[i].id, action) == 0)
            return bindings[i].message;
    }
    return 0;
}

// Returns the created controls by stable resource ID. Does not impose
// undeclared menu actions or privilege escalation. The Owner can remove
// controls or change their geometry, even if that makes the UI unusable.
inline OSTenDialogControls
OSTenCreateDialogControls(BView* parent, const OSTenResourceCatalog& catalog,
    const char* dialogID, const OSTenDialogAction* bindings,
    size_t bindingCount,
    const OSTenDialogControlFactories* extensions = NULL,
    void* factoryContext = NULL)
{
    OSTenDialogControls result;
    if (parent == NULL || dialogID == NULL)
        return result;
    const std::string prefix(dialogID);
    const std::string orderKey = prefix + ".controls";
    std::istringstream order(catalog.String(orderKey.c_str(), ""));
    std::string id;
    while (std::getline(order, id, ',')) {
        if (id.empty() || id.find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                "0123456789._-") != std::string::npos
            || result.find(id) != result.end())
            continue;
        const std::string base = prefix + ".control." + id + ".";
        const char* type = catalog.String((base + "type").c_str(), "");
        const char* captionKey = catalog.String(
            (base + "caption_key").c_str(), "");
        const char* label = captionKey[0]
            ? catalog.String(captionKey, "") : "";
        const char* alias = catalog.String(
            (base + "frame_key").c_str(), "");
        const std::string frameKey = alias[0] ? alias : base + "frame";
        const BRect frame = catalog.Rect(frameKey.c_str(),
            BRect(0, 0, 80, 28));
        const char* action = catalog.String((base + "action").c_str(), "");
        const uint32 what = OSTenDialogActionMessage(
            action, bindings, bindingCount);
        BView* view = NULL;

        if (strcmp(type, "text") == 0) {
            BTextControl* field = new BTextControl(
                frame, id.c_str(), label, "", NULL);
            field->SetDivider(catalog.Number(
                (base + "divider").c_str(), 56, 0, 1024));
            view = field;
            parent->AddChild(view);
        } else if (strcmp(type, "button") == 0) {
            BButton* button = new BButton(frame, id.c_str(), label,
                what ? new BMessage(what) : NULL);
            view = button;
            parent->AddChild(view);
            if (catalog.Number((base + "default").c_str(), 0, 0, 1) > 0
                && parent->Window() != NULL)
                parent->Window()->SetDefaultButton(button);
        } else if (strcmp(type, "text_area") == 0) {
            BRect textFrame(4, 4, frame.Width() - 21,
                frame.Height() - 8);
            BTextView* text = new BTextView(frame, id.c_str(), textFrame,
                B_FOLLOW_ALL, B_WILL_DRAW | B_NAVIGABLE);
            text->MakeEditable(false);
            text->MakeSelectable(true);
            text->SetViewColor(catalog.Color(
                (base + "background").c_str(),
                rgb_color{255, 255, 255, 255}));
            view = text;
            parent->AddChild(new BScrollView(
                (id + "-scroll").c_str(), text,
                B_FOLLOW_ALL, 0, false, true));
        } else if (strcmp(type, "checkbox") == 0) {
            BCheckBox* checkbox = new BCheckBox(frame, id.c_str(), label,
                what ? new BMessage(what) : NULL);
            checkbox->SetValue(catalog.Number(
                (base + "checked").c_str(), 0, 0, 1) >= 0.5f
                ? B_CONTROL_ON : B_CONTROL_OFF);
            view = checkbox;
            parent->AddChild(view);
        } else if (strcmp(type, "slider") == 0) {
            int32 minimum = (int32)catalog.Number(
                (base + "minimum").c_str(), 0, -32768, 32767);
            int32 maximum = (int32)catalog.Number(
                (base + "maximum").c_str(), 100, -32768, 32767);
            if (maximum <= minimum)
                maximum = minimum + 1;
            BSlider* slider = new BSlider(frame, id.c_str(), label,
                what ? new BMessage(what) : NULL,
                minimum, maximum, B_HORIZONTAL);
            slider->SetValue((int32)catalog.Number(
                (base + "value").c_str(), minimum, minimum, maximum));
            if (catalog.Number((base + "hash_marks").c_str(),
                    0, 0, 1) >= 0.5f) {
                slider->SetHashMarks(B_HASH_MARKS_BOTTOM);
                slider->SetHashMarkCount((int32)catalog.Number(
                    (base + "hash_count").c_str(), 9, 1, 256));
            }
            view = slider;
            parent->AddChild(view);
        } else if (strcmp(type, "label") == 0) {
            view = new BStringView(frame, id.c_str(), label);
            parent->AddChild(view);
        } else if (strcmp(type, "list") == 0) {
            BListView* list = new BListView(frame, id.c_str(),
                B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL);
            if (what != 0)
                list->SetInvocationMessage(new BMessage(what));
            const rgb_color defaultColor = {255, 255, 255, 255};
            list->SetViewColor(catalog.Color(
                (base + "background").c_str(), defaultColor));
            view = list;
            // The scroll view is a layout implementation detail.
            parent->AddChild(new BScrollView(
                (id + "-scroll").c_str(), list,
                B_FOLLOW_ALL, 0, false, true));
        }
        if (view == NULL && extensions != NULL) {
            OSTenDialogControlFactories::const_iterator factory
                = extensions->find(type);
            if (factory != extensions->end() && factory->second != NULL) {
                view = factory->second(frame, id.c_str(), label, what,
                    catalog, base, factoryContext);
                if (view != NULL)
                    parent->AddChild(view);
            }
        }
        if (view != NULL)
            result[id] = view;
    }
    return result;
}

#endif
