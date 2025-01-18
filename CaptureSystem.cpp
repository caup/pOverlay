#include "CaptureSystem.h"

CaptureSystem::CaptureSystem()
    : m_overlayWindow(nullptr)
    , m_screenDC(nullptr)
    , m_memoryDC(nullptr)
    , m_captureBitmap(nullptr)
    , m_bitmapData(nullptr)
    , m_isCapturing(false) {
}

CaptureSystem::~CaptureSystem() {
    StopCapture();
    CleanupCaptureDC();
}

bool CaptureSystem::Initialize(HWND overlayWindow) {
    m_overlayWindow = overlayWindow;
    return SetupCaptureDC();
}

bool CaptureSystem::SetupCaptureDC() {
    // Get screen DC
    m_screenDC = GetDC(nullptr);
    if (!m_screenDC) return false;

    // Create compatible DC
    m_memoryDC = CreateCompatibleDC(m_screenDC);
    if (!m_memoryDC) {
        ReleaseDC(nullptr, m_screenDC);
        m_screenDC = nullptr;
        return false;
    }

    return true;
}

void CaptureSystem::CleanupCaptureDC() {
    if (m_captureBitmap) {
        DeleteObject(m_captureBitmap);
        m_captureBitmap = nullptr;
    }

    if (m_memoryDC) {
        DeleteDC(m_memoryDC);
        m_memoryDC = nullptr;
    }

    if (m_screenDC) {
        ReleaseDC(nullptr, m_screenDC);
        m_screenDC = nullptr;
    }
}

bool CaptureSystem::StartCapture(const RECT& region) {
    if (m_isCapturing) return false;

    m_captureRegion = region;
    int width = region.right - region.left;
    int height = region.bottom - region.top;

    // Create bitmap for capture
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_captureBitmap = CreateDIBSection(m_memoryDC, &bmi, DIB_RGB_COLORS,
        reinterpret_cast<void**>(&m_bitmapData),
        nullptr, 0);
    if (!m_captureBitmap) return false;

    // Start capture thread
    m_isCapturing = true;
    m_captureThread = std::make_unique<std::thread>(&CaptureSystem::CaptureThread, this);

    return true;
}

void CaptureSystem::StopCapture() {
    if (!m_isCapturing) return;

    m_isCapturing = false;
    if (m_captureThread && m_captureThread->joinable()) {
        m_captureThread->join();
    }
}

void CaptureSystem::CaptureThread() {
    auto nextCapture = std::chrono::steady_clock::now();

    while (m_isCapturing) {
        // Process frame
        float xpPercentage = ProcessFrame();

        // TODO: Update UI with new XP percentage

        // Wait for next frame
        nextCapture += FRAME_DURATION;
        std::this_thread::sleep_until(nextCapture);
    }
}

float CaptureSystem::ProcessFrame() {
    // Select bitmap into DC
    HBITMAP oldBitmap = (HBITMAP)SelectObject(m_memoryDC, m_captureBitmap);

    // Capture screen region
    BitBlt(m_memoryDC, 0, 0,
        m_captureRegion.right - m_captureRegion.left,
        m_captureRegion.bottom - m_captureRegion.top,
        m_screenDC,
        m_captureRegion.left, m_captureRegion.top,
        SRCCOPY);

    // Analyze the captured region
    float result = AnalyzeRegion();

    // Convert to percentage string with 2 decimal places
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(2) << result << L"%";
    // Signal main window to update (we'll add this functionality)
    PostMessage(m_overlayWindow, WM_USER_XP_UPDATE, 0,
        reinterpret_cast<LPARAM>(_wcsdup(ss.str().c_str())));

    // Cleanup
    SelectObject(m_memoryDC, oldBitmap);

    return result;
}

bool CaptureSystem::IsFilledPixel(const RGBQUAD& pixel) {
    // Check for XP fill color (#2D67E2)
    const int fillRed = 0x2D;    // 45
    const int fillGreen = 0x67;  // 103
    const int fillBlue = 0xE2;   // 226

    // Allow some tolerance for slight variations
    const int tolerance = 20;

    return abs(pixel.rgbRed - fillRed) <= tolerance &&
        abs(pixel.rgbGreen - fillGreen) <= tolerance &&
        abs(pixel.rgbBlue - fillBlue) <= tolerance;
}

bool CaptureSystem::IsMarkerPixel(const RGBQUAD& pixel) {
    // Check for regular marker color (#99A6C0)
    const int markerRed = 0x99;   // 153
    const int markerGreen = 0xA6; // 166
    const int markerBlue = 0xC0;  // 192

    // Check for filled marker color (#9BB0ED)
    const int filledMarkerRed = 0x9B;   // 155
    const int filledMarkerGreen = 0xB0; // 176
    const int filledMarkerBlue = 0xED;  // 237

    const int tolerance = 12;

    // Check if it's either a regular marker or a filled marker
    bool isRegularMarker = abs(pixel.rgbRed - markerRed) <= tolerance &&
        abs(pixel.rgbGreen - markerGreen) <= tolerance &&
        abs(pixel.rgbBlue - markerBlue) <= tolerance;

    bool isFilledMarker = abs(pixel.rgbRed - filledMarkerRed) <= tolerance &&
        abs(pixel.rgbGreen - filledMarkerGreen) <= tolerance &&
        abs(pixel.rgbBlue - filledMarkerBlue) <= tolerance;

    return isRegularMarker || isFilledMarker;
}

