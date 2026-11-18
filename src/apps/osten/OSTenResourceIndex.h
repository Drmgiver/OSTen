/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_RESOURCE_INDEX_H
#define OSTEN_RESOURCE_INDEX_H

// Public typed resource index. No Haiku headers or implementation linkage.
// File: OSTEN-RESOURCE-INDEX/1 followed by ID|TYPE|KEY|NAME|WIDTH|HEIGHT.
// IDs are stable across changes to labels, order, appearance and layout.
// Reader rejects the entire document on malformed or duplicate records,
// rather than silently giving the Owner an incomplete catalog.
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>

struct OSTenIndexedResource {
    uint32_t id;
    std::string type;
    std::string key;
    std::string name;
    int width;
    int height;
};

inline uint32_t
OSTenStableResourceID(const std::string& app, const std::string& key)
{
    uint32_t result = 2166136261u;
    const std::string input = app + ":" + key;
    for (size_t i = 0; i < input.size(); ++i) {
        result ^= (unsigned char)input[i];
        result *= 16777619u;
    }
    return result; // Compatibility fallback for an unindexed legacy catalog.
}

inline bool
OSTenReadResourceIndex(const std::string& path,
    std::map<std::string, OSTenIndexedResource>& index)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == NULL)
        return false;

    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL
        || strcmp(line, "OSTEN-RESOURCE-INDEX/1\n") != 0) {
        fclose(file);
        return false;
    }

    std::map<std::string, OSTenIndexedResource> items;
    std::map<uint32_t, std::string> assignedIDs;
    bool valid = true;
    while (fgets(line, sizeof(line), file) != NULL) {
        // Every row must fit, terminate with LF and contain exactly five
        // separators. A partial line is a fatal parse error, not a resource.
        const size_t length = strlen(line);
        if (length == 0 || line[length - 1] != '\n') {
            valid = false;
            break;
        }
        if (line[0] == '#' || line[0] == '\n')
            continue;
        const std::string row(line, length - 1);
        std::string field[6];
        size_t cursor = 0;
        for (int i = 0; i < 5; ++i) {
            const size_t separator = row.find('|', cursor);
            if (separator == std::string::npos) {
                valid = false;
                break;
            }
            field[i] = row.substr(cursor, separator - cursor);
            cursor = separator + 1;
        }
        if (!valid || row.find('|', cursor) != std::string::npos) {
            valid = false;
            break;
        }
        field[5] = row.substr(cursor);
        if (field[0].size() < 3 || field[0].compare(0, 2, "0x") != 0
            || field[1].size() < 2 || field[1].size() > 8
            || field[2].empty() || field[3].empty()) {
            valid = false;
            break;
        }
        for (size_t i = 0; i < field[1].size(); ++i) {
            const char c = field[1][i];
            if (!((c >= 'A' && c <= 'Z')
                    || (i > 0 && c >= '0' && c <= '9'))) {
                valid = false;
                break;
            }
        }
        if (!valid || field[2].find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                "0123456789._-") != std::string::npos) {
            valid = false;
            break;
        }
        // Display names are UTF-8 but may not embed newlines or
        // non-display control characters.
        for (size_t i = 0; i < field[3].size(); ++i) {
            if ((unsigned char)field[3][i] < 32) {
                valid = false;
                break;
            }
        }
        if (!valid) break;
        char *idEnd = NULL, *wEnd = NULL, *hEnd = NULL;
        errno = 0;
        const unsigned long id = strtoul(field[0].c_str(), &idEnd, 0);
        const bool idValid = errno == 0 && idEnd != field[0].c_str()
            && *idEnd == '\0' && id <= UINT32_MAX;
        errno = 0;
        const long w = strtol(field[4].c_str(), &wEnd, 10);
        const bool wValid = errno == 0 && wEnd != field[4].c_str()
            && *wEnd == '\0' && w >= 0 && w <= 2048;
        errno = 0;
        const long h = strtol(field[5].c_str(), &hEnd, 10);
        const bool hValid = errno == 0 && hEnd != field[5].c_str()
            && *hEnd == '\0' && h >= 0 && h <= 2048;
        if (!idValid || !wValid || !hValid
            || ((w != 0 || h != 0)
                && ((field[1] != "ICON" && field[1] != "IMAG")
                    || w == 0 || h == 0))
            || items.find(field[2]) != items.end()
            || assignedIDs.find((uint32_t)id) != assignedIDs.end()
            || items.size() >= 8192) {
            valid = false;
            break;
        }
        OSTenIndexedResource entry;
        entry.id = (uint32_t)id;
        entry.type = field[1];
        entry.key = field[2];
        entry.name = field[3];
        entry.width = (int)w;
        entry.height = (int)h;
        items[entry.key] = entry;
        assignedIDs[entry.id] = entry.key;
    }
    const bool readFailed = ferror(file) != 0;
    const bool closeFailed = fclose(file) != 0;
    if (readFailed || closeFailed)
        valid = false;
    if (!valid)
        return false; // Caller retains its previous complete index.
    index.swap(items);
    return true;
}

#endif
