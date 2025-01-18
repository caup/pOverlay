#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <vector>
#include <memory>
#include <string>

#include "resource.h"
#include "WindowManager.h"
#include "CaptureSystem.h"
#include "FontManager.h"
#include "ConfigManager.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Error handling helper
void ShowError(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"Error", MB_ICONEXCLAMATION | MB_OK);
}

// Application state
struct AppState {
    bool isDrawing = false;
    bool isClickthrough = true;
    bool hasSelectedRegion = false;
    POINT startPoint = { 0, 0 };
    POINT endPoint = { 0, 0 };
    RECT selectedRegion = { 0, 0, 0, 0 };

    std::unique_ptr<FontManager> fontManager;
    std::unique_ptr<ConfigManager> configManager;

    // Text display members
    POINT textPosition = { 350, 350 };
    std::wstring xpText = L"0.00%";
    bool isDraggingText = false;
    POINT dragOffset = { 0, 0 };

	// Game window members
    std::optional<WindowManager::GameWindow> gameWindow;
    static constexpr UINT_PTR WINDOW_TRACK_TIMER = 1;
    static constexpr DWORD WINDOW_TRACK_INTERVAL = 500; // Check every 500ms

    // Add CaptureSystem
    std::unique_ptr<CaptureSystem> captureSystem;
};

