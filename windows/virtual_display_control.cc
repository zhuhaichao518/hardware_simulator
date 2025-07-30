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
                return std::stoi(numberStr);
            } catch(const std::exception& e) {
                std::cout << "ParseDisplayAddress: Error parsing number: " << e.what() << std::endl;
            }
        }
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

static bool GetDeviceInstance(const std::string& deviceId, DEVINST& dev_inst) {
    std::wstring w_device_id = StringToWideString(deviceId);

    CONFIGRET cr = CM_Locate_DevNodeW(
        &dev_inst,
        const_cast<wchar_t*>(w_device_id.c_str()),
        0
    );
    
    return (cr == CR_SUCCESS);
}

static bool GetDeviceLastArrival(DEVINST dev_inst, FILETIME& last_arrival) {
    DEVPROPKEY key = DEVPKEY_Device_LastArrivalDate;
    DEVPROPTYPE propType;
    LONGLONG fileTimeValue;
    ULONG bufferSize = sizeof(LONGLONG);

    CONFIGRET cr = CM_Get_DevNode_PropertyW(
        dev_inst,
        &key,
        &propType,
        reinterpret_cast<PBYTE>(&fileTimeValue),
        &bufferSize,
        0
    );
    
    if (cr == CR_SUCCESS) {
        last_arrival.dwLowDateTime = (DWORD)(fileTimeValue & 0xFFFFFFFF);
        last_arrival.dwHighDateTime = (DWORD)(fileTimeValue >> 32);
        return true;
    }
    
    last_arrival.dwLowDateTime = 0;
    last_arrival.dwHighDateTime = 0;
    return false;
}

static bool GetParentDeviceInstance(DEVINST dev_inst, DEVINST& parent_inst, std::string& parent_id) {
    CONFIGRET cr = CM_Get_Parent(
        &parent_inst,
        dev_inst,
        0
    );
    if (cr != CR_SUCCESS) return false;

    wchar_t idBuffer[MAX_DEVICE_ID_LEN] = {0};
    cr = CM_Get_Device_IDW(
        parent_inst,
        idBuffer,
        MAX_DEVICE_ID_LEN,
        0
    );
    if (cr != CR_SUCCESS) return false;

    std::wstring w_parent_id(idBuffer);
    parent_id = WideStringToString(w_parent_id);
    return true;
}

static bool GetDeviceDescription(DEVINST dev_inst, std::string& description) {
    DEVPROPKEY key = DEVPKEY_Device_DeviceDesc;
    DEVPROPTYPE propType;
    wchar_t buffer[256] = {0};
    ULONG bufferSize = sizeof(buffer);

    CONFIGRET cr = CM_Get_DevNode_PropertyW(
        dev_inst,
        &key,
        &propType,
        reinterpret_cast<PBYTE>(buffer),
        &bufferSize,
        0
    );
    
    if (cr == CR_SUCCESS && propType == DEVPROP_TYPE_STRING) {
        std::wstring wDescription(buffer);
        description = WideStringToString(wDescription);
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
        parsec_vdd::VddRemoveDisplay(vdd_handle_, display->GetDisplayUid());
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
        std::ignore = VirtualDisplayControl::GetAllDisplays();
        return vdd_index;
    } else {
        std::cout << "Error: Failed to add display to VDD" << std::endl;
        return -1;
    }
}

bool VirtualDisplayControl::RemoveDisplay(int display_uid) {
    if (!initialized_ || vdd_handle_ == INVALID_HANDLE_VALUE) {
        std::cout << "Error: VirtualDisplayControl not initialized" << std::endl;
        return false;
    }

    std::cout << "get remove call with id" << display_uid << std::endl;
    
    auto it = std::find_if(displays_.begin(), displays_.end(), 
        [display_uid](const std::unique_ptr<VirtualDisplay>& display) {
            return display->GetDisplayUid() == display_uid;
        });
    
    if (it == displays_.end()) {
        std::cout << "Error: Display not found" << std::endl;
        return false;
    }

    if(it->get()->GetDisplayInfo().is_virtual == false) {
        std::cout << "Error: Cannot remove non-virtual display" << std::endl;
        return false;
    }
    
    parsec_vdd::VddRemoveDisplay(vdd_handle_, display_uid);
    displays_.erase(it);
    std::cout << "Virtual display removed: " << display_uid << std::endl;
    return true;
}

