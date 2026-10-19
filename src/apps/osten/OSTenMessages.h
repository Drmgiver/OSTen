/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_MESSAGES_H
#define OSTEN_MESSAGES_H

#include <SupportDefs.h>


enum {
	kOSTenNewFolder = 'osnf',
	kOSTenOpen = 'osop',
	kOSTenClose = 'oscl',
	kOSTenSelectAll = 'ossa',
	kOSTenViewAsIcons = 'osvi',
	kOSTenViewAsList = 'osvl',
	kOSTenFinderViewModeChanged = 'osvm',
	kOSTenSystemFolderAccess = 'osfa',
	kOSTenRename = 'osrn',
	kOSTenGetInfo = 'osgi',
	kOSTenEmptyTrash = 'oset',
	kOSTenArrangeName = 'osan',
	kOSTenArrangeKind = 'osak',
	kOSTenCleanUp = 'oscu',
	kOSTenRefreshAll = 'osrf',
	kOSTenTogglePopupFolder = 'ospf'
};


#endif // OSTEN_MESSAGES_H
