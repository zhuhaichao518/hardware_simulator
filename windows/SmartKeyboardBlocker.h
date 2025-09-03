#pragma once

#include <windows.h>
#include <functional>

/**
 * Smart Keyboard Blocker - Only blocks keys when the current app has focus
 * Does not affect other applications' normal operation
 * Uses static methods, no instance needed
 */
class SmartKeyboardBlocker {
public:
    // Callback function type for blocked keys
    using BlockedKeyCallback = std::function<void(const DWORD vk_code, bool isDown)>;

    /**
     * Start smart blocking (static method)
     * @param target_window Target window handle, if nullptr auto-detect current process main window
     * @param callback Optional callback function called when keys are blocked
     * @return Returns true on success
     */
    static bool StartBlocking(HWND target_window = nullptr, BlockedKeyCallback callback = nullptr);

    /**
     * Stop blocking (static method)
     */
    static void StopBlocking();

    /**
     * Check if currently blocking (static method)
     */
    static bool IsBlocking() { return hook_handle_ != nullptr; }

    /**
     * Set callback function (static method)
     */
    static void SetCallback(BlockedKeyCallback callback) { callback_ = callback; }

    /**
     * Get current process main window
     */
    static HWND GetMainWindow();

    static HWND target_window_;
    static bool IsTargetWindowActive();

private:
    // Disable constructor - static methods only
    SmartKeyboardBlocker() = delete;
    SmartKeyboardBlocker(const SmartKeyboardBlocker&) = delete;
    SmartKeyboardBlocker& operator=(const SmartKeyboardBlocker&) = delete;

    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    static bool ShouldBlockKey(DWORD vk_code);

    // Static member variables
    static HHOOK hook_handle_;
    static BlockedKeyCallback callback_;
    static std::unordered_map<DWORD, bool> key_states_; // Track key states to avoid repeat callbacks
};
