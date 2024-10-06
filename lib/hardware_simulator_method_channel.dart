import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'hardware_simulator_platform_interface.dart';

/// An implementation of [HardwareSimulatorPlatform] that uses method channels.
class MethodChannelHardwareSimulator extends HardwareSimulatorPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('hardware_simulator');
  bool isinitialized = false;

  void init() {
    methodChannel.setMethodCallHandler((call) async {
      if (call.method == "onCursorMoved") {
        for (var callback in _callbacks) {
          callback(call.arguments['dx'], call.arguments['dy']);
        }
      }
      return null;
    });
    isinitialized = true;
  }

  @override
  Future<String?> getPlatformVersion() async {
    final version =
        await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }

  @override
  Future<void> lockCursor() async {
    await methodChannel.invokeMethod('lockCursor');
  }

  @override
  Future<void> unlockCursor() async {
    // if not implemented, just care about main monitor.
    await methodChannel.invokeMethod('unlockCursor');
  }

  @override
  Future<int?> getMonitorCount() async {
    final count = await methodChannel.invokeMethod('getMonitorCount');
    return count;
  }

  final List<CursorMovedCallback> _callbacks = [];

  @override
  void addCursorMoved(CursorMovedCallback callback) {
    if (!isinitialized) init();
    _callbacks.add(callback);
  }

  @override
  void removeCursorMoved(CursorMovedCallback callback) {
    _callbacks.remove(callback);
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
      'screenId': screenId,
    });
  }

  // Absolute mouse movement. x, y is the percentage of the screen ranged from 0 - 1.
  @override
  Future<void> performMouseMoveAbsl(
      double percentx, double percenty, int screenId) async {
    await methodChannel.invokeMethod('mouseMoveA', {
      'x': percentx,
      'y': percenty,
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
