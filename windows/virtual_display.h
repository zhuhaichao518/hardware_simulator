#ifndef VIRTUAL_DISPLAY_H_
#define VIRTUAL_DISPLAY_H_

#include <windows.h>
#include <string>

class VirtualDisplay {
public:
    struct DisplayConfig {
        int width;
        int height;
        int refresh_rate;
        int bit_depth = 32; // Default to 32-bit color depth
        
        DisplayConfig() : width(1920), height(1080), refresh_rate(60), bit_depth(32) {}
        DisplayConfig(int w, int h, int rate = 60, int depth = 32) : width(w), height(h), refresh_rate(rate), bit_depth(depth) {}
    };

    struct DisplayInfo {
        bool   active        = false;
        bool   is_virtual    = false;  // Whether this is a virtual display
        int    display_uid   = 0;       // Display unique identifier  
        std::string device_name;         // "\\\\.\\DISPLAYn"
        std::string display_name; 
        std::string device_description;   
        FILETIME last_arrival;
    };


    VirtualDisplay();
    VirtualDisplay(const DisplayConfig& config, const DisplayInfo& info, int display_uid = -1)
        : config_(config), info_(info), display_uid_(display_uid){}
    ~VirtualDisplay();

    bool ChangeDisplaySettings(const DisplayConfig& config);
    DisplayConfig GetConfig() const { return config_; }
    DisplayInfo GetDisplayInfo() const { return info_; }
    int GetDisplayUid() const { return display_uid_; }
    void SetDisplayUid(int uid) { display_uid_ = uid; }

private:
    DisplayConfig config_;
    DisplayInfo info_;
    int display_uid_;

    VirtualDisplay(const VirtualDisplay&) = delete;
    VirtualDisplay& operator=(const VirtualDisplay&) = delete;
};

#endif