#pragma once
#include <windows.h>
#include <string>
#include <vector>

class FontManager {
private:
    std::vector<HANDLE> loadedFonts;

public:
    ~FontManager() {
        // Clean up all loaded fonts
        for (HANDLE fontHandle : loadedFonts) {
            RemoveFontMemResourceEx(fontHandle);
        }
    }

    bool LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
        // Load the font resource
        HRSRC fontResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
        if (!fontResource) return false;

        HGLOBAL fontData = LoadResource(hInstance, fontResource);
        if (!fontData) return false;

        void* fontPtr = LockResource(fontData);
        DWORD fontSize = SizeofResource(hInstance, fontResource);

        // Add font to memory
        DWORD numFonts = 0;
        HANDLE fontHandle = AddFontMemResourceEx(
            fontPtr,
            fontSize,
            nullptr,
            &numFonts
        );

        if (fontHandle) {
            loadedFonts.push_back(fontHandle);
            return true;
        }

        return false;
    }
};