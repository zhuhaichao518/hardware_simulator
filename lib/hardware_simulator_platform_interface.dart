import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'hardware_simulator_method_channel.dart';

typedef CursorMovedCallback = void Function(double x, double y);
typedef CursorPressedCallback = void Function(int button, bool isDown);
typedef CursorWheelCallback = void Function(double deltaX, double deltaY);
typedef CursorImageUpdatedCallback = void Function(
    int message, int messageInfo, Uint8List cursorImage);

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

  Future<bool> registerService() async {
    return false;
  }

  Future<void> unregisterService() async {
    return;
  }

  Future<bool> isRunningAsSystem() async {
    return true;
  }

  Future<void> showNotification(String content) async {
    return;
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
    throw UnimplementedError('removeCursorMoved() has not been implemented.');
  }

  void addCursorPressed(CursorPressedCallback callback) {
    throw UnimplementedError('addCursorPressed() has not been implemented.');
  }

  void removeCursorPressed(CursorPressedCallback callback) {
    throw UnimplementedError('removeCursorPressed() has not been implemented.');
  }

  void addCursorWheel(CursorWheelCallback callback) {
    throw UnimplementedError('addCursorWheel() has not been implemented.');
  }

  void removeCursorWheel(CursorWheelCallback callback) {
    throw UnimplementedError('removeCursorWheel() has not been implemented.');
  }

  void addCursorImageUpdated(
      CursorImageUpdatedCallback callback, int callbackId) {
    throw UnimplementedError(
        'addCursorImageUpdated() has not been implemented.');
  }

  void removeCursorImageUpdated(int callbackId) {
    throw UnimplementedError(
        'removeCursorImageUpdated() has not been implemented.');
  }

  Future<void> performKeyEvent(int keyCode, bool isDown) async {
    throw UnimplementedError('performKeyEvent() has not been implemented.');
  }

  // Relative mouse movement.
  Future<void> performMouseMoveRelative(
      double deltax, double deltay, int screenId) async {
    throw UnimplementedError(
        'performMouseMoveRelative() has not been implemented.');
  }

  // Absolute mouse movement. x, y is the percentage of the screen ranged from 0 - 1.
  Future<void> performMouseMoveAbsl(
      double percentx, double percenty, int screenId) async {
    throw UnimplementedError(
        'performMouseMoveAbsl() has not been implemented.');
  }

  Future<void> performMouseClick(int buttonId, bool isDown) async {
    throw UnimplementedError('performMouseClick() has not been implemented.');
  }

  Future<void> performMouseScroll(double dx, double dy) async {
    throw UnimplementedError('performMouseScroll() has not been implemented.');
  }

  // Touch event simulation
  Future<void> performTouchEvent(double x, double y, int touchId, bool isDown) async {
    throw UnimplementedError('performTouchEvent() has not been implemented.');
  }

  Future<void> performTouchMove(double x, double y, int touchId) async {
    throw UnimplementedError('performTouchMove() has not been implemented.');
  }

  Future<int> createGameController() async {
    throw UnimplementedError(
        'createGameController() has not been implemented.');
  }

  Future<void> removeGameController(int controllerId) async {
    throw UnimplementedError(
        'removeGameController() has not been implemented.');
  }

  Future<void> doControllerAction(int controllerId, String action) async {
    throw UnimplementedError(
        'removeGameController() has not been implemented.');
  }
}
