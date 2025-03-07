#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

class NotificationWindow {
public:
    static void Show(const std::wstring& message);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
    NotificationWindow(HWND hwnd);
    NotificationWindow(const std::wstring& message);
    ~NotificationWindow();

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Initialize();
    void CreateAndShowWindow();
    void RunMessageLoop();
    void DrawWindow(HDC hdc);

    static ULONG_PTR gdiplusToken;
    static int instanceCount;

    HWND hwnd = nullptr;
    std::wstring fullMessage;
    bool buttonHovered = false;
    bool animationActive = false;
    int lineX = 0;

    static double dpi;
    static int WINDOW_WIDTH;
    static int WINDOW_HEIGHT;
    static int BUTTON_WIDTH;
    static int BUTTON_HEIGHT;

    // Constants
    static constexpr const wchar_t* CLASS_NAME = L"NotificationWindowClass";
    static constexpr int ANIMATION_DURATION = 3000;
    static constexpr int TIMER_ID = 1;
};
