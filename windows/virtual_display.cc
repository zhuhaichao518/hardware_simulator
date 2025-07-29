#include "virtual_display.h"
#include "parsec-vdd.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>

#include <vector>
#include <map>
#include <algorithm>
#include <devpkey.h>    // DEVPKEY_Device_*


VirtualDisplay::VirtualDisplay() : display_uid_(-1) {
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
    dm.dmBitsPerPel = config.bit_depth;
    dm.dmDisplayFrequency = config.refresh_rate;    
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
    
    std::wstring wDeviceName(info_.device_name.begin(), info_.device_name.end());
    LONG result = ChangeDisplaySettingsExW(wDeviceName.c_str(), 
                                           &dm, NULL, CDS_UPDATEREGISTRY, NULL);
    return (result == DISP_CHANGE_SUCCESSFUL);
}