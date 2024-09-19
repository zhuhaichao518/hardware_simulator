//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <hardware_simulator/hardware_simulator_plugin.h>

void fl_register_plugins(FlPluginRegistry* registry) {
  g_autoptr(FlPluginRegistrar) hardware_simulator_registrar =
      fl_plugin_registry_get_registrar_for_plugin(registry, "HardwareSimulatorPlugin");
  hardware_simulator_plugin_register_with_registrar(hardware_simulator_registrar);
}
