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

// Shared filesystem operations, with Finder-level System Folder policy.
status_t RenameFinderEntry(const entry_ref& sourceRef, const char* newName,
    bool ownerUnlocked = false);
status_t EmptyFinderDirectory(BDirectory& directory);

status_t TransferEntry(const entry_ref& sourceRef, BDirectory& targetDirectory,
	const BPath& targetDirectoryPath, bool copy, bool preserveExisting,
	BString* recoveryPath = NULL, bool ownerUnlocked = false);

#endif