// Global state
std::unique_ptr<AppState> g_state = std::make_unique<AppState>();

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Register for raw input (keyboard)
        RAWINPUTDEVICE rid = {};
        rid.usUsagePage = 0x01;
        rid.usUsage = 0x06;  // Keyboard
        rid.dwFlags = RIDEV_INPUTSINK;
        rid.hwndTarget = hwnd;

        if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
            ShowError(L"Failed to register raw input devices!");
            return -1;
        }
        return 0;
    }

    case WM_INPUT: {
        UINT size = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));

        std::vector<BYTE> buffer(size);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
            ShowError(L"Failed to get raw input data!");
            return 0;
        }

        RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            if (raw->data.keyboard.Message == WM_KEYDOWN) {
                if (raw->data.keyboard.VKey == VK_F7) {
                    g_state->isClickthrough = !g_state->isClickthrough;

                    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                    if (g_state->isClickthrough) {
                        exStyle |= WS_EX_TRANSPARENT;
                        // Keep text visible but make background fully transparent
                        SetLayeredWindowAttributes(hwnd, RGB(128, 128, 128), 0, LWA_COLORKEY);
                    }
                    else {
                        exStyle &= ~WS_EX_TRANSPARENT;
                        // Semi-transparent background for setup mode
                        SetLayeredWindowAttributes(hwnd, 0, 100, LWA_ALPHA);
                    }
                    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
        }
        return 0;
    }

    case WM_USER_XP_UPDATE: {
        // Update XP text
        wchar_t* newText = reinterpret_cast<wchar_t*>(lParam);
        g_state->xpText = newText;
        free(newText);  // Free the duplicated string
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!g_state->isClickthrough) {
            // Check if click is within text bounds
            RECT textRect;
            GetClientRect(hwnd, &textRect);
            HDC hdc = GetDC(hwnd);

            // Try Crimson Text first, fall back to Times New Roman
            HFONT font = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Crimson Text");

            if (!font) {
                font = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Times New Roman");
            }

            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            DrawText(hdc, g_state->xpText.c_str(), -1, &textRect,
                DT_CALCRECT | DT_SINGLELINE);
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            ReleaseDC(hwnd, hdc);

            textRect.left += g_state->textPosition.x;
            textRect.right += g_state->textPosition.x;
            textRect.top += g_state->textPosition.y;
            textRect.bottom += g_state->textPosition.y;

            POINT clickPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&textRect, clickPoint)) {
                g_state->isDraggingText = true;
                g_state->dragOffset.x = clickPoint.x - g_state->textPosition.x;
                g_state->dragOffset.y = clickPoint.y - g_state->textPosition.y;
                return 0;
            }

            // Start new drawing regardless of existing region
            g_state->isDrawing = true;
            g_state->startPoint.x = GET_X_LPARAM(lParam);
            g_state->startPoint.y = GET_Y_LPARAM(lParam);
            g_state->endPoint = g_state->startPoint;
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (g_state->isDraggingText) {
            g_state->textPosition.x = GET_X_LPARAM(lParam) - g_state->dragOffset.x;
            g_state->textPosition.y = GET_Y_LPARAM(lParam) - g_state->dragOffset.y;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        if (g_state->isDrawing) {
            g_state->endPoint.x = GET_X_LPARAM(lParam);
            g_state->endPoint.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_state->isDraggingText) {
            g_state->isDraggingText = false;
            g_state->configManager->SaveCurrentState(
                g_state->hasSelectedRegion,
                g_state->selectedRegion,
                g_state->textPosition
            );
            return 0;
        }

        if (g_state->isDrawing) {
            g_state->isDrawing = false;
            ReleaseCapture();

            RECT rect;
            rect.left = min(g_state->startPoint.x, g_state->endPoint.x);
            rect.top = min(g_state->startPoint.y, g_state->endPoint.y);
            rect.right = max(g_state->startPoint.x, g_state->endPoint.x);
            rect.bottom = max(g_state->startPoint.y, g_state->endPoint.y);

            // Only set region if it has size
            if (rect.right - rect.left > 0 && rect.bottom - rect.top > 0) {
                g_state->selectedRegion = rect;
                g_state->hasSelectedRegion = true;

                // Stop any existing capture before starting a new one
                if (g_state->captureSystem) {
                    g_state->captureSystem->StopCapture();
                }
                else {
                    g_state->captureSystem = std::make_unique<CaptureSystem>();
                    if (!g_state->captureSystem->Initialize(hwnd)) {
                        ShowError(L"Failed to initialize capture system!");
                        g_state->captureSystem.reset();
                        return 0;
                    }
                }

                // Start capture with new region
                if (!g_state->captureSystem->StartCapture(rect)) {
                    ShowError(L"Failed to start capture!");
                    g_state->captureSystem.reset();
                    g_state->hasSelectedRegion = false;
                }
                else {
                    g_state->configManager->SaveCurrentState(
                        g_state->hasSelectedRegion,
                        g_state->selectedRegion,
                        g_state->textPosition
                    );
                }

                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // Get client area size
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        // Create memory DC for double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
        // Fill background with the color we're using as transparent
        HBRUSH bgBrush = CreateSolidBrush(RGB(128, 128, 128));
        FillRect(memDC, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        // Only show rectangles when not in click-through mode
        if (!g_state->isClickthrough) {
            // Draw selected region if exists
            if (g_state->hasSelectedRegion) {
                HBRUSH brush = CreateSolidBrush(RGB(0, 255, 0));  // Green for selected region
                FrameRect(memDC, &g_state->selectedRegion, brush);
                DeleteObject(brush);
            }
            // Draw current rectangle if drawing
            if (g_state->isDrawing) {
                RECT currentRect;
                currentRect.left = min(g_state->startPoint.x, g_state->endPoint.x);
                currentRect.top = min(g_state->startPoint.y, g_state->endPoint.y);
                currentRect.right = max(g_state->startPoint.x, g_state->endPoint.x);
                currentRect.bottom = max(g_state->startPoint.y, g_state->endPoint.y);
                HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));  // Red for drawing
                FrameRect(memDC, &currentRect, brush);
                DeleteObject(brush);
            }
        }

        // Draw XP text if:
        // 1. We're in setup mode (not click-through), OR
        // 2. We have a selected region AND the game window is focused
        bool shouldDrawText = !g_state->isClickthrough ||
            (g_state->hasSelectedRegion && g_state->gameWindow &&
                GetForegroundWindow() == g_state->gameWindow->handle);
        if (shouldDrawText) {
            // Create and setup font - try Crimson Text first, fall back to Times New Roman
            HFONT font = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Crimson Text");

            if (!font) {
                font = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Times New Roman");
            }
            HFONT oldFont = (HFONT)SelectObject(memDC, font);

            // Setup text color and mode
            SetTextColor(memDC, RGB(255, 255, 255));  // White text
            SetBkMode(memDC, TRANSPARENT);

            // Draw text with outline
            SetTextColor(memDC, RGB(0, 0, 0));  // Black outline
            for (int offsetX = -1; offsetX <= 1; offsetX++) {
                for (int offsetY = -1; offsetY <= 1; offsetY++) {
                    if (offsetX == 0 && offsetY == 0) continue;
                    TextOut(memDC,
                        g_state->textPosition.x + offsetX,
                        g_state->textPosition.y + offsetY,
                        g_state->xpText.c_str(),
                        g_state->xpText.length());
                }
            }

            // Draw main text
            SetTextColor(memDC, RGB(255, 255, 255));  // White text
            TextOut(memDC,
                g_state->textPosition.x,
                g_state->textPosition.y,
                g_state->xpText.c_str(),
                g_state->xpText.length());

            // Cleanup font
            SelectObject(memDC, oldFont);
            DeleteObject(font);
        }

        // Copy memory DC to window
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

        // Cleanup
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER: {
        if (wParam == AppState::WINDOW_TRACK_TIMER && g_state->gameWindow) {
            // Update overlay position to match game window
            if (!WindowManager::RefreshOverlayPosition(hwnd, g_state->gameWindow.value())) {
                // Game window was closed or minimized
                ShowError(L"Lost connection to Pantheon window!");
                DestroyWindow(hwnd);
            }
        }
        return 0;
    }

    case WM_DESTROY: {
        g_state->configManager->SaveCurrentState(
            g_state->hasSelectedRegion,
            g_state->selectedRegion,
            g_state->textPosition
        );
        KillTimer(hwnd, AppState::WINDOW_TRACK_TIMER);
        if (g_state->captureSystem) {
            g_state->captureSystem->StopCapture();
        }
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

bool RegisterOverlayClass(HINSTANCE hInstance) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wc.lpszClassName = L"OverlayWindow";
    wc.hbrBackground = nullptr;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        ShowError(L"Failed to register window class!");
        return false;
    }
    return true;
}

HWND CreateOverlayWindow(HINSTANCE hInstance, const RECT& bounds) {
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        L"OverlayWindow",
        L"Game Overlay",
        WS_POPUP,
        bounds.left, bounds.top,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr, nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        ShowError(L"Failed to create overlay window!");
        return nullptr;
    }

    // Start with color keying for the background
    SetLayeredWindowAttributes(hwnd, RGB(128, 128, 128), 0, LWA_COLORKEY);

    return hwnd;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!RegisterOverlayClass(hInstance)) {
        return 1;
    }

    // Initialize ConfigManager before other systems
    g_state->configManager = std::make_unique<ConfigManager>();
    auto config = g_state->configManager->LoadConfig();

    // Always start in click-through mode
    g_state->isClickthrough = true;

    // Apply loaded configuration
    g_state->textPosition = config.textPosition;

    // Initialize FontManager and load Crimson Text font
    g_state->fontManager = std::make_unique<FontManager>();
    if (!g_state->fontManager->LoadFontFromResource(hInstance, IDR_FONT_CRIMSONTEXT)) {
        ShowError(L"Failed to load Crimson Text font!");
        return 1;
    }

    // Find Pantheon window
    auto gameWindow = WindowManager::FindPantheonWindow();
    if (!gameWindow) {
        ShowError(L"Pantheon window not found!");
        return 1;
    }

    g_state->gameWindow = gameWindow;

    // Create overlay sized to match game window
    HWND hwnd = CreateOverlayWindow(hInstance, gameWindow->bounds);
    if (!hwnd) {
        return 1;
    }

    // Initialize capture system if we have a saved region
    if (config.hasRegion) {
        g_state->selectedRegion = config.xpBarRegion;
        g_state->hasSelectedRegion = true;

        g_state->captureSystem = std::make_unique<CaptureSystem>();
        if (g_state->captureSystem->Initialize(hwnd)) {
            if (!g_state->captureSystem->StartCapture(config.xpBarRegion)) {
                g_state->captureSystem.reset();
                g_state->hasSelectedRegion = false;
            }
        }
    }

    // Set timer to track window position
    SetTimer(hwnd, AppState::WINDOW_TRACK_TIMER, AppState::WINDOW_TRACK_INTERVAL, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}