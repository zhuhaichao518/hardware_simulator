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
        int    displayUid       = 0;       // ParseDisplayAddress
        std::string deviceName;         // "\\\\.\\DISPLAYn"
        std::string displayName; 
        std::string deviceDescription;   
        FILETIME lastArrival;
    };


    VirtualDisplay();
    VirtualDisplay(const DisplayConfig& config, const DisplayInfo& info, int display_id = -1)
        : config_(config), info_(info), display_id_(display_id){}
    ~VirtualDisplay();

    bool ChangeDisplaySettings(const DisplayConfig& config);
    DisplayConfig GetConfig() const { return config_; }
    DisplayInfo GetDisplayInfo() const { return info_; }
    int GetDisplayId() const { return display_id_; }
    void SetDisplayId(int id) { display_id_ = id; }

private:
    DisplayConfig config_;
    DisplayInfo info_;
    int display_id_;
    
    bool FindParsecDisplay(char* deviceName, int bufferSize);

    //debug
    static void DumpAllDisplays();
    static DISPLAY_DEVICEW MakeDisplayDevice();

    VirtualDisplay(const VirtualDisplay&) = delete;
    VirtualDisplay& operator=(const VirtualDisplay&) = delete;
};

#endif