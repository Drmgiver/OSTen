/* SPDX-License-Identifier: MIT */
#ifndef OSTEN_RESOURCE_CATALOG_H
#define OSTEN_RESOURCE_CATALOG_H

// OSTen editable UI resource layer, version 1.
// Source code supplies safe defaults. Ordinary, Owner-editable files in the
// visible System Folder override those defaults without recompiling an app.
// Values are loaded once per process; restart the application to reload them.
// The future graphical Resource Editor must use these same stable keys.
#include <Bitmap.h>
#include <Font.h>
#include <GraphicsDefs.h>

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

class OSTenResourceCatalog {
public:
    explicit OSTenResourceCatalog(const char* application)
    {
        fDirectory = "/boot/System Folder/Resources/";
        fDirectory += application;
        fDirectory += "/";
        std::string manifest = fDirectory + "Interface.ostr";
        FILE* file = fopen(manifest.c_str(), "rb");
        if (file == NULL)
            return;

        char line[8192];
        while (fgets(line, sizeof(line), file) != NULL) {
            std::string entry(line);
            if (entry.empty() || entry[entry.size() - 1] != '\n') {
                if (!feof(file)) {
                    int ch;
                    while ((ch = fgetc(file)) != '\n' && ch != EOF) {}
                }
                continue;
            }
            if (!entry.empty() && entry[entry.size() - 1] == '\n')
                entry.resize(entry.size() - 1);
            if (!entry.empty() && entry[entry.size() - 1] == '\r')
                entry.resize(entry.size() - 1);
            if (entry.empty() || entry[0] == '#' || entry[0] == ';')
                continue;
            size_t equals = entry.find('=');
            if (equals == std::string::npos || equals == 0)
                continue;
            std::string key = entry.substr(0, equals);
            std::string value = entry.substr(equals + 1);
            if (key.find_first_not_of(
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-")
                != std::string::npos)
                continue;
            // Escaped newlines/tabs keep multi-line UI strings editable
            // in a single-line .ostr manifest without executable logic.
            if (fValues.size() < 2048)
                fValues[key] = _Unescape(value);
        }
        fclose(file);
    }

    const char* String(const char* key, const char* fallback) const
    {
        std::map<std::string, std::string>::const_iterator it
            = fValues.find(key);
        return it != fValues.end() ? it->second.c_str() : fallback;
    }

    float Number(const char* key, float fallback,
        float minimum, float maximum) const
    {
        std::map<std::string, std::string>::const_iterator it
            = fValues.find(key);
        if (it == fValues.end())
            return fallback;
        char* end = NULL;
        errno = 0;
        double parsed = strtod(it->second.c_str(), &end);
        if (errno != 0 || end == it->second.c_str() || *end != '\0'
            || !isfinite(parsed) || parsed < minimum || parsed > maximum)
            return fallback;
        return (float)parsed;
    }

    BPoint Point(const char* key, BPoint fallback) const
    {
        std::map<std::string, std::string>::const_iterator it
            = fValues.find(key);
        if (it == fValues.end())
            return fallback;
        float x, y;
        char tail = 0;
        if (sscanf(it->second.c_str(), "%f,%f%c", &x, &y, &tail) != 2
            || !isfinite(x) || !isfinite(y)
            || x < -8192 || x > 8192 || y < -8192 || y > 8192)
            return fallback;
        return BPoint(x, y);
    }

    BRect Rect(const char* key, BRect fallback) const
    {
        std::map<std::string, std::string>::const_iterator it
            = fValues.find(key);
        if (it == fValues.end())
            return fallback;
        float left, top, right, bottom;
        char tail = 0;
        if (sscanf(it->second.c_str(), "%f,%f,%f,%f%c",
                &left, &top, &right, &bottom, &tail) != 4
            || !isfinite(left) || !isfinite(top) || !isfinite(right)
            || !isfinite(bottom) || left < -8192 || top < -8192
            || right > 8192 || bottom > 8192
            || right <= left || bottom <= top)
            return fallback;
        return BRect(left, top, right, bottom);
    }

    rgb_color Color(const char* key, rgb_color fallback) const
    {
        std::map<std::string, std::string>::const_iterator it
            = fValues.find(key);
        if (it == fValues.end())
            return fallback;
        unsigned red, green, blue;
        char tail = 0;
        if (sscanf(it->second.c_str(), "%u,%u,%u%c",
                &red, &green, &blue, &tail) != 3
            || red > 255 || green > 255 || blue > 255)
            return fallback;
        return rgb_color{(uint8)red, (uint8)green, (uint8)blue, 255};
    }

    // Font names and sizes are resource data too. The fallback is a native
    // Haiku font; invalid or unavailable typefaces retain that fallback.
    BFont Font(const char* prefix, const BFont* fallback) const
    {
        BFont font(fallback);
        std::string stem(prefix);
        std::string familyKey = stem + ".family";
        std::string styleKey = stem + ".style";
        std::string sizeKey = stem + ".size";
        const char* family = String(familyKey.c_str(), "");
        const char* style = String(styleKey.c_str(), "");
        if (family[0] != 0 && style[0] != 0)
            font.SetFamilyAndStyle(family, style);
        font.SetSize(Number(sizeKey.c_str(), font.Size(), 6, 48));
        return font;
    }

    // 32-bit BGRA bitmap decoded from raw RGBA pixels (top row first).
    // Exact dimensions and file length are required; bad overrides are ignored.
    // The resource editor may replace these files, but they are never executed.
    BBitmap* Bitmap(const char* name, int width, int height) const
    {
        if (name == NULL || strchr(name, '/') || strstr(name, "..")
            || width <= 0 || height <= 0 || width > 2048 || height > 2048)
            return NULL;
        std::string path = fDirectory + name + ".rgba";
        FILE* file = fopen(path.c_str(), "rb");
        if (file == NULL)
            return NULL;
        size_t bytes = (size_t)width * height * 4;
        std::vector<unsigned char> rgba(bytes);
        bool valid = fread(&rgba[0], 1, bytes, file) == bytes
            && fgetc(file) == EOF;
        fclose(file);
        if (!valid)
            return NULL;

        BBitmap* bitmap = new BBitmap(
            BRect(0, 0, width - 1, height - 1), B_RGBA32);
        if (bitmap->InitCheck() != B_OK) {
            delete bitmap;
            return NULL;
        }
        unsigned char* target = (unsigned char*)bitmap->Bits();
        int stride = bitmap->BytesPerRow();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const unsigned char* p = &rgba[((size_t)y * width + x) * 4];
                unsigned char* out = target + y * stride + x * 4;
                out[0] = p[2];
                out[1] = p[1];
                out[2] = p[0];
                out[3] = p[3];
            }
        }
        return bitmap;
    }

private:
    static std::string _Unescape(const std::string& text)
    {
        std::string output;
        output.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] != '\\' || i + 1 >= text.size()) {
                output += text[i];
                continue;
            }
            char next = text[i + 1];
            if (next == 'n') {
                output += '\n';
                ++i;
            } else if (next == 't') {
                output += '\t';
                ++i;
            } else if (next == '\\') {
                output += '\\';
                ++i;
            } else {
                output += text[i]; // Unknown escapes remain literal.
            }
        }
        return output;
    }

    std::string fDirectory;
    std::map<std::string, std::string> fValues;
};

inline const OSTenResourceCatalog&
OSTenSystemResources()
{
    static OSTenResourceCatalog resources("System");
    return resources;
}

inline const OSTenResourceCatalog&
OSTenFinderResources()
{
    static OSTenResourceCatalog resources("Finder");
    return resources;
}

#endif
