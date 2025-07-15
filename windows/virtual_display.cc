#include "virtual_display.h"
#include "parsec-vdd.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>



namespace {

    struct DisplayInfo {
    bool   active        = false;
    int    identifier    = 0;
    int    cloneOf       = 0;
    int    address       = 0;       // ParseDisplayAddress
    std::string deviceName;         // "\\\\.\\DISPLAYn"
    std::string displayName;       

    // std::string adapter;
    // std::string adapterInstanceId;
    // FILETIME  lastArrival, adapterArrival;
};

static std::vector<std::string> GetDisplayPaths() {
    std::vector<std::string> paths;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\monitor\\Enum",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD count = 0, type = 0, size = sizeof(count);
        if (RegQueryValueExA(hKey, "Count", nullptr, &type,
                             reinterpret_cast<BYTE*>(&count), &size) == ERROR_SUCCESS)
        {
            for (DWORD i = 0; i < count; ++i) {
                char name[16]; sprintf_s(name, "%u", i);
                DWORD bufSize = 0;
                if (RegQueryValueExA(hKey, name, nullptr, nullptr, nullptr, &bufSize) == ERROR_SUCCESS) {
                    std::string val; val.resize(bufSize);
                    if (RegQueryValueExA(hKey, name, nullptr, nullptr,
                                         reinterpret_cast<BYTE*>(&val[0]), &bufSize) == ERROR_SUCCESS)
                    {
                        // 去掉末尾 '\0'
                        if (!val.empty() && val.back() == '\0') val.pop_back();
                        paths.push_back(val);
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }
    return paths;
}

// C# static int ParseDisplayAddress(string path)
static int ParseDisplayAddress(const std::string& path) {
    auto pos = path.find_last_of("uid");
    auto p = path.rfind("uid", pos == std::string::npos ? 0 : pos);
    if (p != std::string::npos) {
        try {
            return std::stoi(path.substr(p + 3));
        } catch(...) {}
    }
    return 0;
}

// C# static string ParseDisplayCode(string id)
static std::string ParseDisplayCode(const std::string& id) {
    std::vector<std::string> tok;
    size_t start = 0;
    while (true) {
        auto pos = id.find('#', start);
        tok.push_back(id.substr(start, pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return tok.size() >= 2 ? tok[1] : tok[0];
}

static std::vector<DisplayInfo> GetAllDisplays() {

    std::map<std::string, DisplayInfo> displayMap;
    std::vector<std::pair<DisplayInfo, DisplayInfo>> cloneGroups;
    auto paths = GetDisplayPaths();

    DISPLAY_DEVICEA ddAdapter{}, ddMonitor{};
    ddAdapter.cb = sizeof(ddAdapter);

    for (int ai = 0; EnumDisplayDevicesA(nullptr, ai, &ddAdapter, 0); ++ai) {
        DisplayInfo* prevActive = nullptr;


        ddMonitor = {}; ddMonitor.cb = sizeof(ddMonitor);
        for (int mi = 0;
             EnumDisplayDevicesA(ddAdapter.DeviceName, mi, &ddMonitor,
                                 EDD_GET_DEVICE_INTERFACE_NAME);
             ++mi)
        {

            if (!(ddMonitor.StateFlags & DISPLAY_DEVICE_ATTACHED))
                continue;

            int pathIdx = -1;
            for (int k = 0; k < (int)paths.size(); ++k) {
                std::string p = paths[k];
                std::replace(p.begin(), p.end(), '\\', '#');
                std::string deviceId(ddMonitor.DeviceID);
                if (deviceId.find(p) != std::string::npos) {
                    pathIdx = k;
                    break;
                }
            }
            if (pathIdx < 0) 
                continue;

            const std::string key = paths[pathIdx];
            if (!displayMap.count(key)) {

                DisplayInfo d;
                d.active      = !!(ddMonitor.StateFlags & DISPLAY_DEVICE_ACTIVE);
                d.address     = ParseDisplayAddress(key);
                d.deviceName  = ddAdapter.DeviceName;
                d.displayName = ParseDisplayCode(ddMonitor.DeviceID);

                if (d.active) {
                    if (!prevActive) {
                        prevActive = &displayMap[key];
                    } else {
                        cloneGroups.emplace_back(*prevActive, d);
                    }
                    // TODO: FetchAllModes() -> EnumDisplaySettings 
                }

                // TODO: Device.GetDeviceInstance / GetDeviceLastArrival / GetParentDeviceInstance / GetDeviceDescription / GetDeviceLastArrival

                displayMap[key] = d;
                paths.erase(paths.begin() + pathIdx);
            }
        }
    }

    std::vector<DisplayInfo> displays;
    for (auto &kv : displayMap)
        displays.push_back(kv.second);
    std::sort(displays.begin(), displays.end(),
              [](auto &a, auto &b){
                  return a.deviceName < b.deviceName;
              });

    for (int i = 0; i < (int)displays.size(); ++i)
        displays[i].identifier = i + 1;


    for (auto &grp : cloneGroups) {
        auto &d1 = grp.first, &d2 = grp.second;
        int id1=0, id2=0;
        for (auto &di : displays) {
            if (di.deviceName==d1.deviceName && di.address==d1.address) id1 = di.identifier;
            if (di.deviceName==d2.deviceName && di.address==d2.address) id2 = di.identifier;
        }
        if (id1 && id2) {
            for (auto &di : displays) {
                if (di.identifier==id1) di.cloneOf = id2;
                if (di.identifier==id2) di.cloneOf = id1;
            }
        }
    }

    return displays;
}
}

VirtualDisplay::VirtualDisplay() : display_id_(-1) {
    config_ = DisplayConfig();
}

VirtualDisplay::~VirtualDisplay() {
}

bool VirtualDisplay::ChangeDisplaySettings(const DisplayConfig& config) {
    //VirtualDisplay::DumpAllDisplays();
    std::vector<DisplayInfo> all = GetAllDisplays();
    for (auto &d : all) {
        std::printf("[%d] %s (%s#%d)%s\n",
            d.identifier,
            d.deviceName.c_str(),
            d.displayName.c_str(),
            d.address,
            d.cloneOf ? " (clone)" : ""
        );
    }
    config_ = config;
    

    if (all.empty() || all.size() < display_id_ || display_id_ < 0) {
        return false;
    }
    
    DEVMODEA dm = {};
    dm.dmSize = sizeof(dm);
    dm.dmPelsWidth = config.width;
    dm.dmPelsHeight = config.height;
    dm.dmBitsPerPel = config.bit_depth;
    dm.dmDisplayFrequency = config.refresh_rate;    
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

    std::cout << "ChangeDisplaySettingsExA parameters:" << std::endl;
    std::cout << "  Device: " << all[display_id_].deviceName << std::endl;
    std::cout << "  Resolution: " << dm.dmPelsWidth << "x" << dm.dmPelsHeight << std::endl;
    std::cout << "  Color depth: " << dm.dmBitsPerPel << " bits" << std::endl;
    std::cout << "  Refresh rate: " << dm.dmDisplayFrequency << " Hz" << std::endl;
    std::cout << "  Field flags: 0x" << std::hex << dm.dmFields << std::dec << std::endl;

    LONG result = ChangeDisplaySettingsExA( all[display_id_].deviceName.c_str(), 
                                           &dm, NULL, CDS_UPDATEREGISTRY, NULL);
    
    return (result == DISP_CHANGE_SUCCESSFUL);
}

bool VirtualDisplay::FindParsecDisplay(char* deviceName, int bufferSize) {
    DISPLAY_DEVICEA adapter = {};
    adapter.cb = sizeof(adapter);
    
    for (DWORD adapterIndex = 0; EnumDisplayDevicesA(NULL, adapterIndex, &adapter, 0); adapterIndex++) {
        
        bool isParsecAdapter = (strstr(adapter.DeviceString, parsec_vdd::VDD_DISPLAY_NAME) != nullptr) ||
                              (strstr(adapter.DeviceString, parsec_vdd::VDD_ADAPTER_NAME) != nullptr) ||
                              (strstr(adapter.DeviceID, parsec_vdd::VDD_HARDWARE_ID) != nullptr);
        
        if (isParsecAdapter) {
            DWORD parsecMonitorCount = 0;
            
            for (DWORD monitorIndex = 0; ; monitorIndex++) {
                DISPLAY_DEVICEA monitor = {};
                monitor.cb = sizeof(monitor);
                
                if (!EnumDisplayDevicesA(adapter.DeviceName, monitorIndex, &monitor, 0)) {
                    break;
                }
                
                if (strstr(monitor.DeviceID, parsec_vdd::VDD_DISPLAY_ID) != nullptr) {
                    if (display_id_ < 0 || parsecMonitorCount == static_cast<DWORD>(display_id_)) {
                        strcpy_s(deviceName, bufferSize, adapter.DeviceName);
                        return true;
                    }
                    
                    parsecMonitorCount++;
                }
            }
        }
        
        memset(&adapter, 0, sizeof(adapter));
        adapter.cb = sizeof(adapter);
    }
    
    return false;
}

DISPLAY_DEVICEA VirtualDisplay::MakeDisplayDevice() {
    DISPLAY_DEVICEA dd{};
    dd.cb = sizeof(dd);
    return dd;
}

void VirtualDisplay::DumpAllDisplays() {
    std::cout << "=== All Adapters ===\n";
    for (DWORD ai = 0; ; ++ai) {
        DISPLAY_DEVICEA adapter = MakeDisplayDevice();
        if (!EnumDisplayDevicesA(NULL, ai, &adapter, 0))
            break;
        std::cout
          << "[Adapter " << ai << "] "
          << adapter.DeviceName << " / " 
          << adapter.DeviceString << " / ID=" 
          << adapter.DeviceID << "\n";
    }

    std::cout << "\n=== Active Monitors ===\n";
    for (DWORD ai = 0; ; ++ai) {
        DISPLAY_DEVICEA adapter = MakeDisplayDevice();
        if (!EnumDisplayDevicesA(NULL, ai, &adapter, 0))
            break;

        for (DWORD mi = 0; ; ++mi) {
            DISPLAY_DEVICEA mon = MakeDisplayDevice();
            if (!EnumDisplayDevicesA(adapter.DeviceName, mi, &mon, 0))
                break;


            constexpr DWORD ACTIVE_FLAGS =
              DISPLAY_DEVICE_ACTIVE
            | DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;
            if ((mon.StateFlags & ACTIVE_FLAGS) == ACTIVE_FLAGS) {
                std::cout
                  << "[Monitor " << mi << "] on " 
                  << adapter.DeviceName << " -> "
                  << mon.DeviceName << " / " 
                  << mon.DeviceString << "\n";
            }
        }
    }
    std::cout << "=== End ===\n";
}