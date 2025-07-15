#include "virtual_display_control.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>

bool VirtualDisplayControl::initialized_ = false;
HANDLE VirtualDisplayControl::vdd_handle_ = INVALID_HANDLE_VALUE;
std::vector<std::unique_ptr<VirtualDisplay>> VirtualDisplayControl::displays_;

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
            //如果中途vdd_handle_出问题了怎么办？
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
        auto display = std::make_unique<VirtualDisplay>();
        display->SetDisplayId(vdd_index);
        
        displays_.push_back(std::move(display));
        return vdd_index;
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
        auto display = std::make_unique<VirtualDisplay>();
        display->SetDisplayId(vdd_index);

        VirtualDisplay::DisplayConfig config(width, height, refresh_rate);
        display->ChangeDisplaySettings(config);
        
        displays_.push_back(std::move(display));
        return vdd_index;
    } else {
        std::cout << "Error: Failed to add display to VDD" << std::endl;
        return -1;
    }
}

bool VirtualDisplayControl::RemoveDisplay(int display_id) {
    if (!initialized_ || vdd_handle_ == INVALID_HANDLE_VALUE) {
        std::cout << "Error: VirtualDisplayControl not initialized" << std::endl;
        return false;
    }
    
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

std::string VirtualDisplayControl::GetVddInfo() {
    std::string info = "Parsec VDD Information:\n";
    info += "Display ID: " + std::string(parsec_vdd::VDD_DISPLAY_ID) + "\n";
    info += "Display Name: " + std::string(parsec_vdd::VDD_DISPLAY_NAME) + "\n";
    info += "Adapter Name: " + std::string(parsec_vdd::VDD_ADAPTER_NAME) + "\n";
    info += "Hardware ID: " + std::string(parsec_vdd::VDD_HARDWARE_ID) + "\n";
    
    if (initialized_ && vdd_handle_ != INVALID_HANDLE_VALUE) {
        int version = parsec_vdd::VddVersion(vdd_handle_);
        info += "Driver Version: " + std::to_string(version) + "\n";
        info += "Handle Status: Active\n";
    } else {
        info += "Handle Status: Inactive\n";
    }
    
    return info;
}

bool VirtualDisplayControl::CheckVddStatus() {
    parsec_vdd::DeviceStatus status = parsec_vdd::QueryDeviceStatus(&parsec_vdd::VDD_CLASS_GUID, parsec_vdd::VDD_HARDWARE_ID);
    return (status == parsec_vdd::DEVICE_OK);
}


