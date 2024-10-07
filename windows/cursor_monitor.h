#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include <memory>
#include <vector>
using CursorChangedCallback = std::function<void(int, int, const std::vector<uint8_t>&)>;

#define CURSOR_INVISIBLE 1
#define CURSOR_VISIBLE 2
#define CURSOR_UPDATED_DEFAULT 3
#define CURSOR_UPDATED_IMAGE 4
#define CURSOR_UPDATED_CACHED 5
class CursorMonitor {
public:
    static CursorChangedCallback g_callback;
    static HWINEVENTHOOK Global_HOOK;
    static void startHook(CursorChangedCallback callback);
    static void endHook();
};