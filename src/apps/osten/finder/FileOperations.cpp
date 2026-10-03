/* SPDX-License-Identifier: MIT */

#include "FileOperations.h"

#include <CopyEngine.h>
#include <RemoveEngine.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


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
	BString* recoveryPath)
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
