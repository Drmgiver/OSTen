/* SPDX-License-Identifier: MIT */
// Compile this with a normal host C++ compiler: the index is not Haiku-only.
#include "OSTenResourceIndex.h"
#include <assert.h>
#include <stdio.h>
#include <map>
#include <string>

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
    // A namespace/key has a deterministic ID when there is no explicit index.
    assert(OSTenStableResourceID("Finder", "dialog.rename.title")
        == OSTenStableResourceID("Finder", "dialog.rename.title"));
    assert(OSTenStableResourceID("Finder", "dialog.rename.title")
        != OSTenStableResourceID("System", "dialog.rename.title"));
    puts("Portable resource-index reader: PASS");
    return 0;
}
