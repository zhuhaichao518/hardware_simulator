#include "include/hardware_simulator/hardware_simulator_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "hardware_simulator_plugin.h"

void HardwareSimulatorPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  hardware_simulator::HardwareSimulatorPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
