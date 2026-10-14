/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_FILE_OPERATIONS_H
#define OSTEN_FILE_OPERATIONS_H

#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <String.h>

// Finder policy only: protection is not yet enforced against other applications.
bool IsOSTenSystemFolderPath(const BPath& path);
bool MayTransferSystemFolderItem(const BPath& sourcePath,
	const BPath& destinationPath, bool copy, bool ownerUnlocked);

status_t TransferEntry(const entry_ref& sourceRef, BDirectory& targetDirectory,
	const BPath& targetDirectoryPath, bool copy, bool preserveExisting,
	BString* recoveryPath = NULL, bool ownerUnlocked = false);

#endif
