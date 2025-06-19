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
  
  @override
  void addCursorImageUpdated(CursorImageUpdatedCallback callback, int callbackId, bool hookAll) {
    // TODO: implement addCursorImageUpdated
  }
  
  @override
  void addCursorMoved(CursorMovedCallback callback) {
    // TODO: implement addCursorMoved
  }
  
  @override
  void addCursorPressed(CursorPressedCallback callback) {
    // TODO: implement addCursorPressed
  }
  
  @override
  void addCursorWheel(CursorWheelCallback callback) {
    // TODO: implement addCursorWheel
  }
  
  @override
  Future<int> createGameController() {
    // TODO: implement createGameController
    throw UnimplementedError();
  }
  
  @override
  Future<void> doControllerAction(int controllerId, String action) {
    // TODO: implement doControllerAction
    throw UnimplementedError();
  }
  
  @override
  Future<int?> getMonitorCount() {
    // TODO: implement getMonitorCount
    throw UnimplementedError();
  }
  
  @override
  Future<bool> isRunningAsSystem() {
    // TODO: implement isRunningAsSystem
    throw UnimplementedError();
  }
  
  @override
  Future<void> lockCursor() {
    // TODO: implement lockCursor
    throw UnimplementedError();
  }
  
  @override
  Future<void> performKeyEvent(int keyCode, bool isDown) {
    // TODO: implement performKeyEvent
    throw UnimplementedError();
  }
  
  @override
  Future<void> performMouseClick(int buttonId, bool isDown) {
    // TODO: implement performMouseClick
    throw UnimplementedError();
  }
  
  @override
  Future<void> performMouseMoveAbsl(double percentx, double percenty, int screenId) {
    // TODO: implement performMouseMoveAbsl
    throw UnimplementedError();
  }
  
  @override
  Future<void> performMouseMoveRelative(double deltax, double deltay, int screenId) {
    // TODO: implement performMouseMoveRelative
    throw UnimplementedError();
  }
  
  @override
  Future<void> performMouseScroll(double dx, double dy) {
    // TODO: implement performMouseScroll
    throw UnimplementedError();
  }
  
  @override
  Future<void> performTouchEvent(double x, double y, int touchId, bool isDown, int screenId) {
    // TODO: implement performTouchEvent
    throw UnimplementedError();
  }
  
  @override
  Future<void> performTouchMove(double x, double y, int touchId, int screenId) {
    // TODO: implement performTouchMove
    throw UnimplementedError();
  }
  
  @override
  Future<bool> registerService() {
    // TODO: implement registerService
    throw UnimplementedError();
  }
  
  @override
  void removeCursorImageUpdated(int callbackId) {
    // TODO: implement removeCursorImageUpdated
  }
  
  @override
  void removeCursorMoved(CursorMovedCallback callback) {
    // TODO: implement removeCursorMoved
  }
  
  @override
  void removeCursorPressed(CursorPressedCallback callback) {
    // TODO: implement removeCursorPressed
  }
  
  @override
  void removeCursorWheel(CursorWheelCallback callback) {
    // TODO: implement removeCursorWheel
  }
  
  @override
  Future<void> removeGameController(int controllerId) {
    // TODO: implement removeGameController
    throw UnimplementedError();
  }
  
  @override
  Future<void> showNotification(String content) {
    // TODO: implement showNotification
    throw UnimplementedError();
  }
  
  @override
  Future<void> unlockCursor() {
    // TODO: implement unlockCursor
    throw UnimplementedError();
  }
  
  @override
  Future<void> unregisterService() {
    // TODO: implement unregisterService
    throw UnimplementedError();
  }
  
  @override
  void addKeyboardPressed(KeyboardPressedCallback callback) {
    // TODO: implement addKeyboardPressed
  }
  
  @override
  Future<bool?> getIsMouseConnected() {
    // TODO: implement getIsMouseConnected
    throw UnimplementedError();
  }
  
  @override
  void removeKeyboardPressed(KeyboardPressedCallback callback) {
    // TODO: implement removeKeyboardPressed
  }
}

void main() {
  final HardwareSimulatorPlatform initialPlatform = HardwareSimulatorPlatform.instance;

  test('$MethodChannelHardwareSimulator is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelHardwareSimulator>());
  });

  test('getPlatformVersion', () async {
    HardwareSimulator hardwareSimulatorPlugin = HardwareSimulator();
    MockHardwareSimulatorPlatform fakePlatform = MockHardwareSimulatorPlatform();
    HardwareSimulatorPlatform.instance = fakePlatform;

    expect(await hardwareSimulatorPlugin.getPlatformVersion(), '42');
  });
}
