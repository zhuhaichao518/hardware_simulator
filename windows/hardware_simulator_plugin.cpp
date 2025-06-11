#include "hardware_simulator_plugin.h"

#include "cursor_monitor.h"
#include "gamecontroller_manager.h"
#include "notification_window.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <algorithm>
#include <future>
#include <memory>
#include <sstream>

// Used to run win32 service.
#include <sddl.h>
#include <shellapi.h>
#include <shlwapi.h>  // PathCombineW, PathRemoveFileSpecW
#include <string>

#pragma comment(lib, "shlwapi.lib")

typedef HSYNTHETICPOINTERDEVICE(WINAPI* PFN_CreateSyntheticPointerDevice)(POINTER_INPUT_TYPE pointerType, ULONG maxCount, POINTER_FEEDBACK_MODE mode);
typedef BOOL(WINAPI* PFN_InjectSyntheticPointerInput)(HSYNTHETICPOINTERDEVICE device, CONST POINTER_TYPE_INFO* pointerInfo, UINT32 count);
typedef VOID(WINAPI* PFN_DestroySyntheticPointerDevice)(HSYNTHETICPOINTERDEVICE device);

namespace hardware_simulator {

// Function declarations
void performKeyEvent(uint16_t modcode, bool isDown, bool isRepeat);
void performTouchEvent(int screenId, double x, double y, uint32_t touchId, bool isDown, bool isRepeat);

thread_local HDESK _lastKnownInputDesktop = nullptr;
PFN_CreateSyntheticPointerDevice fnCreateSyntheticPointerDevice = nullptr;
PFN_InjectSyntheticPointerInput fnInjectSyntheticPointerInput = nullptr;
PFN_DestroySyntheticPointerDevice fnDestroySyntheticPointerDevice = nullptr;

HSYNTHETICPOINTERDEVICE g_touchDevice = nullptr;
POINTER_TYPE_INFO g_touchInfo[10] = {};
UINT32 g_activeTouchSlots = 0;

// auto repeat feature
struct KeyState {
    bool isDown = false;
    std::chrono::steady_clock::time_point lastEventTime;
};

struct TouchState {
    bool isDown = false;
    double x = 0;
    double y = 0;
    int screenId = 0;
    std::chrono::steady_clock::time_point lastEventTime;
};

static bool g_auto_repeat_enabled = true;
static std::atomic<bool> g_thread_running{false};
static std::mutex g_event_mutex;
static std::unordered_map<uint16_t, KeyState> g_key_states;
static uint16_t g_last_known_key_down = 0;
static std::unordered_map<uint32_t, TouchState> g_touch_states;

static void EventMonitorThread() {
    while (g_thread_running) {
        if (g_auto_repeat_enabled) {
            auto now = std::chrono::steady_clock::now();
            
            {
                std::lock_guard<std::mutex> lock(g_event_mutex);
                
                // Check key states
                if (g_key_states.find(g_last_known_key_down) != g_key_states.end()) {
                    if (g_key_states[g_last_known_key_down].isDown) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - g_key_states[g_last_known_key_down].lastEventTime).count();
                        if (duration >= 500) {  // 500ms repeat interval for keys
                            performKeyEvent(g_last_known_key_down, true, true);
                        }
                    }
                }
                
                // Check touch states
                for (auto& [id, state] : g_touch_states) {
                    if (state.isDown) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - state.lastEventTime).count();
                        if (duration >= 300) {  // 300ms repeat interval for touch
                            performTouchEvent(state.screenId, state.x, state.y, id, true, true);
                            state.lastEventTime = now;
                        }
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void HardwareSimulatorPlugin::StopMonitorThread() {
    if (g_thread_running) {
        g_thread_running = false;
        if (monitor_thread_ && monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

void HardwareSimulatorPlugin::StartMonitorThread() {
    if (!g_thread_running) {
        g_thread_running = true;
        monitor_thread_ = std::make_unique<std::thread>(EventMonitorThread);
    }
}

void SetAutoRepeatEnabled(bool enabled) {
    if (g_auto_repeat_enabled == enabled) {
        return;  // No change needed
    }

    g_auto_repeat_enabled = enabled;
    
    // Note: Since this is a global function, we can't access the plugin instance directly.
    // The auto-repeat state will be handled when the thread is next started/stopped.
    if (!enabled) {
        // Clear any existing states
        std::lock_guard<std::mutex> lock(g_event_mutex);
        g_key_states.clear();
        g_touch_states.clear();
    }
}
// end of auto repeat feature

//Todo:OpenInputDesktop should fail because we have a window resource in this process.
//We need to create another process to handle this scenario.
HDESK syncThreadDesktop() {
    auto hDesk = OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, FALSE, GENERIC_ALL);
    if (!hDesk) {
        //auto err = GetLastError();
        //BOOST_LOG(error) << "Failed to Open Input Desktop [0x"sv << util::hex(err).to_string_view() << ']';

        return nullptr;
    }

    if (!SetThreadDesktop(hDesk)) {
        //auto err = GetLastError();
        //BOOST_LOG(error) << "Failed to sync desktop to thread [0x"sv << util::hex(err).to_string_view() << ']';
    }

    CloseDesktop(hDesk);

    return hDesk;
}

struct MonitorInfo {
    RECT rect;
    bool is_primary;
};

std::vector<MonitorInfo> g_monitors;

void update_monitors() {
    g_monitors.clear();
    //EnumDisplayDevicesW in WebRTC
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT rect, LPARAM data) {
        auto& list = *reinterpret_cast<std::vector<MonitorInfo>*>(data);
        MONITORINFO info{ sizeof(MONITORINFO) };
        GetMonitorInfo(hMon, &info);
        list.push_back({ info.rcMonitor, (info.dwFlags & MONITORINFOF_PRIMARY) != 0 });
        return TRUE;
        }, reinterpret_cast<LPARAM>(&g_monitors));
}

std::vector<MonitorInfo> get_monitors() {
    return g_monitors;
}

LRESULT CALLBACK DpiMonitorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DPICHANGED:
            update_monitors();
            break;
        case WM_DISPLAYCHANGE:
            update_monitors();
            break; 
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*
HWND CreateHiddenMessageWindow() {
    WNDCLASS wc{};
    wc.lpfnWndProc = DpiMonitorWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DpiMonitorWindow";
    RegisterClass(&wc);

    return CreateWindowEx(0, wc.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
}*/

HWND CreateHiddenMessageWindow() {
    WNDCLASS wc{};
    wc.lpfnWndProc = DpiMonitorWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DpiMonitorWindow";

    if (!RegisterClass(&wc)) {
        //DWORD err = GetLastError();
        //OutputDebugString(L"RegisterClass failed!");
        return nullptr;
    }

    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"Hidden DPI Monitor",
        WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr
    );

    if (!hWnd) {
        //DWORD err = GetLastError();
        //OutputDebugString(L"CreateWindowEx failed!");
        return nullptr;
    }

    ShowWindow(hWnd, SW_HIDE);
    return hWnd;
}

bool adjust_to_main_screen(int screen_index, double x_percent, double y_percent, LONG& out_x, LONG& out_y) {
    auto monitors = get_monitors();
    if (screen_index < 0 || screen_index >= monitors.size()) return false;

    const auto& screen = monitors[screen_index];
    int ox, oy;
    ox = screen.rect.left + static_cast<int>((screen.rect.right - screen.rect.left) * x_percent);
    oy = screen.rect.top + static_cast<int>((screen.rect.bottom - screen.rect.top) * y_percent);

    int primary_width = GetSystemMetrics(SM_CXSCREEN);
    int primary_height = GetSystemMetrics(SM_CYSCREEN);

    out_x = static_cast<LONG>(ox * (65535.0f / primary_width));
    out_y = static_cast<LONG>(oy * (65535.0f / primary_height));
    return true;
}

bool adjust_touch_to_screen(int screen_index, double x_percent, double y_percent, LONG& out_x, LONG& out_y) {
    auto monitors = get_monitors();
    if (monitors.empty() || screen_index < 0 || screen_index >= monitors.size()) {
        out_x = out_y = 0;
        return false;
    }

    RECT global_bounds = {0};
    for (const auto& monitor : monitors) {
        global_bounds.left = (std::min)(global_bounds.left, monitor.rect.left);
        global_bounds.top = (std::min)(global_bounds.top, monitor.rect.top);
        global_bounds.right = (std::max)(global_bounds.right, monitor.rect.right);
        global_bounds.bottom = (std::max)(global_bounds.bottom, monitor.rect.bottom);
    }

    const auto& screen = monitors[screen_index];
    
    double global_x = (screen.rect.left - global_bounds.left + 
                      (screen.rect.right - screen.rect.left) * x_percent) / 
                     (global_bounds.right - global_bounds.left);
    
    double global_y = (screen.rect.top - global_bounds.top + 
                      (screen.rect.bottom - screen.rect.top) * y_percent) / 
                     (global_bounds.bottom - global_bounds.top);

    out_x = static_cast<LONG>(global_x * GetSystemMetrics(SM_CXVIRTUALSCREEN));
    out_y = static_cast<LONG>(global_y * GetSystemMetrics(SM_CYVIRTUALSCREEN));
    return true;
}

bool initTouchAPI() {
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (!hUser32) {
        return false;
    }

    fnCreateSyntheticPointerDevice = (PFN_CreateSyntheticPointerDevice)GetProcAddress(hUser32, "CreateSyntheticPointerDevice");
    fnInjectSyntheticPointerInput = (PFN_InjectSyntheticPointerInput)GetProcAddress(hUser32, "InjectSyntheticPointerInput");
    fnDestroySyntheticPointerDevice = (PFN_DestroySyntheticPointerDevice)GetProcAddress(hUser32, "DestroySyntheticPointerDevice");

    return fnCreateSyntheticPointerDevice && fnInjectSyntheticPointerInput && fnDestroySyntheticPointerDevice;
}

bool createTouchDevice() {
    if (!fnCreateSyntheticPointerDevice) {
        initTouchAPI();
        if (!fnCreateSyntheticPointerDevice) {
            return false;
        }
    }

    g_touchDevice = fnCreateSyntheticPointerDevice(PT_TOUCH, ARRAYSIZE(g_touchInfo), POINTER_FEEDBACK_DEFAULT);
    return g_touchDevice != nullptr;
}

void destroyTouchDevice() {
    if (g_touchDevice && fnDestroySyntheticPointerDevice) {
        fnDestroySyntheticPointerDevice(g_touchDevice);
        g_touchDevice = nullptr;
    }
}

bool sendTouchInput() {
    if (!g_touchDevice || !fnInjectSyntheticPointerInput) {
        return false;
    }

    if (fnInjectSyntheticPointerInput(g_touchDevice, g_touchInfo, g_activeTouchSlots)) {
        return true;
    }

    return false;
}

void async_send_touch_input_retry() {
    while (true) {
        auto send = sendTouchInput();
        if (send == 1) {
            break;
        }

        auto hDesk = syncThreadDesktop();
        if (_lastKnownInputDesktop != hDesk) {
            _lastKnownInputDesktop = hDesk;
        }
        else {
            break;
        }
    }
}

void send_touch_input() {
    auto send = sendTouchInput();
    if (send != true) {
        // put resend into new thread.
        std::future<void> retry_future = std::async(std::launch::async, async_send_touch_input_retry);
        retry_future.wait();
    }
}

void performTouchEvent(int screenId, double x, double y, uint32_t touchId, bool isDown, bool isRepeat = false) {
    if (!g_touchDevice) {
        if (!createTouchDevice()) {
            return;
        }
    }

    POINTER_TYPE_INFO* pointer = nullptr;
    for (UINT32 i = 0; i < ARRAYSIZE(g_touchInfo); i++) {
        if (g_touchInfo[i].touchInfo.pointerInfo.pointerId == touchId) {
            pointer = &g_touchInfo[i];
            break;
        }
    }

    if (!pointer) {
        for (UINT32 i = 0; i < ARRAYSIZE(g_touchInfo); i++) {
            if (g_touchInfo[i].touchInfo.pointerInfo.pointerFlags == POINTER_FLAG_NONE) {
                pointer = &g_touchInfo[i];
                g_touchInfo[i].touchInfo.pointerInfo.pointerId = touchId;
                g_activeTouchSlots = (g_activeTouchSlots > (i + 1)) ? g_activeTouchSlots : (i + 1);
                break;
            }
        }
    }

    if (!pointer) {
        return;
    }

    pointer->type = PT_TOUCH;
    auto& touchInfo = pointer->touchInfo;
    touchInfo.pointerInfo.pointerType = PT_TOUCH;

    LONG out_x, out_y;
    if (!adjust_touch_to_screen(screenId,x,y,out_x,out_y)) return;
    touchInfo.pointerInfo.ptPixelLocation.x = out_x;//static_cast<LONG>(x * GetSystemMetrics(SM_CXSCREEN));
    touchInfo.pointerInfo.ptPixelLocation.y = out_y;//static_cast<LONG>(y * GetSystemMetrics(SM_CYSCREEN));

    if (isDown) {
        touchInfo.pointerInfo.pointerFlags = TOUCHEVENTF_PRIMARY | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
    } else {
        touchInfo.pointerInfo.pointerFlags = POINTER_FLAG_UP;
    }

    touchInfo.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;

    touchInfo.rcContact.left = touchInfo.pointerInfo.ptPixelLocation.x - 10;
    touchInfo.rcContact.right = touchInfo.pointerInfo.ptPixelLocation.x + 10;
    touchInfo.rcContact.top = touchInfo.pointerInfo.ptPixelLocation.y - 10;
    touchInfo.rcContact.bottom = touchInfo.pointerInfo.ptPixelLocation.y + 10;

    touchInfo.pressure = 1024;

    send_touch_input();

    // Add state tracking
    if (g_auto_repeat_enabled && !isRepeat) {
        std::lock_guard<std::mutex> lock(g_event_mutex);
        if (isDown) {
            g_touch_states[touchId] = {true, x, y, screenId, std::chrono::steady_clock::now()};
        } else if (g_touch_states.find(touchId) != g_touch_states.end()) {
            g_touch_states.erase(touchId);
        }
    }
}

void performTouchMove(int screenId, double x, double y, uint32_t touchId) {
    if (!g_touchDevice) {
        return;
    }

    POINTER_TYPE_INFO* pointer = nullptr;
    for (UINT32 i = 0; i < ARRAYSIZE(g_touchInfo); i++) {
        if (g_touchInfo[i].touchInfo.pointerInfo.pointerId == touchId) {
            pointer = &g_touchInfo[i];
            break;
        }
    }

    if (!pointer) {
        return;
    }

    auto& touchInfo = pointer->touchInfo;

    LONG out_x, out_y;
    if (!adjust_touch_to_screen(screenId, x, y, out_x, out_y)) return;
    touchInfo.pointerInfo.ptPixelLocation.x = out_x;//static_cast<LONG>(x * GetSystemMetrics(SM_CXSCREEN));
    touchInfo.pointerInfo.ptPixelLocation.y = out_y;//static_cast<LONG>(y * GetSystemMetrics(SM_CYSCREEN));

    touchInfo.pointerInfo.pointerFlags = POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_UPDATE;

    touchInfo.rcContact.left = touchInfo.pointerInfo.ptPixelLocation.x - 10;
    touchInfo.rcContact.right = touchInfo.pointerInfo.ptPixelLocation.x + 10;
    touchInfo.rcContact.top = touchInfo.pointerInfo.ptPixelLocation.y - 10;
    touchInfo.rcContact.bottom = touchInfo.pointerInfo.ptPixelLocation.y + 10;

    if (g_auto_repeat_enabled) {
        std::lock_guard<std::mutex> lock(g_event_mutex);
        if (g_touch_states.find(touchId) != g_touch_states.end()) {
            //update the position
            g_touch_states[touchId] = { true, x, y, screenId, std::chrono::steady_clock::now() };
        }
    }

    send_touch_input();
}

BOOL IsRunningAsSystem() {
    BOOL bIsSystem = FALSE;
    HANDLE hToken = NULL;
    DWORD dwLengthNeeded = 0;
    PTOKEN_USER pTokenUser = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwLengthNeeded) &&
        (GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
        CloseHandle(hToken);
        return FALSE;
    }

    pTokenUser = (PTOKEN_USER)LocalAlloc(LPTR, dwLengthNeeded);
    if (!pTokenUser) {
        CloseHandle(hToken);
        return FALSE;
    }

    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwLengthNeeded, &dwLengthNeeded)) {
        LocalFree(pTokenUser);
        CloseHandle(hToken);
        return FALSE;
    }

