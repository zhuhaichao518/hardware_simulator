#ifndef VIRTUAL_DISPLAY_H_
#define VIRTUAL_DISPLAY_H_

#include <windows.h>

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

    VirtualDisplay();
    ~VirtualDisplay();

    bool ChangeDisplaySettings(const DisplayConfig& config);
    DisplayConfig GetConfig() const { return config_; }
    int GetDisplayId() const { return display_id_; }
    void SetDisplayId(int id) { display_id_ = id; }

private:
    DisplayConfig config_;
    int display_id_;
    
    bool FindParsecDisplay(char* deviceName, int bufferSize);

    //debug
    static void DumpAllDisplays();
    static DISPLAY_DEVICEA MakeDisplayDevice();

    VirtualDisplay(const VirtualDisplay&) = delete;
    VirtualDisplay& operator=(const VirtualDisplay&) = delete;
};

#endif