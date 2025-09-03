#include "cursor_monitor.h"
#include "hardware_simulator_plugin.h"

#include <windows.h>
#include <string>
#include <unordered_set>
#include <cstring>
#include <map>

HWINEVENTHOOK CursorMonitor::Global_HOOK = nullptr;
// Constants (Define as needed)
const int kBytesPerPixel = 4;

// Cached cursor to hash for a callback
static std::map<long long, std::unordered_set<uint32_t>> cachedcursors;
static std::map<long long, CursorChangedCallback> callbacks;
static std::map<long long, bool> hookAllCursorImage;

// Position monitoring callbacks and state
static std::map<long long, CursorPositionCallback> positionCallbacks;
static HHOOK positionHook = nullptr;
static POINT lastCursorPos = {0, 0};

bool HasAlphaChannel(const uint32_t* data, int stride, int width, int height) {
    const RGBQUAD* plane = reinterpret_cast<const RGBQUAD*>(data);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (plane->rgbReserved != 0)
                return true;
            plane += 1;
        }
        plane += stride - width;
    }
    return false;
}

#define RGBA(r, g, b, a)                                   \
  ((((a) << 24) & 0xff000000) | (((b) << 16) & 0xff0000) | \
   (((g) << 8) & 0xff00) | ((r)&0xff))

// Pixel colors used when generating cursor outlines.
const uint32_t kPixelRgbaBlack = RGBA(0, 0, 0, 0xff);
const uint32_t kPixelRgbaWhite = RGBA(0xff, 0xff, 0xff, 0xff);
const uint32_t kPixelRgbaTransparent = RGBA(0, 0, 0, 0);

const uint32_t kPixelRgbWhite = RGB(0xff, 0xff, 0xff);

// Expands the cursor shape to add a white outline for visibility against
// dark backgrounds.
void AddCursorOutline(int width, int height, uint32_t* data) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // If this is a transparent pixel (bgr == 0 and alpha = 0), check the
            // neighbor pixels to see if this should be changed to an outline pixel.
            if (*data == kPixelRgbaTransparent) {
                // Change to white pixel if any neighbors (top, bottom, left, right)
                // are black.
                if ((y > 0 && data[-width] == kPixelRgbaBlack) ||
                    (y < height - 1 && data[width] == kPixelRgbaBlack) ||
                    (x > 0 && data[-1] == kPixelRgbaBlack) ||
                    (x < width - 1 && data[1] == kPixelRgbaBlack)) {
                    *data = kPixelRgbaWhite;
                }
            }
            data++;
        }
    }
}

// Premultiplies RGB components of the pixel data in the given image by
// the corresponding alpha components.
void AlphaMul(uint32_t* data, int width, int height) {
    static_assert(sizeof(uint32_t) == kBytesPerPixel,
        "size of uint32 should be the number of bytes per pixel");

    for (uint32_t* data_end = data + width * height; data != data_end; ++data) {
        RGBQUAD* from = reinterpret_cast<RGBQUAD*>(data);
        RGBQUAD* to = reinterpret_cast<RGBQUAD*>(data);
        to->rgbBlue =
            (static_cast<uint16_t>(from->rgbBlue) * from->rgbReserved) / 0xff;
        to->rgbGreen =
            (static_cast<uint16_t>(from->rgbGreen) * from->rgbReserved) / 0xff;
        to->rgbRed =
            (static_cast<uint16_t>(from->rgbRed) * from->rgbReserved) / 0xff;
    }
}

