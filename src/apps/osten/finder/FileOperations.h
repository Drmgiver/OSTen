/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_FILE_OPERATIONS_H
#define OSTEN_FILE_OPERATIONS_H

#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <String.h>

status_t TransferEntry(const entry_ref& sourceRef, BDirectory& targetDirectory,
	const BPath& targetDirectoryPath, bool copy, bool preserveExisting,
	BString* recoveryPath = NULL);

#endif