int VirtualDisplayControl::GetDisplayCount() {
    return static_cast<int>(displays_.size());
}

std::vector<int> VirtualDisplayControl::GetDisplayList() {
    std::vector<int> ids;
    for (const auto& display : displays_) {
        ids.push_back(display->GetDisplayUid());
    }
    return ids;
}

int VirtualDisplayControl::GetAllDisplays() {

    std::map <std::string, std::unique_ptr<VirtualDisplay>> displayMap;
    auto paths= GetDisplayPaths();

    DISPLAY_DEVICEW ddAdapter{}, ddMonitor{};
    ddAdapter.cb = sizeof(ddAdapter);

    for (int ai = 0; EnumDisplayDevicesW(nullptr, ai, &ddAdapter, 0); ++ai) {

        // std::cout << "__________________________________________---_______________________________" << std::endl;
        // std::cout << "Processing adapter: " << WideStringToString(ddAdapter.DeviceName) << "\n";
        // std::cout << "StateFlags: " << ddAdapter.StateFlags << "\n";
        // std::cout << "DeviceID: " << WideStringToString(ddAdapter.DeviceID) << "\n";



        ddMonitor = {}; ddMonitor.cb = sizeof(ddMonitor);
        for (int mi = 0;
             EnumDisplayDevicesW(ddAdapter.DeviceName, mi, &ddMonitor,
                                 EDD_GET_DEVICE_INTERFACE_NAME);
             ++mi)
        {
            // std::cout << "__________________________________________--display-_______________________________" << std::endl;
            // std::cout << "Processing monitor: " << WideStringToString(ddMonitor.DeviceName) << "\n";
            // std::cout << "StateFlags: " << ddMonitor.StateFlags << "\n";
            // std::cout << "DeviceID: " << WideStringToString(ddMonitor.DeviceID) << "\n";


            if (!(ddMonitor.StateFlags & DISPLAY_DEVICE_ATTACHED))
                continue;

            int path_idx = -1;
            std::string devicePath;
            for (int k = 0; k < (int)paths.size(); ++k) {
                std::string p = paths[k];
                std::replace(p.begin(), p.end(), '\\', '#');
                std::wstring w_device_id(ddMonitor.DeviceID);
                std::string device_id = WideStringToString(w_device_id);
                if (device_id.find(p) != std::string::npos) {
                    path_idx = k;
                    devicePath = paths[k];
                    break;
                }
            }
            if (path_idx < 0) 
                continue;


            if (!displayMap.count(devicePath)) {

                std::wstring w_device_id(ddMonitor.DeviceID);
                std::string device_id = WideStringToString(w_device_id);

                VirtualDisplay::DisplayInfo d;
                d.display_uid     = ParseDisplayAddress(devicePath);

                if (ParseDisplayCode(device_id).compare(parsec_vdd::VDD_DISPLAY_ID) == 0) {
                    d.is_virtual = true;
                } else {
                    d.is_virtual = false;
                }

                d.active      = !!(ddMonitor.StateFlags & DISPLAY_DEVICE_ACTIVE);
                std::wstring w_device_name(ddAdapter.DeviceName);
                d.device_name  = WideStringToString(w_device_name);
                d.display_name = ParseDisplayCode(device_id);

                DEVINST dev_inst;
                if (GetDeviceInstance(devicePath, dev_inst)) {
                    GetDeviceLastArrival(dev_inst, d.last_arrival);
                    GetDeviceDescription(dev_inst, d.device_description);
                }else{
                    std::cout << "Failed to get device instance for: " << device_id << "\n";
                }


                DEVMODEW dm = {};
                dm.dmSize = sizeof(dm);
                VirtualDisplay::DisplayConfig config = VirtualDisplay::DisplayConfig();

                if (EnumDisplaySettingsW(ddAdapter.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                    config.width = dm.dmPelsWidth;
                    config.height = dm.dmPelsHeight;
                    config.refresh_rate = dm.dmDisplayFrequency;
                } 

                std::unique_ptr<VirtualDisplay> display = std::make_unique<VirtualDisplay>(config, d);
                if(d.is_virtual) {
                    display->SetDisplayUid(d.display_uid - 256);
                } else { 
                    display->SetDisplayUid(d.display_uid);
                }

                displayMap[devicePath] = std::move(display);
               
                // std::cout << "=== This is display  ------------------ ===\n";
                // std::cout << "  Display ID: " << d.display_uid << "\n";
                // std::cout << "  Device Name: " << d.device_name << "\n";
                // std::cout << "  Display Name: " << d.display_name << "\n";
                // std::cout << "  Device Description: " << d.device_description << "\n";
                // std::cout << "  Last Arrival: " << FileTimeToString(d.last_arrival) << "\n";
                // std::cout << "  Active: " << (d.active ? "Yes" : "No") << "\n";

                // std::cout << "  Display Config: " 
                //           << config.width << "x" 
                //           << config.height << "@" 
                //           << config.refresh_rate << "Hz, ";
                // std::cout << "=== End ===\n";
        
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
            const FILETIME& ftA = a.second->GetDisplayInfo().last_arrival;
            const FILETIME& ftB = b.second->GetDisplayInfo().last_arrival;
            

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

bool VirtualDisplayControl::ChangeDisplaySettings(int display_uid, const VirtualDisplay::DisplayConfig& config) {
    auto it = std::find_if(displays_.begin(), displays_.end(),
        [display_uid](const std::unique_ptr<VirtualDisplay>& display) {
            return display && display->GetDisplayUid() == display_uid;
        });

    if (it == displays_.end()) {
        std::cout << "Error: Invalid display UID:" << display_uid << std::endl;
        return false;
    }

    auto& display = *it;
    if (!display) {
        std::cout << "Error: Display not found" << std::endl;
        return false;
    }
    
    if (!display->ChangeDisplaySettings(config)) {
        std::cout << "Error: Failed to change display settings" << std::endl;
        return false;
    }

    std::cout << "Display settings changed for UID: " << display_uid << std::endl;
    return true;
}


std::vector<VirtualDisplayControl::DetailedDisplayInfo> VirtualDisplayControl::GetDetailedDisplayList() {
    std::vector<DetailedDisplayInfo> result;
    VirtualDisplayControl::GetAllDisplays();
    
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
            
            // Copy display info data
            auto display_info = display->GetDisplayInfo();
            info.active = display_info.active;
            info.display_uid = display->GetDisplayUid();
            info.device_name = display_info.device_name;
            info.display_name = display_info.display_name;
            info.device_description = display_info.device_description;
            info.last_arrival = display_info.last_arrival;
            
            // Set additional fields
            info.is_virtual = display_info.is_virtual;  // These are managed virtual displays

            
            // std::cout << "Display[" << i << "]:" << std::endl;
            // std::cout << "  Resolution: " << info.width << "x" << info.height << "@" << info.refresh_rate << "Hz" << std::endl;
            // std::cout << "  Display UID: " << info.display_uid << std::endl;
            // std::cout << "  Device Name: " << info.device_name << std::endl;
            // std::cout << "  Display Name: " << info.display_name << std::endl;
            // std::cout << "  Active: " << (info.active ? "Yes" : "No") << std::endl;
            // std::cout << "  Is Virtual: " << (info.is_virtual ? "Yes" : "No") << std::endl;
            
            result.push_back(info);
        } else {
            std::cout << "Display[" << i << "]: null pointer" << std::endl;
        }
    }

    std::cout << "Returning " << result.size() << " displays" << std::endl;
    return result;
}

std::vector<VirtualDisplay::DisplayConfig> VirtualDisplayControl::GetDisplayConfigs(int display_uid) {
    auto it = std::find_if(displays_.begin(), displays_.end(),
        [display_uid](const std::unique_ptr<VirtualDisplay>& display) {
            return display && display->GetDisplayUid() == display_uid;
        });

    if (it == displays_.end()) {
        std::cout << "Display not found for UID: " << display_uid << std::endl;
        return {};
    }

    auto& display = *it;
    if (!display) {
        std::cout << "Display pointer is null" << std::endl;
        return {};
    }

    return display->GetDisplayConfigList();
}

std::vector<VirtualDisplay::DisplayConfig> VirtualDisplayControl::GetCustomDisplayConfigs() {
    std::vector<VirtualDisplay::DisplayConfig> configs;
    
    HKEY vdd_key;
    LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                                L"SOFTWARE\\Parsec\\vdd", 
                                0, 
                                KEY_READ, 
                                &vdd_key);
    
    if (result != ERROR_SUCCESS) {
        std::cout << "Failed to open Parsec VDD registry key" << std::endl;
        return configs;
    }
    
    for (int i = 0; i < 5; i++) {
        std::wstring subkey_name = std::to_wstring(i);
        HKEY index_key;
        
        result = RegOpenKeyExW(vdd_key, 
                               subkey_name.c_str(), 
                               0, 
                               KEY_READ, 
                               &index_key);
        
        if (result == ERROR_SUCCESS) {
            DWORD width, height, hz;
            DWORD data_size = sizeof(DWORD);
            
            LONG width_result = RegQueryValueExW(index_key, L"width", nullptr, nullptr, 
                                                 reinterpret_cast<BYTE*>(&width), &data_size);
            data_size = sizeof(DWORD);
            LONG height_result = RegQueryValueExW(index_key, L"height", nullptr, nullptr, 
                                                  reinterpret_cast<BYTE*>(&height), &data_size);
            data_size = sizeof(DWORD);
            LONG hz_result = RegQueryValueExW(index_key, L"hz", nullptr, nullptr, 
                                              reinterpret_cast<BYTE*>(&hz), &data_size);
            
            if (width_result == ERROR_SUCCESS && height_result == ERROR_SUCCESS && hz_result == ERROR_SUCCESS) {
                VirtualDisplay::DisplayConfig config;
                config.width = static_cast<int>(width);
                config.height = static_cast<int>(height);
                config.refresh_rate = static_cast<int>(hz);
                configs.push_back(config);
            }
            
            RegCloseKey(index_key);
        }
    }
    
    RegCloseKey(vdd_key);
    std::cout << "Retrieved " << configs.size() << " custom display configs from registry" << std::endl;
    return configs;
}

bool VirtualDisplayControl::SetCustomDisplayConfigs(const std::vector<VirtualDisplay::DisplayConfig>& configs) {
    HKEY vdd_key;
    LONG result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, 
                                  L"SOFTWARE\\Parsec\\vdd", 
                                  0, 
                                  nullptr, 
                                  REG_OPTION_NON_VOLATILE, 
                                  KEY_WRITE, 
                                  nullptr, 
                                  &vdd_key, 
                                  nullptr);
    
    if (result != ERROR_SUCCESS) {
        std::cout << "Failed to create/open Parsec VDD registry key. Admin rights required." << std::endl;
        return false;
    }
    
    for (int i = 0; i < 5; i++) {
        std::wstring subkey_name = std::to_wstring(i);
        
        if (i >= static_cast<int>(configs.size())) {
            result = RegDeleteKeyW(vdd_key, subkey_name.c_str());
            if (result == ERROR_SUCCESS) {
                std::cout << "Deleted registry subkey: " << i << std::endl;
            }
        } else {
            HKEY index_key;
            result = RegCreateKeyExW(vdd_key, 
                                     subkey_name.c_str(), 
                                     0, 
                                     nullptr, 
                                     REG_OPTION_NON_VOLATILE, 
                                     KEY_WRITE, 
                                     nullptr, 
                                     &index_key, 
                                     nullptr);
            
            if (result == ERROR_SUCCESS) {
                const auto& config = configs[i];
                DWORD width = static_cast<DWORD>(config.width);
                DWORD height = static_cast<DWORD>(config.height);
                DWORD hz = static_cast<DWORD>(config.refresh_rate);
                
                RegSetValueExW(index_key, L"width", 0, REG_DWORD, 
                               reinterpret_cast<const BYTE*>(&width), sizeof(DWORD));
                RegSetValueExW(index_key, L"height", 0, REG_DWORD, 
                               reinterpret_cast<const BYTE*>(&height), sizeof(DWORD));
                RegSetValueExW(index_key, L"hz", 0, REG_DWORD, 
                               reinterpret_cast<const BYTE*>(&hz), sizeof(DWORD));
                
                std::cout << "Set custom display config " << i << ": " 
                          << config.width << "x" << config.height << "@" << config.refresh_rate << "Hz" << std::endl;
                
                RegCloseKey(index_key);
            } else {
                std::cout << "Failed to create registry subkey: " << i << std::endl;
                RegCloseKey(vdd_key);
                return false;
            }
        }
    }
    
    RegCloseKey(vdd_key);
    std::cout << "Successfully set " << configs.size() << " custom display configs" << std::endl;
    return true;
}


