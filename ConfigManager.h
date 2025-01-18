#pragma once
#include <windows.h>
#include <string>
#include <filesystem>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

class ConfigManager {
public:
    struct Config {
        // XP bar region
        RECT xpBarRegion = { 0, 0, 0, 0 };
        bool hasRegion = false;

        // Text display
        POINT textPosition = { 350, 350 };
    };

    ConfigManager() {
        // Get application data path
        wchar_t appDataPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
            m_configPath = std::filesystem::path(appDataPath) / L"XPBarTracker" / L"config.ini";
        }
    }

    bool SaveConfig(const Config& config) {
        // Ensure directory exists
        std::filesystem::create_directories(m_configPath.parent_path());

        // Save region data
        WritePrivateProfileString(L"Region", L"HasRegion",
            config.hasRegion ? L"1" : L"0", m_configPath.c_str());

        if (config.hasRegion) {
            WriteRegionToINI(L"Region", config.xpBarRegion);
        }

        // Save text position
        WritePointToINI(L"TextDisplay", L"Position", config.textPosition);

        return true;
    }

    // Save current application state
    void SaveCurrentState(bool hasSelectedRegion, const RECT& selectedRegion, const POINT& textPosition) {
        Config config;
        config.textPosition = textPosition;
        config.hasRegion = hasSelectedRegion;
        if (config.hasRegion) {
            config.xpBarRegion = selectedRegion;
        }
        SaveConfig(config);
    }

    Config LoadConfig() {
        Config config;

        // Load region data
        wchar_t buffer[16];
        GetPrivateProfileString(L"Region", L"HasRegion", L"0",
            buffer, sizeof(buffer) / sizeof(wchar_t), m_configPath.c_str());

        config.hasRegion = (_wtoi(buffer) != 0);
        if (config.hasRegion) {
            config.xpBarRegion = ReadRegionFromINI(L"Region");
        }

        // Load text position
        config.textPosition = ReadPointFromINI(L"TextDisplay", L"Position");

        return config;
    }

private:
    std::filesystem::path m_configPath;

    void WritePointToINI(const wchar_t* section, const wchar_t* key, const POINT& point) {
        wchar_t value[32];
        swprintf_s(value, L"%d,%d", point.x, point.y);
        WritePrivateProfileString(section, key, value, m_configPath.c_str());
    }

    POINT ReadPointFromINI(const wchar_t* section, const wchar_t* key) {
        POINT point = { 350, 350 }; // Default values
        wchar_t buffer[32];
        GetPrivateProfileString(section, key, L"350,350",
            buffer, sizeof(buffer) / sizeof(wchar_t), m_configPath.c_str());

        swscanf_s(buffer, L"%d,%d", &point.x, &point.y);
        return point;
    }

    void WriteRegionToINI(const wchar_t* section, const RECT& rect) {
        wchar_t value[64];
        swprintf_s(value, L"%d,%d,%d,%d",
            rect.left, rect.top, rect.right, rect.bottom);
        WritePrivateProfileString(section, L"Bounds", value, m_configPath.c_str());
    }

    RECT ReadRegionFromINI(const wchar_t* section) {
        RECT rect = { 0, 0, 0, 0 };
        wchar_t buffer[64];
        GetPrivateProfileString(section, L"Bounds", L"0,0,0,0",
            buffer, sizeof(buffer) / sizeof(wchar_t), m_configPath.c_str());

        swscanf_s(buffer, L"%d,%d,%d,%d",
            &rect.left, &rect.top, &rect.right, &rect.bottom);
        return rect;
    }
};