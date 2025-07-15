#ifndef FLUTTER_PLUGIN_VIRTUAL_DISPLAY_CONTROL_H_
#define FLUTTER_PLUGIN_VIRTUAL_DISPLAY_CONTROL_H_

#include "parsec-vdd.h"
#include "virtual_display.h" 
#include <windows.h>
#include <vector>
#include <memory>
#include <string>

class VirtualDisplayControl {
public:
    static bool Initialize();
    static void Shutdown();
    static int AddDisplay();
    static int AddDisplay(int width, int height, int refresh_rate = 60);
    static bool RemoveDisplay(int display_id);

    static int GetDisplayCount();
    static std::vector<int> GetDisplayList();
    static bool IsInitialized() { 
        return initialized_; 
    }
    
    static std::string GetVddInfo();
    static bool CheckVddStatus();

    VirtualDisplayControl() = delete;
    VirtualDisplayControl(const VirtualDisplayControl&) = delete;
    VirtualDisplayControl& operator=(const VirtualDisplayControl&) = delete;

private:
    static bool initialized_;
    static HANDLE vdd_handle_;
    static std::vector<std::unique_ptr<VirtualDisplay>> displays_;
};

#endif