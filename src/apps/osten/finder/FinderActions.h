/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_FINDER_ACTIONS_H
#define OSTEN_FINDER_ACTIONS_H

#include <Entry.h>

void ShowFinderInfo(const entry_ref& ref);
void ShowFinderRename(const entry_ref& ref, bool ownerUnlocked);
void EmptyFinderTrash();

#endif
