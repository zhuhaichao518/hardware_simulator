#include "virtual_display_control.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdio>

#define INITGUID
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <devpkey.h>    // DEVPKEY_Device_*

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "virtual_display.h"


bool VirtualDisplayControl::initialized_ = false;
HANDLE VirtualDisplayControl::vdd_handle_ = INVALID_HANDLE_VALUE;
std::vector<std::unique_ptr<VirtualDisplay>> VirtualDisplayControl::displays_;


namespace {

static std::string WideStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

static std::wstring StringToWideString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

static std::vector<std::string> GetDisplayPaths() {
    std::vector<std::string> paths;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\monitor\\Enum",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD count = 0, type = 0, size = sizeof(count);
        if (RegQueryValueExW(hKey, L"Count", nullptr, &type,
                             reinterpret_cast<BYTE*>(&count), &size) == ERROR_SUCCESS)
        {
            for (DWORD i = 0; i < count; ++i) {
                wchar_t name[16]; swprintf_s(name, L"%u", i);
                DWORD bufSize = 0;
                if (RegQueryValueExW(hKey, name, nullptr, nullptr, nullptr, &bufSize) == ERROR_SUCCESS) {
                    std::wstring val; val.resize(bufSize / sizeof(wchar_t));
                    if (RegQueryValueExW(hKey, name, nullptr, nullptr,
                                         reinterpret_cast<BYTE*>(&val[0]), &bufSize) == ERROR_SUCCESS)
                    {
                        if (!val.empty() && val.back() == L'\0') val.pop_back();
                        std::string pathStr = WideStringToString(val);
                        paths.push_back(pathStr);
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

    auto pos = path.find("UID");
    if (pos != std::string::npos) {
   
        std::string uidStr = path.substr(pos + 3); 
        

        size_t endPos = 0;
        while (endPos < uidStr.length() && std::isdigit(uidStr[endPos])) {
            endPos++;
        }
        
        if (endPos > 0) {
            try {
                std::string numberStr = uidStr.substr(0, endPos);
                std::cout << "ParseDisplayAddress: Found UID number: " << numberStr << " in path: " << path << std::endl;
                return std::stoi(numberStr);
            } catch(const std::exception& e) {
                std::cout << "ParseDisplayAddress: Error parsing number: " << e.what() << std::endl;
            }
        }
    }
    
    std::cout << "ParseDisplayAddress: No UID found in path: " << path << std::endl;
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

static bool GetDeviceInstance(const std::string& deviceId, DEVINST& devInst) {
    std::wstring wDeviceId = StringToWideString(deviceId);

    CONFIGRET cr = CM_Locate_DevNodeW(
        &devInst,
        const_cast<wchar_t*>(wDeviceId.c_str()),
        0
    );
    
    std::cout << "CM_Locate_DevNodeW result: cr = " << cr << " for deviceId: " << WideStringToString(wDeviceId) << std::endl;
    return (cr == CR_SUCCESS);
}

static bool GetDeviceLastArrival(DEVINST devInst, FILETIME& lastArrival) {
    DEVPROPKEY key = DEVPKEY_Device_LastArrivalDate;
    DEVPROPTYPE propType;
    LONGLONG fileTimeValue;
    ULONG bufferSize = sizeof(LONGLONG);

    CONFIGRET cr = CM_Get_DevNode_PropertyW(
        devInst,
        &key,
        &propType,
        reinterpret_cast<PBYTE>(&fileTimeValue),
        &bufferSize,
        0
    );
    
    std::cout << "GetDeviceLastArrival: CONFIGRET = " << cr << ", propType = " << propType << std::endl;
    
    if (cr == CR_SUCCESS) {
        std::cout << "FileTimeValue: " << fileTimeValue << std::endl;
        lastArrival.dwLowDateTime = (DWORD)(fileTimeValue & 0xFFFFFFFF);
        lastArrival.dwHighDateTime = (DWORD)(fileTimeValue >> 32);
        std::cout << "FILETIME: Low=" << lastArrival.dwLowDateTime << ", High=" << lastArrival.dwHighDateTime << std::endl;
        return true;
    }
    
    lastArrival.dwLowDateTime = 0;
    lastArrival.dwHighDateTime = 0;
    return false;
}

static bool GetParentDeviceInstance(DEVINST devInst, DEVINST& parentInst, std::string& parentId) {
    CONFIGRET cr = CM_Get_Parent(
        &parentInst,
        devInst,
        0
    );
    if (cr != CR_SUCCESS) return false;

    wchar_t idBuffer[MAX_DEVICE_ID_LEN] = {0};
    cr = CM_Get_Device_IDW(
        parentInst,
        idBuffer,
        MAX_DEVICE_ID_LEN,
        0
    );
    if (cr != CR_SUCCESS) return false;

    std::wstring wParentId(idBuffer);
    parentId = WideStringToString(wParentId);
    return true;
}

static bool GetDeviceDescription(DEVINST devInst, std::string& description) {
    DEVPROPKEY key = DEVPKEY_Device_DeviceDesc;
    DEVPROPTYPE propType;
    wchar_t buffer[256] = {0};
    ULONG bufferSize = sizeof(buffer);

    CONFIGRET cr = CM_Get_DevNode_PropertyW(
        devInst,
        &key,
        &propType,
        reinterpret_cast<PBYTE>(buffer),
        &bufferSize,
        0
    );
    
    if (cr == CR_SUCCESS && propType == DEVPROP_TYPE_STRING) {
        description = WideStringToString(std::wstring(buffer));
        return true;
    }
    
    description = "";
    return false;
}

static std::string FileTimeToString(const FILETIME& ft) {
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) {
        return "Unknown";
    }
    
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) {
        return "Invalid";
    }
    
    char buffer[64];
    sprintf_s(buffer, sizeof(buffer), 
        "%04d-%02d-%02d %02d:%02d:%02d UTC",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    
    return std::string(buffer);
}

}


bool VirtualDisplayControl::Initialize() {
    if (initialized_) {
        std::cout << "VirtualDisplayControl already initialized" << std::endl;
        return true;
    }
    
    parsec_vdd::DeviceStatus status = parsec_vdd::QueryDeviceStatus(&parsec_vdd::VDD_CLASS_GUID, parsec_vdd::VDD_HARDWARE_ID);
    
    switch (status) {
        case parsec_vdd::DEVICE_OK:
            std::cout << "Parsec VDD driver is ready" << std::endl;
            break;
        case parsec_vdd::DEVICE_NOT_INSTALLED:
            std::cout << "Error: Parsec VDD driver is not installed" << std::endl;
            return false;
        case parsec_vdd::DEVICE_DISABLED:
            std::cout << "Error: Parsec VDD driver is disabled" << std::endl;
            return false;
        case parsec_vdd::DEVICE_RESTART_REQUIRED:
            std::cout << "Warning: Restart required for Parsec VDD driver" << std::endl;
            break;
        default:
            std::cout << "Warning: Parsec VDD driver status unknown: " << status << std::endl;
            break;
    }
    
    vdd_handle_ = parsec_vdd::OpenDeviceHandle(&parsec_vdd::VDD_ADAPTER_GUID);
    
    if (vdd_handle_ == NULL || vdd_handle_ == INVALID_HANDLE_VALUE) {
        std::cout << "Error: Failed to open Parsec VDD device handle" << std::endl;
        return false;
    }
    
    int version = parsec_vdd::VddVersion(vdd_handle_);
    if (version >= 0) {
        std::cout << "Parsec VDD initialized successfully, version: " << version << std::endl;
    } else {
        std::cout << "Warning: Failed to get VDD version" << std::endl;
    }
    
    initialized_ = true;
    displays_.clear();
    
    std::thread updater([]() {
        while (initialized_ && vdd_handle_ != INVALID_HANDLE_VALUE) {
            //handle any potential issues with vdd_handle_ here
            parsec_vdd::VddUpdate(vdd_handle_);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    updater.detach();
    
    std::cout << "VirtualDisplayControl initialized successfully" << std::endl;
    return true;
}

void VirtualDisplayControl::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    for (auto& display : displays_) {
        parsec_vdd::VddRemoveDisplay(vdd_handle_, display->GetDisplayId());
    }
    
    if (vdd_handle_ != INVALID_HANDLE_VALUE) {
        parsec_vdd::CloseDeviceHandle(vdd_handle_);
        vdd_handle_ = INVALID_HANDLE_VALUE;
    }
    
    initialized_ = false;
    displays_.clear();
    std::cout << "VirtualDisplayControl shutdown completed" << std::endl;
}

int VirtualDisplayControl::AddDisplay() {
    if (!initialized_ || vdd_handle_ == INVALID_HANDLE_VALUE) {
        std::cout << "Error: VirtualDisplayControl not initialized" << std::endl;
        return -1;
    }
    
    int vdd_index = parsec_vdd::VddAddDisplay(vdd_handle_);
    if (vdd_index >= 0) {
        std::cout << "Virtual display added: " << vdd_index << std::endl;
        int nowCount = VirtualDisplayControl::GetAllDisplays();
        return nowCount;
    } else {
        std::cout << "Error: Failed to add display to VDD" << std::endl;
        return -1;
    }
}

int VirtualDisplayControl::AddDisplay(int width, int height, int refresh_rate) {
    if (!initialized_ || vdd_handle_ == INVALID_HANDLE_VALUE) {
        std::cout << "Error: VirtualDisplayControl not initialized" << std::endl;
        return -1;
    }
    
    int vdd_index = parsec_vdd::VddAddDisplay(vdd_handle_);
    if (vdd_index >= 0) {
        std::cout << "Virtual display added with config: " << vdd_index << std::endl;
        int nowCount = VirtualDisplayControl::GetAllDisplays();

        VirtualDisplay::DisplayConfig config(width, height, refresh_rate);     
        displays_[nowCount-1]->ChangeDisplaySettings(config);
        
        return vdd_index;
    } else {
        std::cout << "Error: Failed to add display to VDD" << std::endl;
        return -1;
    }
}

//async issue
bool VirtualDisplayControl::RemoveDisplay(int display_id) {
    if (!initialized_ || vdd_handle_ == INVALID_HANDLE_VALUE) {
        std::cout << "Error: VirtualDisplayControl not initialized" << std::endl;
        return false;
    }

    std::cout << "get remove call with id" << display_id << std::endl;
    
    auto it = std::find_if(displays_.begin(), displays_.end(), 
        [display_id](const std::unique_ptr<VirtualDisplay>& display) {
            return display->GetDisplayId() == display_id;
        });
    
    if (it == displays_.end()) {
        std::cout << "Error: Display not found" << std::endl;
        return false;
    }
    
    parsec_vdd::VddRemoveDisplay(vdd_handle_, display_id);
    displays_.erase(it);
    std::cout << "Virtual display removed: " << display_id << std::endl;
    return true;
}

int VirtualDisplayControl::GetDisplayCount() {
    return static_cast<int>(displays_.size());
}

std::vector<int> VirtualDisplayControl::GetDisplayList() {
    std::vector<int> ids;
    for (const auto& display : displays_) {
        ids.push_back(display->GetDisplayId());
    }
    return ids;
}

int VirtualDisplayControl::GetAllDisplays() {

    std::map <std::string, std::unique_ptr<VirtualDisplay>> displayMap;
    auto paths= GetDisplayPaths();

    DISPLAY_DEVICEW ddAdapter{}, ddMonitor{};
    ddAdapter.cb = sizeof(ddAdapter);

    for (int ai = 0; EnumDisplayDevicesW(nullptr, ai, &ddAdapter, 0); ++ai) {

        std::cout << "__________________________________________---_______________________________" << std::endl;
        std::cout << "Processing adapter: " << WideStringToString(ddAdapter.DeviceName) << "\n";
        std::cout << "StateFlags: " << ddAdapter.StateFlags << "\n";
        std::cout << "DeviceID: " << WideStringToString(ddAdapter.DeviceID) << "\n";



        ddMonitor = {}; ddMonitor.cb = sizeof(ddMonitor);
        for (int mi = 0;
             EnumDisplayDevicesW(ddAdapter.DeviceName, mi, &ddMonitor,
                                 EDD_GET_DEVICE_INTERFACE_NAME);
             ++mi)
        {
            std::cout << "__________________________________________--display-_______________________________" << std::endl;
            std::cout << "Processing monitor: " << WideStringToString(ddMonitor.DeviceName) << "\n";
            std::cout << "StateFlags: " << ddMonitor.StateFlags << "\n";
            std::cout << "DeviceID: " << WideStringToString(ddMonitor.DeviceID) << "\n";


            if (!(ddMonitor.StateFlags & DISPLAY_DEVICE_ATTACHED))
                continue;

            int pathIdx = -1;
            for (int k = 0; k < (int)paths.size(); ++k) {
                std::string p = paths[k];
                std::replace(p.begin(), p.end(), '\\', '#');
                std::wstring wDeviceId(ddMonitor.DeviceID);
                std::string deviceId = WideStringToString(wDeviceId);
                if (deviceId.find(p) != std::string::npos) {
                    pathIdx = k;
                    break;
                }
            }
            if (pathIdx < 0) 
                continue;

            const std::string devicePath = paths[pathIdx];

            std::wstring wAdapterDeviceId(ddAdapter.DeviceID);
            std::string adapterDeviceId = WideStringToString(wAdapterDeviceId);
            if (adapterDeviceId.compare(parsec_vdd::VDD_DISPLAY_ID) == 0) {
                continue;
            }
            if (!displayMap.count(devicePath)) {

                VirtualDisplay::DisplayInfo d;
                d.active      = !!(ddMonitor.StateFlags & DISPLAY_DEVICE_ACTIVE);
                d.displayUid     = ParseDisplayAddress(devicePath);
                std::wstring wDeviceName(ddAdapter.DeviceName);
                d.deviceName  = WideStringToString(wDeviceName);
                std::wstring wDeviceId(ddMonitor.DeviceID);
                std::string deviceId = WideStringToString(wDeviceId);
                d.displayName = ParseDisplayCode(deviceId);

                DEVINST devInst;
                if (GetDeviceInstance(devicePath, devInst)) {
                    GetDeviceLastArrival(devInst, d.lastArrival);
                    GetDeviceDescription(devInst, d.deviceDescription);
                }else{
                    std::cout << "Failed to get device instance for: " << deviceId << "\n";
                }


                DEVMODEW dm = {};
                dm.dmSize = sizeof(dm);
                VirtualDisplay::DisplayConfig config = VirtualDisplay::DisplayConfig();

                if (EnumDisplaySettingsW(ddAdapter.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                    config.width = dm.dmPelsWidth;
                    config.height = dm.dmPelsHeight;
                    config.refresh_rate = dm.dmDisplayFrequency;
                    config.bit_depth = dm.dmBitsPerPel;
                } 

               
                std::cout << "=== This is display  ------------------ ===\n";
                std::cout << "  Display ID: " << d.displayUid << "\n";
                std::cout << "  Device Name: " << d.deviceName << "\n";
                std::cout << "  Display Name: " << d.displayName << "\n";
                std::cout << "  Device Description: " << d.deviceDescription << "\n";
                std::cout << "  Last Arrival: " << FileTimeToString(d.lastArrival) << "\n";
                std::cout << "  Active: " << (d.active ? "Yes" : "No") << "\n";

                std::cout << "  Display Config: " 
                          << config.width << "x" 
                          << config.height << "@" 
                          << config.refresh_rate << "Hz, "
                          << config.bit_depth << " bits\n";
                std::cout << "=== End ===\n";
                
                std::unique_ptr<VirtualDisplay> display = std::make_unique<VirtualDisplay>(config, d);
                display->SetDisplayId(d.displayUid - 256);
                displayMap[devicePath] = std::move(display);
                paths.erase(paths.begin() + pathIdx);
            }
        }
    }


    displays_.clear();
    
    std::vector<std::pair<std::string, std::unique_ptr<VirtualDisplay>>> sortedDisplays;
    for (auto& pair : displayMap) {
        sortedDisplays.push_back(std::make_pair(pair.first, std::move(pair.second)));
    }

    std::sort(sortedDisplays.begin(), sortedDisplays.end(), 
        [](const auto& a, const auto& b) {
            const FILETIME& ftA = a.second->GetDisplayInfo().lastArrival;
            const FILETIME& ftB = b.second->GetDisplayInfo().lastArrival;
            

            ULARGE_INTEGER uliA, uliB;
            uliA.LowPart = ftA.dwLowDateTime;
            uliA.HighPart = ftA.dwHighDateTime;
            uliB.LowPart = ftB.dwLowDateTime;
            uliB.HighPart = ftB.dwHighDateTime;
            
            return uliA.QuadPart < uliB.QuadPart;  
        });
    

    for (auto& pair : sortedDisplays) {
        displays_.push_back(std::move(pair.second));
    }
    
    std::cout << "Total displays loaded: " << displays_.size() << " (sorted by arrival time)\n";
    return static_cast<int>(displays_.size());
}

bool VirtualDisplayControl::CheckVddStatus() {
    parsec_vdd::DeviceStatus status = parsec_vdd::QueryDeviceStatus(&parsec_vdd::VDD_CLASS_GUID, parsec_vdd::VDD_HARDWARE_ID);
    return (status == parsec_vdd::DEVICE_OK);
}

bool VirtualDisplayControl::ChangeDisplaySettings(int display_index, const VirtualDisplay::DisplayConfig& config) {
    if (display_index < 0 || display_index >= static_cast<int>(displays_.size())) {
        std::cout << "Error: Invalid display index" << std::endl;
        return false;
    }
    
    auto& display = displays_[display_index];
    if (!display) {
        std::cout << "Error: Display not found" << std::endl;
        return false;
    }
    
    if (!display->ChangeDisplaySettings(config)) {
        std::cout << "Error: Failed to change display settings" << std::endl;
        return false;
    }
    
    std::cout << "Display settings changed for index: " << display_index << std::endl;
    return true;
}


std::vector<VirtualDisplayControl::DetailedDisplayInfo> VirtualDisplayControl::GetDetailedDisplayList() {
    std::cout << "=== GetDetailedDisplayList called ===" << std::endl;
    std::vector<DetailedDisplayInfo> result;
    VirtualDisplayControl::GetAllDisplays();
    
    std::cout << "Total managed displays: " << displays_.size() << std::endl;
    
    // First, add all managed virtual displays from displays_ array
    for (size_t i = 0; i < displays_.size(); i++) {
        const auto& display = displays_[i];
        if (display) {
            DetailedDisplayInfo info;
            info.index = static_cast<int>(i);
            
            // Copy config data
            auto config = display->GetConfig();
            info.width = config.width;
            info.height = config.height;
            info.refresh_rate = config.refresh_rate;
            info.bit_depth = config.bit_depth;
            
            // Copy display info data
            auto displayInfo = display->GetDisplayInfo();
            info.active = displayInfo.active;
            info.display_uid = display->GetDisplayId();
            info.device_name = displayInfo.deviceName;
            info.display_name = displayInfo.displayName;
            info.device_description = displayInfo.deviceDescription;
            info.last_arrival = displayInfo.lastArrival;
            
            // Set additional fields
            info.is_virtual = true;  // These are managed virtual displays
            info.display_id = display->GetDisplayId();
            
            std::cout << "Display[" << i << "]:" << std::endl;
            std::cout << "  Resolution: " << info.width << "x" << info.height << "@" << info.refresh_rate << "Hz" << std::endl;
            std::cout << "  Display ID: " << info.display_id << std::endl;
            std::cout << "  Device Name: " << info.device_name << std::endl;
            std::cout << "  Display Name: " << info.display_name << std::endl;
            std::cout << "  Active: " << (info.active ? "Yes" : "No") << std::endl;
            std::cout << "  Is Virtual: " << (info.is_virtual ? "Yes" : "No") << std::endl;
            
            result.push_back(info);
        } else {
            std::cout << "Display[" << i << "]: null pointer" << std::endl;
        }
    }

    std::cout << "Returning " << result.size() << " displays" << std::endl;
    std::cout << "=== GetDetailedDisplayList end ===" << std::endl;
    return result;
}


