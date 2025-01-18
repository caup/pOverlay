#define WM_USER_XP_UPDATE (WM_USER + 1)

#pragma once
#include <windows.h>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

class CaptureSystem {
public:
    CaptureSystem();
    ~CaptureSystem();

    // Initialize capture system
    bool Initialize(HWND overlayWindow);

    // Start/Stop capture
    bool StartCapture(const RECT& region);
    void StopCapture();

    // Process one frame and return XP percentage (0-100)
    float ProcessFrame();

private:
    // Capture thread function
    void CaptureThread();

    // Helper functions
    bool SetupCaptureDC();
    void CleanupCaptureDC();
    float AnalyzeRegion();
    bool IsFilledPixel(const RGBQUAD& pixel);
    bool IsMarkerPixel(const RGBQUAD& pixel);
    bool IsFilledMarkerPixel(const RGBQUAD& pixel);
    bool IsBackgroundPixel(const RGBQUAD& pixel);
    bool IsVerticalBarSequence(int x, int y);



    // Members
    HWND m_overlayWindow;
    RECT m_captureRegion;

    // GDI resources
    HDC m_screenDC;
    HDC m_memoryDC;
    HBITMAP m_captureBitmap;
    BYTE* m_bitmapData;

    // Thread control
    std::atomic<bool> m_isCapturing;
    std::unique_ptr<std::thread> m_captureThread;

    // Timing control
    static constexpr int CAPTURE_FPS = 4; // 4 frames per second
    static constexpr auto FRAME_DURATION = std::chrono::milliseconds(1000 / CAPTURE_FPS);
};