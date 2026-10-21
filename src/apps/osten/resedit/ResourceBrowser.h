/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_RESOURCE_BROWSER_H
#define OSTEN_RESOURCE_BROWSER_H

#include <string>

void StartOSTenResourceBrowser();
void OpenOSTenResourceProperty(const std::string& file,
    const std::string& application, const std::string& key);

#endif
