/* SPDX-License-Identifier: MIT */
// Compile with a host C++ compiler: the index format is portable.
#include "OSTenResourceIndex.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <string>

static void
CheckDocument(const std::string& body, bool expectSuccess)
{
    char temporary[] = "/tmp/osten-index-XXXXXX";
    const int fd = mkstemp(temporary);
    assert(fd >= 0);
    FILE* file = fdopen(fd, "wb");
    assert(file != NULL);
    assert(fwrite(body.data(), 1, body.size(), file) == body.size());
    assert(fclose(file) == 0);

    // A failure must leave a previously loaded catalog intact.
    std::map<std::string, OSTenIndexedResource> entries;
    OSTenIndexedResource sentinel;
    sentinel.id = 55;
    sentinel.type = "TEXT";
    sentinel.key = "existing";
    sentinel.name = "Existing";
    sentinel.width = 0;
    sentinel.height = 0;
    entries[sentinel.key] = sentinel;
    const bool ok = OSTenReadResourceIndex(temporary, entries);
    unlink(temporary);
    assert(ok == expectSuccess);
    if (!ok) {
        assert(entries.size() == 1);
        assert(entries["existing"].id == 55);
    } else {
        assert(entries.find("existing") == entries.end());
        assert(entries.size() == 2);
        assert(entries["one"].id == 1);
        assert(entries["two"].id == 2);
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::map<std::string, OSTenIndexedResource> found;
    assert(OSTenReadResourceIndex(argv[1], found));
    assert(!found.empty());
    for (std::map<std::string, OSTenIndexedResource>::const_iterator it
            = found.begin(); it != found.end(); ++it) {
        assert(it->second.key == it->first);
        assert(it->second.id != 0);
        assert(!it->second.type.empty());
    }
    assert(OSTenStableResourceID("Finder", "dialog.rename.title")
        == OSTenStableResourceID("Finder", "dialog.rename.title"));
    assert(OSTenStableResourceID("Finder", "dialog.rename.title")
        != OSTenStableResourceID("System", "dialog.rename.title"));

    const std::string header = "OSTEN-RESOURCE-INDEX/1\n";
    const std::string first = "0x00000001|TEXT|one|First label|0|0\n";
    const std::string second = "0x00000002|ICON|two|Second label|48|48\n";
    CheckDocument(header + first + second, true);
    CheckDocument(header + first + first, false); // repeated ID/key
    CheckDocument(header + first
        + "0x00000001|TEXT|two|Second|0|0\n", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|one|Second|0|0\n", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|two|Second|5|0\n", false);
    CheckDocument(header + first
        + "0x00000002|INVALIDTYPE|two|Second|0|0\n", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|invalid key|Second|0|0\n", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|two|Second|0|0|unexpected\n", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|two|Second|0|0", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|two|Second|0|99999\n", false);
    CheckDocument(header + first
        + "0x00000002|TEXT|two||0|0\n", false);
    CheckDocument("INVALID-HEADER\n" + first + second, false);
    puts("Portable resource-index reader and rejection tests: PASS");
    return 0;
}