std::unique_ptr<uint32_t[]> CreateMouseCursorFromHCursor(
    HDC dc,
    HCURSOR cursor,
    int* final_width,
    int* final_height,
    int* hotspot_x,
    int* hotspot_y) {

    ICONINFO iinfo;
    if (!GetIconInfo(cursor, &iinfo)) {
        return NULL;
    }

    *hotspot_x = iinfo.xHotspot;
    *hotspot_y = iinfo.yHotspot;

    bool is_color = iinfo.hbmColor != NULL;
    BITMAP bitmap_info;
    if (!GetObject(iinfo.hbmMask, sizeof(bitmap_info), &bitmap_info)) {
        return NULL;
    }

    int width = bitmap_info.bmWidth;
    int height = bitmap_info.bmHeight;
    std::unique_ptr<uint32_t[]> mask_data(new uint32_t[width * height]);

    BITMAPV5HEADER bmi = { 0 };
    bmi.bV5Size = sizeof(bmi);
    bmi.bV5Width = width;
    bmi.bV5Height = -height;  // Request a top-down bitmap
    bmi.bV5Planes = 1;
    bmi.bV5BitCount = kBytesPerPixel * 8;
    bmi.bV5Compression = BI_RGB;
    bmi.bV5AlphaMask = 0xff000000;
    bmi.bV5CSType = LCS_WINDOWS_COLOR_SPACE;
    bmi.bV5Intent = LCS_GM_BUSINESS;

    if (!GetDIBits(dc, iinfo.hbmMask, 0, height, mask_data.get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS)) {
        return NULL;
    }

    uint32_t* mask_plane = mask_data.get();
    bool has_alpha = false;
    std::unique_ptr<uint32_t[]> color_data(new uint32_t[width * height * 4]);

    if (is_color) {
        if (!GetDIBits(dc, iinfo.hbmColor, 0, height, color_data.get(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS)) {
            return NULL;
        }
        has_alpha = HasAlphaChannel(color_data.get(), width, width, height);
    }
    else {
        height /= 2;
        color_data = std::unique_ptr<uint32_t[]>(new uint32_t[width * height * 4]);
        memcpy(color_data.get(), mask_plane + (width * height), width * height * 4);
    }

    if (!has_alpha) {
        bool add_outline = false;
        uint32_t* dst = color_data.get();
        uint32_t* mask = mask_plane;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (*mask == RGB(0xff, 0xff, 0xff)) {
                    if (*dst != 0) {
                        add_outline = true;
                        *dst = RGBA(0, 0, 0, 0xff);
                    }
                    else {
                        *dst = RGBA(0, 0, 0, 0);
                    }
                }
                else {
                    *dst = RGBA(0, 0, 0, 0xff) ^ *dst;
                }
                ++dst;
                ++mask;
            }
        }
        if (add_outline) {
            AddCursorOutline(width, height, color_data.get());
        }
    }

    AlphaMul(color_data.get(), width, height);

    if (iinfo.hbmColor) {
        DeleteObject(iinfo.hbmColor);
    }
    if (iinfo.hbmMask) {
        DeleteObject(iinfo.hbmMask);
    }

    *final_width = width;
    *final_height = height;

    return std::move(color_data);
}

HANDLE GetCursorHandle(HCURSOR hCursor) {
    static HCURSOR hArrow = LoadCursor(NULL, IDC_ARROW);
    if (hCursor == hArrow)
        return IDC_ARROW;

    static HCURSOR hIBeam = LoadCursor(NULL, IDC_IBEAM);
    if (hCursor == hIBeam)
        return IDC_IBEAM;

    static HCURSOR hWait = LoadCursor(NULL, IDC_WAIT);
    if (hCursor == hWait)
        return IDC_WAIT;

    static HCURSOR hCross = LoadCursor(NULL, IDC_CROSS);
    if (hCursor == hCross)
        return IDC_CROSS;

    static HCURSOR hUpArrow = LoadCursor(NULL, IDC_UPARROW);
    if (hCursor == hUpArrow)
        return IDC_UPARROW;

    static HCURSOR hSize = LoadCursor(NULL, IDC_SIZE);
    if (hCursor == hSize)
        return IDC_SIZE;

    static HCURSOR hIcon = LoadCursor(NULL, IDC_ICON);
    if (hCursor == hIcon)
        return IDC_ICON;

    static HCURSOR hSizeNWSE = LoadCursor(NULL, IDC_SIZENWSE);
    if (hCursor == hSizeNWSE)
        return IDC_SIZENWSE;

    static HCURSOR hSizeNESW = LoadCursor(NULL, IDC_SIZENESW);
    if (hCursor == hSizeNESW)
        return IDC_SIZENESW;

    static HCURSOR hSizeWE = LoadCursor(NULL, IDC_SIZEWE);
    if (hCursor == hSizeWE)
        return IDC_SIZEWE;

    static HCURSOR hSizeNS = LoadCursor(NULL, IDC_SIZENS);
    if (hCursor == hSizeNS)
        return IDC_SIZENS;

    static HCURSOR hSizeAll = LoadCursor(NULL, IDC_SIZEALL);
    if (hCursor == hSizeAll)
        return IDC_SIZEALL;

    static HCURSOR hNo = LoadCursor(NULL, IDC_NO);
    if (hCursor == hNo)
        return IDC_NO;

#if (WINVER >= 0x0500)
    static HCURSOR hHand = LoadCursor(NULL, IDC_HAND);
    if (hCursor == hHand)
        return IDC_HAND;
#endif

    static HCURSOR hAppStarting = LoadCursor(NULL, IDC_APPSTARTING);
    if (hCursor == hAppStarting)
        return IDC_APPSTARTING;

#if (WINVER >= 0x0400)
    static HCURSOR hHelp = LoadCursor(NULL, IDC_HELP);
    if (hCursor == hHelp)
        return IDC_HELP;
#endif

#if (WINVER >= 0x0606)
    static HCURSOR hPin = LoadCursor(NULL, IDC_PIN);
    if (hCursor == hPin)
        return IDC_PIN;

    static HCURSOR hPerson = LoadCursor(NULL, IDC_PERSON);
    if (hCursor == hPerson)
        return IDC_PERSON;
#endif

    return NULL;
}

