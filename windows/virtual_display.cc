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


VirtualDisplay::VirtualDisplay() : display_id_(-1) {
    std::cout << "VirtualDisplay created" << std::endl;
    config_ = DisplayConfig();
    //DumpAllDisplays();
    std::cout << " ----------------------------------------------------- xxx ----------------" << std::endl;
    //GetAllDisplays();
     std::cout << " ----------------------------------------------------- xxx ----------------" << std::endl;
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
    
    std::wstring wDeviceName(info_.deviceName.begin(), info_.deviceName.end());
    LONG result = ChangeDisplaySettingsExW(wDeviceName.c_str(), 
                                           &dm, NULL, CDS_UPDATEREGISTRY, NULL);
    
    return (result == DISP_CHANGE_SUCCESSFUL);
}

DISPLAY_DEVICEW VirtualDisplay::MakeDisplayDevice() {
    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);
    return dd;
}