/* SPDX-License-Identifier: MIT */

#include "FileOperations.h"

#include <CopyEngine.h>
#include <RemoveEngine.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <vector>


static bool
PathContains(const BPath& directory, const BPath& entry)
{
	const char* directoryPath = directory.Path();
	const char* entryPath = entry.Path();
	size_t length = strlen(directoryPath);
	return strncmp(directoryPath, entryPath, length) == 0
		&& (length == 1 || entryPath[length] == '\0'
			|| entryPath[length] == '/');
}


bool
IsOSTenSystemFolderPath(const BPath& path)
{
	if (path.InitCheck() != B_OK)
		return false;
	const char* root = "/boot/System Folder";
	const size_t length = strlen(root);
	const char* value = path.Path();
	return strncmp(value, root, length) == 0
		&& (value[length] == '\0' || value[length] == '/');
}


bool
MayTransferSystemFolderItem(const BPath& sourcePath,
	const BPath& destinationPath, bool copy, bool ownerUnlocked)
{
	// Reading or copying a protected item out is permitted. Moving it
	// changes the source, and any write into the folder is protected.
	return ownerUnlocked || (!IsOSTenSystemFolderPath(destinationPath)
		&& (copy || !IsOSTenSystemFolderPath(sourcePath)));
}


status_t
RenameFinderEntry(const entry_ref& sourceRef, const char* newName,
    bool ownerUnlocked)
{
    if (newName == NULL || newName[0] == '\0'
        || strcmp(newName, ".") == 0 || strcmp(newName, "..") == 0
        || strchr(newName, '/') != NULL
        || strlen(newName) >= B_FILE_NAME_LENGTH)
        return B_BAD_VALUE;

    BEntry source(&sourceRef, false);
    if (source.InitCheck() != B_OK)
        return source.InitCheck();
    if (!source.Exists())
        return B_ENTRY_NOT_FOUND;

    char oldName[B_FILE_NAME_LENGTH];
    status_t error = source.GetName(oldName);
    if (error != B_OK)
        return error;
    if (strcmp(oldName, newName) == 0)
        return B_OK;

    BPath from;
    BEntry parent;
    error = source.GetPath(&from);
    if (error == B_OK)
        error = source.GetParent(&parent);
    BPath to;
    if (error == B_OK)
        error = parent.GetPath(&to);
    if (error == B_OK)
        error = to.Append(newName);
    if (error != B_OK)
        return error;
    if (!MayTransferSystemFolderItem(from, to, false, ownerUnlocked))
        return B_PERMISSION_DENIED;

    return source.Rename(newName, false);
}


status_t
EmptyFinderDirectory(BDirectory& directory)
{
    if (directory.InitCheck() != B_OK)
        return directory.InitCheck();
    // Snapshot refs before removal: deleting while iterating can skip entries.
    std::vector<entry_ref> refs;
    directory.Rewind();
    BEntry entry;
    while (directory.GetNextEntry(&entry, false) == B_OK) {
        entry_ref ref;
        if (entry.GetRef(&ref) == B_OK)
            refs.push_back(ref);
    }
    status_t firstError = B_OK;
    for (size_t i = 0; i < refs.size(); i++) {
        BEntry item(&refs[i], false);
        status_t error = BRemoveEngine().RemoveEntry(item);
        if (error != B_OK && firstError == B_OK)
            firstError = error;
    }
    return firstError;
}


static void
MakeUniqueName(BDirectory& directory, const char* original, BString& name)
{
	name = original;
	for (int32 suffix = 2; directory.Contains(name.String()); suffix++)
		name.SetToFormat("%s %ld", original, (long)suffix);
}


