/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_SYSTEM_EXTRAS_H
#define OSTEN_SYSTEM_EXTRAS_H

#include <Rect.h>
#include <String.h>
#include <SupportDefs.h>
#include <time.h>
#include <vector>

class BMenu;
class BMessenger;
class BView;
class BWindow;

const uint32 kOSTenNextApplication = 'osnx';
const uint32 kOSTenActivateApplication = 'osat';
const uint32 kOSTenShowNotices = 'osnc';
const uint32 kOSTenClearNotices = 'oscl';
const uint32 kOSTenTestNotice = 'ostn';
const uint32 kOSTenNewNotice = 'nssm';

struct OSTenNotice {
    BString title;
    BString content;
    BString source;
    time_t when;
};

BView* CreateOSTenApplicationSwitcher(BRect frame);
void RefreshOSTenApplicationSwitcher(BView* view);
BView* CreateOSTenClockView(BRect frame);
BView* CreateOSTenNoticeButton(BRect frame);
void RefreshOSTenClock(BView* clock);
void SetOSTenUnreadCount(BView* button, int32 count);
BWindow* CreateOSTenNotificationCenter(const BMessenger& owner, BRect frame);
void UpdateOSTenNotifications(BWindow* center,
    const std::vector<OSTenNotice>& notices);

#endif