bool CaptureSystem::IsFilledMarkerPixel(const RGBQUAD& pixel) {
    const int filledMarkerRed = 0x9B;   // 155
    const int filledMarkerGreen = 0xB0; // 176
    const int filledMarkerBlue = 0xED;  // 237

    const int tolerance = 12;

    return abs(pixel.rgbRed - filledMarkerRed) <= tolerance &&
        abs(pixel.rgbGreen - filledMarkerGreen) <= tolerance &&
        abs(pixel.rgbBlue - filledMarkerBlue) <= tolerance;
}

bool CaptureSystem::IsVerticalBarSequence(int x, int y) {
    const int verticalBarWidth = 4; // Vertical bars are 4 pixels wide
    const int tolerance = 10; // Tolerance for color variations

    // Check if the next 4 pixels are vertical bar pixels
    for (int i = 0; i < verticalBarWidth; i++) {
        if (x + i >= m_captureRegion.right - m_captureRegion.left) {
            return false; // Out of bounds
        }

        RGBQUAD* pixel = reinterpret_cast<RGBQUAD*>(
            m_bitmapData + (y * (m_captureRegion.right - m_captureRegion.left) + (x + i)) * 4);

        if (!IsMarkerPixel(*pixel)) {
            return false; // Not a vertical bar pixel
        }
    }

    return true; // Found a 4-pixel vertical bar sequence
}

bool CaptureSystem::IsBackgroundPixel(const RGBQUAD& pixel) {
    // Check for background color (#002240)
    const int bgRed = 0x00;   // 0
    const int bgGreen = 0x22; // 34
    const int bgBlue = 0x40;  // 64

    const int tolerance = 8;

    return abs(pixel.rgbRed - bgRed) <= tolerance &&
        abs(pixel.rgbGreen - bgGreen) <= tolerance &&
        abs(pixel.rgbBlue - bgBlue) <= tolerance;
}

float CaptureSystem::AnalyzeRegion() {
    if (!m_bitmapData) return 0.0f;

    const int width = m_captureRegion.right - m_captureRegion.left;
    const int height = m_captureRegion.bottom - m_captureRegion.top;

    if (width <= 0 || height <= 0) return 0.0f;

    // Sample from the middle vertical position of the bar
    const int sampleY = height / 2;

    // Count filled pixels
    int filledPixels = 0;
    int totalPixels = 0;

    // Scan horizontally across the bar
    for (int x = 0; x < width; x++) {
        RGBQUAD* pixel = reinterpret_cast<RGBQUAD*>(
            m_bitmapData + (sampleY * width + x) * 4);

        // Skip if pixel isn't part of the XP bar (i.e., not fill color or background)
        if (!IsFilledPixel(*pixel) && !IsBackgroundPixel(*pixel) && !IsMarkerPixel(*pixel)) {
            continue;
        }

        // Check if this is the start of a marker sequence
        if (IsVerticalBarSequence(x, sampleY)) {
            bool isFilledLeft = false;
            bool isFilledRight = false;

            // Check pixels on both sides of the marker
            if (x > 0) {
                RGBQUAD* pixelLeft = reinterpret_cast<RGBQUAD*>(
                    m_bitmapData + (sampleY * width + (x - 1)) * 4);
                isFilledLeft = IsFilledPixel(*pixelLeft);
            }

            if (x + 4 < width) {
                RGBQUAD* pixelRight = reinterpret_cast<RGBQUAD*>(
                    m_bitmapData + (sampleY * width + (x + 4)) * 4);
                isFilledRight = IsFilledPixel(*pixelRight);
            }

            // Check if the marker itself shows the filled color
            bool isMarkerFilled = false;
            for (int i = 0; i < 4; i++) {
                RGBQUAD* markerPixel = reinterpret_cast<RGBQUAD*>(
                    m_bitmapData + (sampleY * width + (x + i)) * 4);
                if (IsFilledMarkerPixel(*markerPixel)) {
                    isMarkerFilled = true;
                    break;
                }
            }

            // Count marker as filled if either:
            // 1. Both sides are filled
            // 2. The marker itself shows the filled marker color
            if ((isFilledLeft && isFilledRight) || isMarkerFilled) {
                filledPixels += 4;
            }
            totalPixels += 4;
            x += 3; // Skip the rest of the marker
            continue;
        }

        // Count normal pixels
        if (IsFilledPixel(*pixel)) {
            filledPixels++;
        }
        totalPixels++;
    }

    // Calculate percentage
    if (totalPixels > 0) {
        return (filledPixels * 100.0f) / totalPixels;
    }

    return 0.0f;
}