uint32_t JSHash(const uint32_t* buffer, int size) {
    uint32_t hash = 1315423911;
    for (std::size_t i = 0; i < size; i++) {
        hash ^= ((hash << 5) + buffer[i] + (hash >> 2));
    }
    return (hash & 0x7FFFFFFF);
}


unsigned char test_endian(void) {
    int test_var = 1;
    unsigned char* test_endian = reinterpret_cast<unsigned char*>(&test_var);
    return (test_endian[0] == 0);
}

std::vector<uint8_t> ConvertUint32ToUint8(const uint32_t* inputArray,
    uint32_t width,
    uint32_t height,
    uint32_t hotx,
    uint32_t hoty,
    uint32_t hash) {
    std::vector<uint8_t> outputArray;
    outputArray.reserve(width * height * sizeof(uint32_t) + sizeof(hash) * 3 +
        1);

    // 9 means it is cursor data. This is used by CloudPlayPlus.
    outputArray.push_back(static_cast<uint8_t>(9));

    // Add size mark
    uint8_t* wBytes = reinterpret_cast<uint8_t*>(&width);
    if (test_endian()) {
        // Little endian system
        for (size_t i = 0; i < sizeof(width); ++i) {
            outputArray.push_back(wBytes[i]);
        }
    }
    else {
        // Big endian system
        for (size_t i = sizeof(width); i > 0; --i) {
            outputArray.push_back(wBytes[i - 1]);
        }
    }

    // Add size mark
    uint8_t* hBytes = reinterpret_cast<uint8_t*>(&height);
    if (test_endian()) {
        // Little endian system
        for (size_t i = 0; i < sizeof(height); ++i) {
            outputArray.push_back(hBytes[i]);
        }
    }
    else {
        // Big endian system
        for (size_t i = sizeof(height); i > 0; --i) {
            outputArray.push_back(hBytes[i - 1]);
        }
    }

    uint8_t* hxBytes = reinterpret_cast<uint8_t*>(&hotx);
    if (test_endian()) {
        // Little endian system
        for (size_t i = 0; i < sizeof(hotx); ++i) {
            outputArray.push_back(hxBytes[i]);
        }
    }
    else {
        // Big endian system
        for (size_t i = sizeof(hotx); i > 0; --i) {
            outputArray.push_back(hxBytes[i - 1]);
        }
    }

    uint8_t* hyBytes = reinterpret_cast<uint8_t*>(&hoty);
    if (test_endian()) {
        // Little endian system
        for (size_t i = 0; i < sizeof(hoty); ++i) {
            outputArray.push_back(hyBytes[i]);
        }
    }
    else {
        // Big endian system
        for (size_t i = sizeof(hoty); i > 0; --i) {
            outputArray.push_back(hyBytes[i - 1]);
        }
    }

    // Add hash bytes
    uint8_t* hashBytes = reinterpret_cast<uint8_t*>(&hash);
    if (test_endian()) {
        for (size_t i = 0; i < sizeof(hash); ++i) {
            outputArray.push_back(hashBytes[i]);
        }
    }
    else {
        for (size_t i = sizeof(hash); i > 0; --i) {
            outputArray.push_back(hashBytes[i - 1]);
        }
    }

    // Add uint32_t values
    for (size_t i = 0; i < width * height; ++i) {
        uint32_t value = inputArray[i];
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);

        if (test_endian()) {
            for (size_t j = sizeof(uint32_t); j > 0; --j) {
                outputArray.push_back(bytes[j - 1]);
            }
        }
        else {
            for (size_t j = 0; j < sizeof(uint32_t); ++j) {
                outputArray.push_back(bytes[j]);
            }
        }
    }
    return outputArray;
}

