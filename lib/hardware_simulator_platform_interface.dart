import 'dart:typed_data';

import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'hardware_simulator_method_channel.dart';

typedef CursorMovedCallback = void Function(double x, double y);
typedef CursorImageUpdatedCallback = void Function(int message, int messageInfo, Uint8List cursorImage);

abstract class HardwareSimulatorPlatform extends PlatformInterface {
  /// Constructs a HardwareSimulatorPlatform.
  HardwareSimulatorPlatform() : super(token: _token);

  static final Object _token = Object();

  static HardwareSimulatorPlatform _instance = MethodChannelHardwareSimulator();

  /// The default instance of [HardwareSimulatorPlatform] to use.
  ///
  /// Defaults to [MethodChannelHardwareSimulator].
  static HardwareSimulatorPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [HardwareSimulatorPlatform] when
  /// they register themselves.
  static set instance(HardwareSimulatorPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  Future<int?> getMonitorCount() async {
    // if not implemented, just care about main monitor.
    return 1;
  }

  Future<void> lockCursor() async {
    // if not implemented, just care about main monitor.
    print("lockCursor called but not supported.");
  }

  Future<void> unlockCursor() async {
    // if not implemented, just care about main monitor.
    print("unlockCursor called but not supported.");
  }

  void addCursorMoved(CursorMovedCallback callback) async {
    print("addCursorMoved called but not supported.");
  }

  void removeCursorMoved(CursorMovedCallback callback) {
    print("removeCursorMoved called but not supported.");
  }

  void addCursorImageUpdated(CursorImageUpdatedCallback callback, int callbackId) {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  void removeCursorImageUpdated(int callbackId) {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  Future<void> performKeyEvent(int keyCode, bool isDown) async {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  // Relative mouse movement.
  Future<void> performMouseMoveRelative(
      double deltax, double deltay, int screenId) async {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  // Absolute mouse movement. x, y is the percentage of the screen ranged from 0 - 1.
  Future<void> performMouseMoveAbsl(
      double percentx, double percenty, int screenId) async {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  Future<void> performMouseClick(int buttonId, bool isDown) async {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }

  // Direction 0:vertial 1:horizontal
  Future<void> performMouseScroll(int direction, int delta) async {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}
