#pragma once
#include <windows.h>
#include <string>
#include <optional>

class WindowManager {
public:
    struct GameWindow {
        HWND handle;
        RECT bounds;
    };

    static std::optional<GameWindow> FindPantheonWindow() {
        // Find window with exact title "Pantheon"
        HWND hwnd = FindWindowW(nullptr, L"Pantheon");
        if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
            return std::nullopt;
        }

        RECT bounds;
        GetWindowRect(hwnd, &bounds);

        // Verify window isn't minimized
        if (bounds.left <= -32000 || bounds.top <= -32000) {
            return std::nullopt;
        }

        return GameWindow{ hwnd, bounds };
    }

    static std::optional<RECT> GetGameWindowBounds(const GameWindow& gameWindow) {
        // Verify window still exists and is valid
        if (!IsWindow(gameWindow.handle)) {
            return std::nullopt;
        }

        RECT bounds;
        GetWindowRect(gameWindow.handle, &bounds);

        // Check if window is minimized
        if (bounds.left <= -32000 || bounds.top <= -32000) {
            return std::nullopt;
        }

        return bounds;
    }

    static void UpdateOverlayPosition(HWND overlayWindow, const RECT& bounds) {
        // Update overlay window position and size to match game window
        SetWindowPos(overlayWindow, HWND_TOPMOST,
            bounds.left, bounds.top,
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // Call this periodically to keep overlay aligned with game window
    static bool RefreshOverlayPosition(HWND overlayWindow, GameWindow& gameWindow) {
        if (auto bounds = GetGameWindowBounds(gameWindow)) {
            UpdateOverlayPosition(overlayWindow, bounds.value());
            gameWindow.bounds = bounds.value();
            return true;
        }
        return false;
    }
};