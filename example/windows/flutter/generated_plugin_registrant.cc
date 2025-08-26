//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <hardware_simulator/hardware_simulator_plugin_c_api.h>
#include <pointer_lock/pointer_lock_plugin_c_api.h>
#include <url_launcher_windows/url_launcher_windows.h>

void RegisterPlugins(flutter::PluginRegistry* registry) {
  HardwareSimulatorPluginCApiRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("HardwareSimulatorPluginCApi"));
  PointerLockPluginCApiRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("PointerLockPluginCApi"));
  UrlLauncherWindowsRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("UrlLauncherWindows"));
}