status_t
TransferEntry(const entry_ref& sourceRef, BDirectory& targetDirectory,
	const BPath& targetDirectoryPath, bool copy, bool preserveExisting,
	BString* recoveryPath, bool ownerUnlocked)
{
	if (recoveryPath != NULL)
		recoveryPath->Truncate(0);
	if (targetDirectoryPath.InitCheck() != B_OK)
		return targetDirectoryPath.InitCheck();
	BEntry source(&sourceRef, false);
	if (source.InitCheck() != B_OK)
		return source.InitCheck();
	BPath sourcePath;
	status_t error = source.GetPath(&sourcePath);
	if (error != B_OK)
		return error;
	if (!source.Exists())
		return B_ENTRY_NOT_FOUND;
	if (source.IsDirectory() && PathContains(sourcePath, targetDirectoryPath))
		return B_BAD_VALUE;

	char originalName[B_FILE_NAME_LENGTH];
	error = source.GetName(originalName);
	if (error != B_OK)
		return error;
	BEntry parent;
	error = source.GetParent(&parent);
	if (error != B_OK)
		return error;
	BEntry targetDirectoryEntry;
	error = targetDirectory.GetEntry(&targetDirectoryEntry);
	if (error != B_OK)
		return error;
	bool sameDirectory = parent == targetDirectoryEntry;
	if (sameDirectory && !copy)
		return B_OK;

	BString targetName(originalName);
	if (copy && sameDirectory) {
		BString copyName;
		copyName.SetToFormat("%s copy", originalName);
		MakeUniqueName(targetDirectory, copyName.String(), targetName);
	} else if (preserveExisting)
		MakeUniqueName(targetDirectory, originalName, targetName);

	BPath destinationPath(targetDirectoryPath);
	error = destinationPath.Append(targetName.String());
	if (error != B_OK)
		return error;
	if (!MayTransferSystemFolderItem(sourcePath, destinationPath, copy,
		ownerUnlocked)) {
		return B_PERMISSION_DENIED;
	}
	BEntry destination(destinationPath.Path(), false);
	if (destination.InitCheck() != B_OK)
		return destination.InitCheck();
	// Replacing an ancestor would destroy the source before it can be moved.
	if (destination.IsDirectory() && PathContains(destinationPath, sourcePath))
		return B_BAD_VALUE;

	// A move to an empty destination needs no copying or temporary folder.
	if (!copy && !destination.Exists()) {
		error = source.MoveTo(&targetDirectory, targetName.String(), false);
		if (error != B_CROSS_DEVICE_LINK)
			return error;
	}

	// Stage the complete copy on the destination volume. The previous item
	// stays untouched if reading, copying attributes, or writing fails.
	BPath stagingPath(targetDirectoryPath);
	error = stagingPath.Append("Finder transfer XXXXXX");
	if (error != B_OK)
		return error;
	char stagingName[B_PATH_NAME_LENGTH];
	strcpy(stagingName, stagingPath.Path());
	if (mkdtemp(stagingName) == NULL)
		return errno;
	BDirectory staging(stagingName);
	BEntry stagingEntry(stagingName);
	if (recoveryPath != NULL)
		*recoveryPath = stagingName;
	BPath payloadPath(stagingName, "new item");
	if (staging.InitCheck() != B_OK || payloadPath.InitCheck() != B_OK)
		return B_ERROR;

	node_ref parentNode;
	node_ref targetNode;
	bool sameVolume = parent.GetNodeRef(&parentNode) == B_OK
		&& targetDirectory.GetNodeRef(&targetNode) == B_OK
		&& parentNode.device == targetNode.device;
	bool stagedCopy = copy || !sameVolume;
	if (stagedCopy) {
		error = BCopyEngine(BCopyEngine::COPY_RECURSIVELY).CopyEntry(
			sourcePath.Path(), payloadPath.Path());
		if (error != B_OK) {
			if (BRemoveEngine().RemoveEntry(stagingEntry) == B_OK
				&& recoveryPath != NULL) {
				recoveryPath->Truncate(0);
			}
			return error;
		}
	}

	bool hadDestination = destination.Exists();
	if (hadDestination) {
		error = destination.MoveTo(&staging, "previous item", false);
		if (error != B_OK) {
			if (BRemoveEngine().RemoveEntry(stagingEntry) == B_OK
				&& recoveryPath != NULL) {
				recoveryPath->Truncate(0);
			}
			return error;
		}
	}
	BEntry payload(payloadPath.Path(), false);
	error = stagedCopy
		? payload.MoveTo(&targetDirectory, targetName.String(), false)
		: source.MoveTo(&targetDirectory, targetName.String(), false);
	if (error != B_OK) {
		// Never remove recovery files when the old item cannot be restored.
		if (hadDestination && destination.MoveTo(&targetDirectory,
			targetName.String(), false) != B_OK) {
			return error;
		}
		if (BRemoveEngine().RemoveEntry(stagingEntry) == B_OK
			&& recoveryPath != NULL) {
			recoveryPath->Truncate(0);
		}
		return error;
	}
	// Cross-volume moves remove the source only after the copy is published.
	if (!copy && stagedCopy) {
		error = BRemoveEngine().RemoveEntry(source);
		if (error != B_OK)
			return error;
	}
	error = BRemoveEngine().RemoveEntry(stagingEntry);
	if (error == B_OK && recoveryPath != NULL)
		recoveryPath->Truncate(0);
	return error;
}
