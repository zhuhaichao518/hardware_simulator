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
    static bool RemoveDisplay(int display_uid);

    static int GetDisplayCount();
    static std::vector<int> GetDisplayList();
    static bool IsInitialized() { 
        return initialized_; 
    }
    
    static bool CheckVddStatus();
    static int GetAllDisplays();
    static bool ChangeDisplaySettings(int display_uid, const VirtualDisplay::DisplayConfig& config);


    // Detailed display information structure
    // This structure mirrors the Dart DisplayData class for consistency
    struct DetailedDisplayInfo {
        // Array index
        int index = -1;
        
        // Display configuration (from VirtualDisplay::DisplayConfig)
        int width = 1920;
        int height = 1080;
        int refresh_rate = 60;
        int bit_depth = 32;
        
        // Display information (from VirtualDisplay::DisplayInfo)
        bool active = false;
        int display_uid = 0;           // Unique display identifier
        std::string device_name;       // Device name like "\\\\.\\DISPLAYn"
        std::string display_name;      // Human-readable display name
        std::string device_description; // Device description
        FILETIME last_arrival;         // Last arrival time
        
        // Additional fields
        bool is_virtual = false;       // Whether this is a virtual display
    };
    static std::vector<DetailedDisplayInfo> GetDetailedDisplayList();

    VirtualDisplayControl() = delete;
    VirtualDisplayControl(const VirtualDisplayControl&) = delete;
    VirtualDisplayControl& operator=(const VirtualDisplayControl&) = delete;

private:
    static bool initialized_;
    static HANDLE vdd_handle_;
    static std::vector<std::unique_ptr<VirtualDisplay>> displays_;
};

#endif