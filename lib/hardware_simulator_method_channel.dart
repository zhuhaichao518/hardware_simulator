import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'hardware_simulator_platform_interface.dart';

/// An implementation of [HardwareSimulatorPlatform] that uses method channels.
class MethodChannelHardwareSimulator extends HardwareSimulatorPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('hardware_simulator');

  @override
  Future<String?> getPlatformVersion() async {
    final version =
        await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }

  @override
  Future<void> performKeyEvent(int keyCode, bool isDown) async {
    await methodChannel.invokeMethod('KeyPress', {
      'code': keyCode,
      'isDown': isDown,
    });
  }

  // Relative mouse movement.
  @override
  Future<void> performMouseMoveRelative(
      double deltax, double deltay, int screenId) async {
    await methodChannel.invokeMethod('mouseMoveR', {
      'x': deltax,
      'y': deltay,
      'screenid': screenId,
    });
  }

  // Absolute mouse movement. x, y is the percentage of the screen ranged from 0 - 1.
  @override
  Future<void> performMouseMoveAbsl(double x, double y, int screenId) async {
    await methodChannel.invokeMethod('mouseMoveA', {
      'x': x,
      'y': y,
      'screenId': screenId,
    });
  }

  @override
  Future<void> performMouseClick(int buttonId, bool isDown) async {
    await methodChannel.invokeMethod('mousePress', {
      'buttonid': buttonId,
      'isDown': isDown,
    });
  }

  // Direction 0:vertial 1:horizontal
  @override
  Future<void> performMouseScroll(int direction, int delta) async {
    await methodChannel.invokeMethod('mouseScroll', {
      'mousescroll': direction,
      'delta': delta,
    });
  }
}