void SyncCursorImage() {
    CURSORINFO ci = { sizeof(ci) };
    GetCursorInfo(&ci);
    HANDLE h = GetCursorHandle(ci.hCursor);
    if (h != NULL) {
        for (auto callback : callbacks) {
            if (!hookAllCursorImage[callback.first]) {
                callback.second(CPP_CURSOR_UPDATED_DEFAULT, (int)reinterpret_cast<intptr_t>(h), {});
            } else {
                // For hookAll=true, treat it as if h was NULL
                HDC hdc = GetDC(nullptr);
                int width = 0, height = 0, hotX = 0, hotY = 0;
                std::unique_ptr<uint32_t[]> image = std::move(
                    CreateMouseCursorFromHCursor(hdc, ci.hCursor, &width, &height, &hotX, &hotY));
                ReleaseDC(nullptr, hdc);

                unsigned int hash = JSHash(image.get(), width * height);

                std::vector<uint8_t> datawith8bitbytes = {};
                if (cachedcursors[callback.first].find(hash) != cachedcursors[callback.first].end()) {
                    callback.second(CPP_CURSOR_UPDATED_CACHED, hash, {});
                }
                else {
                    if (datawith8bitbytes.size() == 0) {
                        datawith8bitbytes =
                            ConvertUint32ToUint8(image.get(), width, height, hotX, hotY, hash);
                    }
                    cachedcursors[callback.first].insert(hash);
                    callback.second(CPP_CURSOR_UPDATED_IMAGE, hash, datawith8bitbytes);
                }
            }
        }
    }
    else {
        HDC hdc = GetDC(nullptr);
        int width = 0, height = 0, hotX = 0, hotY = 0;
        std::unique_ptr<uint32_t[]> image = std::move(
            CreateMouseCursorFromHCursor(hdc, ci.hCursor, &width, &height, &hotX, &hotY));
        ReleaseDC(nullptr, hdc);

        unsigned int hash = JSHash(image.get(), width * height);

        std::vector<uint8_t> datawith8bitbytes = {};
        for (auto callback : callbacks) {
            if (cachedcursors[callback.first].find(hash) != cachedcursors[callback.first].end()) {
                callback.second(CPP_CURSOR_UPDATED_CACHED, hash, {});
            }
            else {
                if (datawith8bitbytes.size() == 0) {
                    datawith8bitbytes =
                        ConvertUint32ToUint8(image.get(), width, height, hotX, hotY, hash);
                }
                cachedcursors[callback.first].insert(hash);
                callback.second(CPP_CURSOR_UPDATED_IMAGE, hash, datawith8bitbytes);
            }
        }
    }
}

struct MousePosition {
    int screenId;
    float xPercent;
    float yPercent;
};

MousePosition GetMousePositionAndScreenId() {
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    HMONITOR hMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &monitorInfo);

    // Use HardwareSimulatorPlugin's static monitors
    const auto& monitors_info = hardware_simulator::HardwareSimulatorPlugin::GetStaticMonitors();
    std::vector<RECT> monitors;
    for (const auto& info : monitors_info) {
        monitors.push_back(info.rect);
    }

    int screenId = 0;
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].left == monitorInfo.rcMonitor.left && 
            monitors[i].top == monitorInfo.rcMonitor.top &&
            monitors[i].right == monitorInfo.rcMonitor.right && 
            monitors[i].bottom == monitorInfo.rcMonitor.bottom) {
            screenId = static_cast<int>(i);
            break;
        }
    }

    float xPercent = (cursorPos.x - monitorInfo.rcMonitor.left) / 
                     (float)(monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left);
    float yPercent = (cursorPos.y - monitorInfo.rcMonitor.top) / 
                     (float)(monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
    
    //xPercent = std::max(0.0f, std::min(1.0f, xPercent));
    //yPercent = std::max(0.0f, std::min(1.0f, yPercent));
    
    return {screenId, xPercent, yPercent};
}

