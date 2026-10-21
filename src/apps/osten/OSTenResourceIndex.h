/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_RESOURCE_INDEX_H
#define OSTEN_RESOURCE_INDEX_H

// Public typed resource index. No Haiku headers or implementation linkage.
// File: OSTEN-RESOURCE-INDEX/1 followed by ID|TYPE|KEY|NAME|WIDTH|HEIGHT.
// IDs are stable across changes to labels, order, appearance and layout.
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
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        std::string field[6];
        char* cursor = line;
        bool complete = true;
        for (int i = 0; i < 6; ++i) {
            char* next = i == 5
                ? strpbrk(cursor, "\r\n") : strchr(cursor, '|');
            if (next == NULL) { complete = false; break; }
            field[i].assign(cursor, next - cursor);
            cursor = next + 1;
        }
        if (!complete || field[1].empty() || field[2].empty()
            || field[2].find_first_not_of(
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                "0123456789._-") != std::string::npos)
            continue;
        char *idEnd = NULL, *wEnd = NULL, *hEnd = NULL;
        unsigned long id = strtoul(field[0].c_str(), &idEnd, 0);
        long w = strtol(field[4].c_str(), &wEnd, 10);
        long h = strtol(field[5].c_str(), &hEnd, 10);
        if (!idEnd || *idEnd != '\0' || idEnd == field[0].c_str()
            || id > UINT32_MAX
            || wEnd == field[4].c_str() || *wEnd != '\0'
            || hEnd == field[5].c_str() || *hEnd != '\0'
            || w < 0 || h < 0 || w > 2048 || h > 2048
            || items.find(field[2]) != items.end()
            || assignedIDs.find((uint32_t)id) != assignedIDs.end())
            continue;
        OSTenIndexedResource entry;
        entry.id = (uint32_t)id;
        entry.type = field[1];
        entry.key = field[2];
        entry.name = field[3].empty() ? field[2] : field[3];
        entry.width = (int)w;
        entry.height = (int)h;
        items[entry.key] = entry;
        assignedIDs[entry.id] = entry.key;
        if (items.size() >= 8192)
            break;
    }
    bool ok = !ferror(file);
    fclose(file);
    if (!ok)
        return false;
    index.swap(items);
    return true;
}

#endif
