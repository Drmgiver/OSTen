/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_GLOBAL_MENU_H
#define OSTEN_GLOBAL_MENU_H

// Public, resource-backed OSTen global menu protocol. Each native app owns
// its own command semantics; OSTen System owns presentation and activation.
// The menu reply contains only documented data: sections, labels, stable
// action IDs and one-key shortcuts. No executable code crosses the boundary.
#include "OSTenResources.h"

#include <Message.h>
#include <string>
#include <sstream>
#include <set>
#include <string.h>

static const uint32 kOSTenGlobalMenuQuery = 'ogmq';
static const uint32 kOSTenGlobalMenuInvoke = 'ogmi';

inline bool
OSTenIsMenuIdentifier(const std::string& value)
{
    return !value.empty() && value.size() <= 128
        && value.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
            "0123456789._-") == std::string::npos;
}

inline void
OSTenBuildGlobalMenuReply(const OSTenResourceCatalog& catalog,
    BMessage& reply)
{
    const char* sections = catalog.String("menu.order", "");
    std::istringstream order(sections);
    std::string section;
    std::set<std::string> usedSections;
    while (std::getline(order, section, ',')) {
        if (!OSTenIsMenuIdentifier(section)
            || !usedSections.insert(section).second)
            continue;
        const std::string prefix = "menu." + section;
        const std::string titleKey = "menu.title." + section;
        const char* title = catalog.String(titleKey.c_str(), section.c_str());
        const char* commands = catalog.String(prefix.c_str(), "");
        BMessage menu('ogms');
        menu.AddString("id", section.c_str());
        menu.AddString("title", title);
        std::istringstream items(commands);
        std::string action;
        std::set<std::string> usedActions;
        while (std::getline(items, action, ',')) {
            if (action == "separator") {
                BMessage item('ogit');
                item.AddBool("separator", true);
                menu.AddMessage("item", &item);
                continue;
            }
            if (!OSTenIsMenuIdentifier(action)
                || !usedActions.insert(action).second)
                continue;
            const std::string labelKey = "menu.item." + action;
            const std::string shortcutKey = "shortcut." + action;
            const char* shortcut = catalog.String(shortcutKey.c_str(), "");
            BMessage item('ogit');
            item.AddString("action", action.c_str());
            item.AddString("label", catalog.String(labelKey.c_str(),
                action.c_str()));
            if (strlen(shortcut) == 1)
                item.AddString("shortcut", shortcut);
            menu.AddMessage("item", &item);
        }
        reply.AddMessage("menu", &menu);
    }
}

// Call from a native BApplication::MessageReceived handler only on
// kOSTenGlobalMenuQuery. The app can publish any Owner-editable menus.
// An empty menu.order declares that the app has no custom global menus.
inline void
OSTenAnswerGlobalMenuQuery(BMessage* query,
    const OSTenResourceCatalog& resources)
{
    BMessage reply(B_REPLY);
    OSTenBuildGlobalMenuReply(resources, reply);
    query->SendReply(&reply);
}

#endif