// Low-level mouse hook procedure for position monitoring
LRESULT CALLBACK MousePositionHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_MOUSEMOVE) {
        POINT currentPos;
        GetCursorPos(&currentPos);
        
        // Check if position has changed
        if (currentPos.x != lastCursorPos.x || currentPos.y != lastCursorPos.y) {
            lastCursorPos = currentPos;
            
            // Get mouse position and screen info
            MousePosition mousePos = GetMousePositionAndScreenId();
            
            // Notify all position callbacks with direct double values
            for (auto& callback : positionCallbacks) {
                callback.second(CPP_CURSOR_POSITION_CHANGED, mousePos.screenId, mousePos.xPercent, mousePos.yPercent);
            }
        }
    }
    
    return CallNextHookEx(positionHook, nCode, wParam, lParam);
}

std::vector<uint8_t> FloatToBytes(float x, float y) {
    std::vector<uint8_t> outputArray;
    outputArray.reserve(sizeof(float) * 2);

    // Add x coordinate
    uint8_t* xBytes = reinterpret_cast<uint8_t*>(&x);
    if (test_endian()) {
        // Little endian system
        for (size_t i = 0; i < sizeof(x); ++i) {
            outputArray.push_back(xBytes[i]);
        }
    }
    else {
        // Big endian system
        for (size_t i = sizeof(x); i > 0; --i) {
            outputArray.push_back(xBytes[i - 1]);
        }
    }

    // Add y coordinate
    uint8_t* yBytes = reinterpret_cast<uint8_t*>(&y);
    if (test_endian()) {
        // Little endian system
        for (size_t i = 0; i < sizeof(y); ++i) {
            outputArray.push_back(yBytes[i]);
        }
    }
    else {
        // Big endian system
        for (size_t i = sizeof(y); i > 0; --i) {
            outputArray.push_back(yBytes[i - 1]);
        }
    }

    return outputArray;
}

bool IsCursorVisible() {
    CURSORINFO ci = { 0 };
    ci.cbSize = sizeof(CURSORINFO);

    if (GetCursorInfo(&ci)) {
        return (ci.flags & CURSOR_SUPPRESSED) == 0;
    }
    return true;
}

void CursorChangedEventProc(HWINEVENTHOOK hook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD time) {
    if (hwnd == nullptr && idObject == OBJID_CURSOR && idChild == CHILDID_SELF) {
        std::string str;
        switch (event) {
        case EVENT_OBJECT_HIDE:
            for (auto callback : callbacks) {
                MousePosition mousePos = GetMousePositionAndScreenId();
                std::vector<uint8_t> positionBytes = FloatToBytes(mousePos.xPercent, mousePos.yPercent);
                callback.second(CPP_CURSOR_INVISIBLE, mousePos.screenId, positionBytes);
            }
            break;
        case EVENT_OBJECT_SHOW:
            for (auto callback : callbacks) {
                MousePosition mousePos = GetMousePositionAndScreenId();
                std::vector<uint8_t> positionBytes = FloatToBytes(mousePos.xPercent, mousePos.yPercent);
                callback.second(CPP_CURSOR_VISIBLE, mousePos.screenId, positionBytes);
            }
            SyncCursorImage();
            break;
        case EVENT_OBJECT_NAMECHANGE:
        {
            SyncCursorImage();
            break;
        }
        default:
            break;
        }
    }
}

