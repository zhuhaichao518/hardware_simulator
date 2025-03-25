#include "notification_window.h"

ULONG_PTR NotificationWindow::gdiplusToken = 0;
int NotificationWindow::instanceCount = 0;
double NotificationWindow::dpi = 0;

int NotificationWindow::WINDOW_WIDTH = 300;
int NotificationWindow::WINDOW_HEIGHT = 100;
int NotificationWindow::BUTTON_WIDTH = 80;
int NotificationWindow::BUTTON_HEIGHT = 30;

void NotificationWindow::Show(const std::wstring& message) {
    if (instanceCount++ == 0) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    }

    NotificationWindow* instance = new NotificationWindow(message);

    instance->Initialize();
    instance->CreateAndShowWindow();
    instance->RunMessageLoop();
}

std::wstring Utf16FromUtf8(const std::string& string) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), -1, nullptr, 0);
    if (size_needed == 0) {
        return {};
    }
    std::wstring wstrTo(size_needed, 0);
    int converted_length = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), -1, &wstrTo[0], size_needed);
    if (converted_length == 0) {
        return {};
    }
    return wstrTo;
}

NotificationWindow::NotificationWindow(const std::wstring& message)
{
    fullMessage = message + Utf16FromUtf8(" 连接到了你的设备");
}

NotificationWindow::~NotificationWindow() {
    if (--instanceCount == 0) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }
}

void NotificationWindow::Initialize() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wc);
}

int GetDpiFromTemporaryWindow() {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"TempDpiWindowClass";
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_NOACTIVATE, L"TempDpiWindowClass", L"", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);

    int dpi = 96;
    if (hwnd) {
        dpi = GetDpiForWindow(hwnd);
        DestroyWindow(hwnd);
    }

    UnregisterClass(L"TempDpiWindowClass", GetModuleHandle(NULL));

    return dpi;
}

void NotificationWindow::CreateAndShowWindow() {
    dpi = GetDpiFromTemporaryWindow() / 96.0;

    WINDOW_WIDTH = (int)(300 * dpi);
    WINDOW_HEIGHT = (int)(100 * dpi);
    BUTTON_WIDTH = (int)(80 * dpi);
    BUTTON_HEIGHT = (int)(30 * dpi);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = screenWidth - WINDOW_WIDTH - (int)(10 * dpi);
    int y = screenHeight - WINDOW_HEIGHT - (int)(40 * dpi);

    hwnd = CreateWindowEx(
        WS_EX_NOACTIVATE |WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        CLASS_NAME,
        L"Notification",
        WS_POPUP /*| WS_DISABLED*/,
        x, y,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (hwnd) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        animationActive = true;
        SetTimer(hwnd, TIMER_ID, (UINT)(ANIMATION_DURATION * dpi) / WINDOW_WIDTH, nullptr);
    }
}

void NotificationWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK NotificationWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    NotificationWindow* instance = nullptr;

    if (uMsg == WM_NCCREATE) {
        auto createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        instance = static_cast<NotificationWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
        instance->hwnd = hwnd;
    }
    else {
        instance = reinterpret_cast<NotificationWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    return instance ? instance->HandleMessage(uMsg, wParam, lParam) : DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT NotificationWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        delete this;
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawWindow(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_ID) {
            lineX += 2;
            if (lineX >= WINDOW_WIDTH) {
                lineX = 0;
                KillTimer(hwnd, TIMER_ID);
                animationActive = false;
                DestroyWindow(hwnd);
            }
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        bool inButton = (x >= WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2) && (x <= WINDOW_WIDTH / 2 + BUTTON_WIDTH / 2) &&
            (y >= WINDOW_HEIGHT - BUTTON_HEIGHT - 10) && (y <= WINDOW_HEIGHT - 10);

        if (inButton != buttonHovered) {
            buttonHovered = inButton;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        if ((x >= WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2) && (x <= WINDOW_WIDTH / 2 + BUTTON_WIDTH / 2) &&
            (y >= WINDOW_HEIGHT - BUTTON_HEIGHT - 10) && (y <= WINDOW_HEIGHT - 10)) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void NotificationWindow::DrawWindow(HDC hdc) {
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
    SelectObject(memDC, hBitmap);

    Gdiplus::Graphics graphics(memDC);

    // Draw background
    Gdiplus::SolidBrush background(Gdiplus::Color(200, 220, 240));
    graphics.FillRectangle(&background, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Draw text
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 102, 0));
    Gdiplus::Font font(L"Segoe UI", 12);
    graphics.DrawString(fullMessage.c_str(), -1, &font, Gdiplus::PointF(10, 10), &textBrush);

    // Draw button
    Gdiplus::SolidBrush btnBrush(buttonHovered ?
        Gdiplus::Color(100, 150, 200) : Gdiplus::Color(0, 100, 200));
    Gdiplus::Rect btnRect(WINDOW_WIDTH / 2 - BUTTON_WIDTH / 2,
        WINDOW_HEIGHT - BUTTON_HEIGHT - 10,
        BUTTON_WIDTH, BUTTON_HEIGHT);
    graphics.FillRectangle(&btnBrush, btnRect);

    // Draw button border
    Gdiplus::Pen pen(Gdiplus::Color(0, 0, 0));
    graphics.DrawRectangle(&pen, btnRect);

    // Draw button text
    Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255));
    graphics.DrawString(Utf16FromUtf8("确定").c_str(), -1, &font,
        Gdiplus::PointF(btnRect.X + (Gdiplus::REAL)(19 * dpi), btnRect.Y + (Gdiplus::REAL)(5 * dpi)), &whiteBrush);

    // Draw animation line
    if (animationActive) {
        Gdiplus::Pen redPen(Gdiplus::Color(255, 0, 0), 2);
        graphics.DrawLine(&redPen, lineX, WINDOW_HEIGHT - (int)(5 * dpi), lineX + (int)(1 * dpi), WINDOW_HEIGHT - (int)(5 * dpi));
    }

    BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, memDC, 0, 0, SRCCOPY);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
}
