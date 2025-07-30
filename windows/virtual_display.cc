#include "virtual_display.h"
#include "parsec-vdd.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>

#include <vector>
#include <algorithm>
#include <devpkey.h>


VirtualDisplay::VirtualDisplay() : display_uid_(-1), current_orientation_(0) {
    std::cout << "VirtualDisplay created" << std::endl;
    config_ = DisplayConfig();
}

VirtualDisplay::~VirtualDisplay() {
}

bool VirtualDisplay::ChangeDisplaySettings(const DisplayConfig& config) {
    
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = config.width;
    dm.dmPelsHeight = config.height;
    dm.dmDisplayFrequency = config.refresh_rate;    
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    
    std::wstring device_name_w(info_.device_name.begin(), info_.device_name.end());
    LONG result = ChangeDisplaySettingsExW(device_name_w.c_str(), 
                                           &dm, NULL, CDS_UPDATEREGISTRY, NULL);
    return (result == DISP_CHANGE_SUCCESSFUL);
}

void VirtualDisplay::FetchAllDisplayConfigs() {
    DEVMODEW dev_mode = {};
    dev_mode.dmSize = sizeof(DEVMODEW);

    display_config_list_.clear();

    std::wstring device_name_w(info_.device_name.begin(), info_.device_name.end());
    
    for (int config_num = -1; EnumDisplaySettingsW(device_name_w.c_str(), config_num, &dev_mode); config_num++) {
        DisplayConfig display_config;
        display_config.width = dev_mode.dmPelsWidth;
        display_config.height = dev_mode.dmPelsHeight;
        display_config.refresh_rate = dev_mode.dmDisplayFrequency;

        if (config_num == -1) {
            config_ = display_config;
            current_orientation_ = dev_mode.dmDisplayOrientation;
        } else {
            display_config_list_.push_back(display_config);
        }
    }

    std::sort(display_config_list_.begin(), display_config_list_.end(),
              [](const DisplayConfig& a, const DisplayConfig& b) {
                  if (a.width != b.width) return a.width < b.width;
                  if (a.height != b.height) return a.height < b.height;
                  return a.refresh_rate < b.refresh_rate;
              });
}

const std::vector<VirtualDisplay::DisplayConfig>& VirtualDisplay::GetDisplayConfigList() const {
    const_cast<VirtualDisplay*>(this)->FetchAllDisplayConfigs();
    return display_config_list_;
}