void CursorMonitor::startHook(CursorChangedCallback callback, long long callback_id, bool hookAll) {
    if (callbacks.empty()) {
        Global_HOOK = SetWinEventHook(
            EVENT_OBJECT_SHOW, EVENT_OBJECT_NAMECHANGE,
            nullptr, CursorChangedEventProc, 0, 0,
            WINEVENT_OUTOFCONTEXT);
    }
    callbacks[callback_id] = callback;
    hookAllCursorImage[callback_id] = hookAll;
    cachedcursors[callback_id] = {};

    if (!IsCursorVisible()) {
        MousePosition mousePos = GetMousePositionAndScreenId();
        std::vector<uint8_t> positionBytes = FloatToBytes(mousePos.xPercent, mousePos.yPercent);
        callback(CPP_CURSOR_INVISIBLE, mousePos.screenId, positionBytes);
    }

    // If hookAll is true, trigger an immediate callback
    if (hookAll) {
        CURSORINFO ci = { sizeof(ci) };
        GetCursorInfo(&ci);
        HDC hdc = GetDC(nullptr);
        int width = 0, height = 0, hotX = 0, hotY = 0;
        std::unique_ptr<uint32_t[]> image = std::move(
            CreateMouseCursorFromHCursor(hdc, ci.hCursor, &width, &height, &hotX, &hotY));
        ReleaseDC(nullptr, hdc);

        unsigned int hash = JSHash(image.get(), width * height);
        std::vector<uint8_t> datawith8bitbytes = ConvertUint32ToUint8(image.get(), width, height, hotX, hotY, hash);
        cachedcursors[callback_id].insert(hash);
        callback(CPP_CURSOR_UPDATED_IMAGE, hash, datawith8bitbytes);
    }
}

void CursorMonitor::endHook(long long callback_id) {
    callbacks.erase(callbacks.find(callback_id));
    hookAllCursorImage.erase(hookAllCursorImage.find(callback_id));
    cachedcursors.erase(cachedcursors.find(callback_id));
    if (callbacks.empty()) {
        UnhookWinEvent(Global_HOOK);
    }
}

void CursorMonitor::startPositionHook(CursorPositionCallback callback, long long callback_id) {
    positionCallbacks[callback_id] = callback;
    
    // Start hook thread if this is the first position callback
    // We have to do this: https://www.soinside.com/question/hP9qqHrPWatdNm68nNtFfd
    if (positionCallbacks.size() == 1) {
        startHookThread();
        GetCursorPos(&lastCursorPos);
    }
    
    // Send initial position
    MousePosition mousePos = GetMousePositionAndScreenId();
    callback(CPP_CURSOR_POSITION_CHANGED, mousePos.screenId, mousePos.xPercent, mousePos.yPercent);
}

void CursorMonitor::endPositionHook(long long callback_id) {
    positionCallbacks.erase(positionCallbacks.find(callback_id));
    
    // Stop hook thread if no more position callbacks
    if (positionCallbacks.empty()) {
        stopHookThread();
    }
}

// Static member definitions for thread management
std::unique_ptr<std::thread> CursorMonitor::hookThread = nullptr;
std::atomic<bool> CursorMonitor::shouldStopHookThread{false};
std::atomic<bool> CursorMonitor::hookThreadRunning{false};

void CursorMonitor::startHookThread() {
    if (hookThreadRunning.load()) {
        return; // Thread already running
    }
    
    shouldStopHookThread.store(false);
    hookThread = std::make_unique<std::thread>(hookThreadFunction);
}

void CursorMonitor::stopHookThread() {
    if (!hookThreadRunning.load()) {
        return; // Thread not running
    }
    
    shouldStopHookThread.store(true);

    // Post a quit message to wake up the message loop
    DWORD threadId = GetThreadId(hookThread->native_handle());
    if (threadId != 0) {
        PostThreadMessage(threadId, WM_QUIT, 0, 0);
    }

    if (hookThread && hookThread->joinable()) {
        hookThread->join();
    }
    
    hookThread.reset();
    hookThreadRunning.store(false);
}

void CursorMonitor::hookThreadFunction() {
    hookThreadRunning.store(true);
    
    // Install the hook in this thread
    positionHook = SetWindowsHookEx(WH_MOUSE_LL, MousePositionHookProc, GetModuleHandle(nullptr), 0);
    
    if (positionHook == nullptr) {
        hookThreadRunning.store(false);
        return;
    }
    
    // Message loop for the hook thread
    MSG msg;
    while (!shouldStopHookThread.load()) {
        // Use GetMessage - PostThreadMessage will wake it up when stopping
        BOOL result = GetMessage(&msg, nullptr, 0, 0);
        
        if (result == 0) {
            // WM_QUIT received
            break;
        } else if (result == -1) {
            // Error occurred
            break;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up hook
    if (positionHook != nullptr) {
        UnhookWindowsHookEx(positionHook);
        positionHook = nullptr;
    }
    
    hookThreadRunning.store(false);
}