    PSID pSystemSid = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&NtAuthority, 1, SECURITY_LOCAL_SYSTEM_RID,
        0, 0, 0, 0, 0, 0, 0, &pSystemSid)) {
        LocalFree(pTokenUser);
        CloseHandle(hToken);
        return FALSE;
    }

    if (EqualSid(pTokenUser->User.Sid, pSystemSid)) {
        bIsSystem = TRUE;
    }

    FreeSid(pSystemSid);
    LocalFree(pTokenUser);
    CloseHandle(hToken);

    return bIsSystem;
}

bool RunBatchAsAdmin(
    LPCWSTR lpBatchFileName, 
    DWORD* pErrorCode = nullptr, 
    bool bWait = false
) {
    WCHAR exePath[MAX_PATH] = {0};
    if (0 == GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        if (pErrorCode) *pErrorCode = GetLastError();
        return false;
    }

    WCHAR exeDir[MAX_PATH] = {0};
    wcscpy_s(exeDir, exePath);
    if (!PathRemoveFileSpecW(exeDir)) {
        if (pErrorCode) *pErrorCode = ERROR_PATH_NOT_FOUND;
        return false;
    }

    WCHAR batchPath[MAX_PATH] = {0};
    if (!PathCombineW(batchPath, exeDir, lpBatchFileName)) {
        if (pErrorCode) *pErrorCode = ERROR_INVALID_NAME;
        return false;
    }

    if (INVALID_FILE_ATTRIBUTES == GetFileAttributesW(batchPath)) {
        if (pErrorCode) *pErrorCode = GetLastError();
        return false;
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";       // require admin
    sei.lpFile = batchPath;      // whole path
    sei.nShow = SW_SHOW;
    
    if (bWait) {
        sei.fMask = SEE_MASK_NOCLOSEPROCESS; 
    }

    if (!ShellExecuteExW(&sei)) {
        const DWORD err = GetLastError();
        if (pErrorCode) *pErrorCode = err;
        return false;
    }

    // if needed, wait for result.
    if (bWait && sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }

    return true;
}

// static
void HardwareSimulatorPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  CreateHiddenMessageWindow();
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "hardware_simulator",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<HardwareSimulatorPlugin>();
  auto plugin_pointer = plugin.get();

  plugin->channel_ = std::move(channel);

  plugin->channel_->SetMethodCallHandler(
      [plugin_pointer](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  // Start the monitor thread if auto-repeat is enabled
  if (g_auto_repeat_enabled) {
      plugin_pointer->StartMonitorThread();
  }

  registrar->AddPlugin(std::move(plugin));
}

HardwareSimulatorPlugin::HardwareSimulatorPlugin() {}

HardwareSimulatorPlugin::~HardwareSimulatorPlugin() {
    StopMonitorThread();
    destroyTouchDevice();
}

void async_send_input_retry(INPUT& i) {
    while (true) {
        auto send = SendInput(1, &i, sizeof(INPUT));
        if (send == 1) {
            break;
        }

        auto hDesk = syncThreadDesktop();
        if (_lastKnownInputDesktop != hDesk) {
            _lastKnownInputDesktop = hDesk;
        }
        else {
            break;
        }
    }
}

void send_input(INPUT& i) {
    auto send = SendInput(1, &i, sizeof(INPUT));
    if (send != 1) {
        // put resend into new thread.
        std::future<void> retry_future = std::async(std::launch::async, async_send_input_retry, std::ref(i));
        retry_future.wait();
    }
}

void performMouseButton(int button, bool release) {
    //SHORT KEY_STATE_DOWN = 0x8000;

    INPUT i{};

    i.type = INPUT_MOUSE;
    auto& mi = i.mi;

    int mouse_button;
    if (button == 1) {
        mi.dwFlags = release ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
        mouse_button = VK_LBUTTON;
    }
    else if (button == 2) {
        mi.dwFlags = release ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
        mouse_button = VK_MBUTTON;
    }
    else if (button == 3) {
        mi.dwFlags = release ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
        mouse_button = VK_RBUTTON;
    }
    else if (button == 4) {
        mi.dwFlags = release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
        mi.mouseData = XBUTTON1;
        mouse_button = VK_XBUTTON1;
    }
    else {
        mi.dwFlags = release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
        mi.mouseData = XBUTTON2;
        mouse_button = VK_XBUTTON2;
    }
    
    //Haichao: what is it used for? It blocks button up for UAC window.
    /*auto key_state = GetAsyncKeyState(mouse_button);
    bool key_state_down = (key_state & 0x8000) != 0;
    if (key_state_down != release) {
        return;
    }*/

    send_input(i);
}

#pragma warning(disable:4244)

void performKeyEvent(uint16_t modcode, bool isDown, bool isRepeat = false) {
    INPUT i{};
    i.type = INPUT_KEYBOARD;
    auto& ki = i.ki;

    // For some reason, MapVirtualKey(VK_LWIN, MAPVK_VK_TO_VSC) doesn't seem to work :/
    if (modcode != VK_LWIN && modcode != VK_RWIN && modcode != VK_PAUSE) {
        ki.wScan = MapVirtualKey(modcode, MAPVK_VK_TO_VSC);
        ki.dwFlags = KEYEVENTF_SCANCODE;
    }
    else {
        ki.wVk = modcode;
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    switch (modcode) {
    case VK_RMENU:
    case VK_RCONTROL:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_DIVIDE:
        ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        break;
    default:
        break;
    }

    if (!isDown) {
        ki.dwFlags |= KEYEVENTF_KEYUP;
    }

    send_input(i);

    // Add state tracking
    if (g_auto_repeat_enabled && !isRepeat) {
        std::lock_guard<std::mutex> lock(g_event_mutex);
        if (isDown) {
            g_key_states[modcode] = {true, std::chrono::steady_clock::now()};
            g_last_known_key_down = modcode;
        } else if (g_key_states.find(modcode) != g_key_states.end()) {
            g_key_states.erase(modcode);
        }
    }
}

void performMouseMoveRelative(double x,double y){
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_MOVE;
    mi.dx = x;
    mi.dy = y;

    send_input(i);
}

void performMouseMoveAbsl(double x,double y,int screenId){
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    LONG newx = 0, newy = 0;


    mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    if (!adjust_to_main_screen(screenId, x,y, newx, newy)) return;
    mi.dx = newx;
    mi.dy = newy;

    send_input(i);
}

void scroll(int distance) {
    INPUT i{};

    i.type = INPUT_MOUSE;
    auto& mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_WHEEL;
    mi.mouseData = distance;

    send_input(i);
}

void hscroll(int distance) {
    INPUT i{};

    i.type = INPUT_MOUSE;
    auto& mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_HWHEEL;
    mi.mouseData = distance;

    send_input(i);
}

std::wstring stringToWstring(const std::string& str) {
    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (wideCharLen <= 0) return L"";

    std::wstring wstr(wideCharLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wideCharLen);
    return wstr;
}

void HardwareSimulatorPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const flutter::EncodableMap* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater()) {
      version_stream << "10+";
    } else if (IsWindows8OrGreater()) {
      version_stream << "8";
    } else if (IsWindows7OrGreater()) {
      version_stream << "7";
    }
    result->Success(flutter::EncodableValue(version_stream.str()));
  } else if (method_call.method_name().compare("getMonitorCount") == 0) {
    int monitorCount = GetSystemMetrics(SM_CMONITORS);
    update_monitors();
    result->Success(flutter::EncodableValue(monitorCount));
  } else if (method_call.method_name().compare("KeyPress") == 0) {
        auto keyCode = (args->find(flutter::EncodableValue("code")))->second;
        auto isDown = (args->find(flutter::EncodableValue("isDown")))->second;
        performKeyEvent(static_cast<int>(std::get<int>((keyCode))), static_cast<bool>(std::get<bool>((isDown))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mouseMoveR") == 0) {
        auto deltax = (args->find(flutter::EncodableValue("x")))->second;
        auto deltay = (args->find(flutter::EncodableValue("y")))->second;
        performMouseMoveRelative(static_cast<double>(std::get<double>((deltax))), static_cast<double>(std::get<double>((deltay))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mouseMoveA") == 0) {
        auto percentx = (args->find(flutter::EncodableValue("x")))->second;
        auto percenty = (args->find(flutter::EncodableValue("y")))->second;
        auto screenId = (args->find(flutter::EncodableValue("screenId")))->second;
        performMouseMoveAbsl(static_cast<double>(std::get<double>((percentx))), static_cast<double>(std::get<double>((percenty))), static_cast<int>(std::get<int>((screenId))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mousePress") == 0) {
        auto buttonid = (args->find(flutter::EncodableValue("buttonId")))->second;
        auto isDown = (args->find(flutter::EncodableValue("isDown")))->second;
        performMouseButton(static_cast<int>(std::get<int>((buttonid))), !static_cast<bool>(std::get<bool>((isDown))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mouseScroll") == 0) {
        auto dx = (args->find(flutter::EncodableValue("dx")))->second;
        auto dy = (args->find(flutter::EncodableValue("dy")))->second;
        int deltax = static_cast<double>(std::get<double>((dx)));
        int deltay = static_cast<double>(std::get<double>((dy)));
        if (deltax!=0){
          hscroll(deltax * 2);
        }
        if (deltay!=0){
          scroll(deltay * 2);
        }
        result->Success(nullptr);
  } else if (method_call.method_name().compare("hookCursorImage") == 0) {
        auto callbackID = static_cast<int>(std::get<int>((args->find(flutter::EncodableValue("callbackID")))->second));
        auto hookAll = static_cast<bool>(std::get<bool>((args->find(flutter::EncodableValue("hookAll")))->second));
        CursorMonitor::startHook([this, callbackID](int message, int msg_info, const std::vector<uint8_t>& cursorImage) {
            flutter::EncodableMap encoded_message;
            encoded_message[flutter::EncodableValue("callbackID")] = flutter::EncodableValue(callbackID);
            encoded_message[flutter::EncodableValue("message")] = flutter::EncodableValue(message);
            encoded_message[flutter::EncodableValue("msg_info")] = flutter::EncodableValue(msg_info);
            encoded_message[flutter::EncodableValue("cursorImage")] = flutter::EncodableValue(std::vector<uint8_t>(cursorImage.begin(), cursorImage.end()));
            if (channel_) {
                channel_->InvokeMethod("onCursorImageMessage", 
                    std::make_unique<flutter::EncodableValue>(encoded_message));
            }
        }, callbackID, hookAll);
        result->Success(nullptr);
  } else if (method_call.method_name().compare("unhookCursorImage") == 0) {
        auto callbackID = static_cast<int>(std::get<int>((args->find(flutter::EncodableValue("callbackID")))->second));
        CursorMonitor::endHook(callbackID);
        result->Success(nullptr);
  } else if (method_call.method_name().compare("createGameController") == 0) {
        int hr = GameControllerManager::CreateGameController();
        result->Success(flutter::EncodableValue(hr));
  } else if (method_call.method_name().compare("removeGameController") == 0) {
        auto id = static_cast<int>(std::get<int>((args->find(flutter::EncodableValue("id")))->second));
        int hr = GameControllerManager::RemoveGameController(id);
        result->Success(flutter::EncodableValue(hr));
  } else if (method_call.method_name().compare("doControlAction") == 0) {
    if (args) {
        auto id_iter = args->find(flutter::EncodableValue("id"));
        auto action_iter = args->find(flutter::EncodableValue("action"));
        
        if (id_iter != args->end() && action_iter != args->end() &&
            std::holds_alternative<int>(id_iter->second) &&
            std::holds_alternative<std::string>(action_iter->second)) {
            
            int id = std::get<int>(id_iter->second);
            std::string action = std::get<std::string>(action_iter->second);

            GameControllerManager::DoControllerAction(id, action);
            result->Success(flutter::EncodableValue());
        } else {
            result->Error("InvalidArguments", "Missing or invalid arguments for doControlAction");
        }
    } else {
        result->Error("NullArguments", "Arguments are null for doControlAction");
    }
  } else if (method_call.method_name().compare("registerService") == 0) {
        DWORD dword;
        bool allowed_to_run = RunBatchAsAdmin(L"service.bat", &dword, true);
        result->Success(flutter::EncodableValue(allowed_to_run));
  } else if (method_call.method_name().compare("unregisterService") == 0) {
        DWORD dword;
        RunBatchAsAdmin(L"unregisterservice.bat", &dword, false);
        result->Success(flutter::EncodableValue());
  } else if (method_call.method_name().compare("isRunningAsSystem") == 0) {
        if (IsRunningAsSystem()) {
            result->Success(flutter::EncodableValue(true));
        }
        else {
            result->Success(flutter::EncodableValue(false));
        }
  } else if (method_call.method_name().compare("showNotification") == 0) {
        auto content = static_cast<std::string>(std::get<std::string>((args->find(flutter::EncodableValue("content")))->second));
        NotificationWindow::Show(stringToWstring(content));
  } else if (method_call.method_name().compare("touchEvent") == 0) {
        auto screenId = (args->find(flutter::EncodableValue("screenId")))->second;
        auto x = (args->find(flutter::EncodableValue("x")))->second;
        auto y = (args->find(flutter::EncodableValue("y")))->second;
        auto touchId = (args->find(flutter::EncodableValue("touchId")))->second;
        auto isDown = (args->find(flutter::EncodableValue("isDown")))->second;
        performTouchEvent(
            static_cast<int>(std::get<int>((screenId))),
            static_cast<double>(std::get<double>((x))),
            static_cast<double>(std::get<double>((y))),
            static_cast<uint32_t>(std::get<int>((touchId))),
            static_cast<bool>(std::get<bool>((isDown)))
        );
        result->Success(nullptr);
  } else if (method_call.method_name().compare("touchMove") == 0) {
        auto screenId = (args->find(flutter::EncodableValue("screenId")))->second;
        auto x = (args->find(flutter::EncodableValue("x")))->second;
        auto y = (args->find(flutter::EncodableValue("y")))->second;
        auto touchId = (args->find(flutter::EncodableValue("touchId")))->second;
        performTouchMove(
            static_cast<int>(std::get<int>((screenId))),
            static_cast<double>(std::get<double>((x))),
            static_cast<double>(std::get<double>((y))),
            static_cast<uint32_t>(std::get<int>((touchId)))
        );
        result->Success(nullptr);
  } else {
    result->NotImplemented();
  }
}

}  // namespace hardware_simulator
