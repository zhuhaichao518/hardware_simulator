import 'package:flutter_test/flutter_test.dart';
import 'package:hardware_simulator/hardware_simulator.dart';
import 'package:hardware_simulator/hardware_simulator_platform_interface.dart';
import 'package:hardware_simulator/hardware_simulator_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockHardwareSimulatorPlatform
    with MockPlatformInterfaceMixin
    implements HardwareSimulatorPlatform {
  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

void main() {
  final HardwareSimulatorPlatform initialPlatform =
      HardwareSimulatorPlatform.instance;

  test('$MethodChannelHardwareSimulator is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelHardwareSimulator>());
  });

  test('getPlatformVersion', () async {
    HardwareSimulator hardwareSimulatorPlugin = HardwareSimulator();
    MockHardwareSimulatorPlatform fakePlatform =
        MockHardwareSimulatorPlatform();
    HardwareSimulatorPlatform.instance = fakePlatform;

    expect(await hardwareSimulatorPlugin.getPlatformVersion(), '42');
  });
}
