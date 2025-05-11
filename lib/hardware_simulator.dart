import 'hardware_simulator_platform_interface.dart';
import 'package:flutter/services.dart';

class HWKeyboard {
  HWKeyboard();
  void performKeyEvent(int keyCode, bool isDown) {
    HardwareSimulatorPlatform.instance.performKeyEvent(keyCode, isDown);
  }
}

class HWMouse {
  HWMouse();
  void performMouseMoveRelative(double deltax, double deltay, int screenId) {
    HardwareSimulatorPlatform.instance
        .performMouseMoveRelative(deltax, deltay, screenId);
  }

  void performMouseMoveAbsl(double percentx, double percenty, int screenId) {
    HardwareSimulatorPlatform.instance
        .performMouseMoveAbsl(percentx, percenty, screenId);
  }

  // mouse left button id 1, right button id 3
  void performMouseClick(int buttonId, bool isDown) {
    HardwareSimulatorPlatform.instance.performMouseClick(buttonId, isDown);
  }

  void performMouseScroll(double dx, double dy) {
    HardwareSimulatorPlatform.instance.performMouseScroll(dx, dy);
  }
}

class GameController {
  int controllerId;

  GameController(this.controllerId);

  static Future<GameController?> createGameController() async {
    int id = await HardwareSimulatorPlatform.instance.createGameController();
    // creation failed.
    if (id < 0) return null;
    return GameController(id);
  }

  Future<void> dispose() async {
    if (controllerId < 0) return;
    await HardwareSimulatorPlatform.instance.removeGameController(controllerId);
  }

  Future<void> simulate(String action) async {
    HardwareSimulatorPlatform.instance.doControllerAction(controllerId, action);
  }
}

class HardwareSimulator {
  static const MethodChannel _channel = MethodChannel('hardware_simulator');

  //For keyboard and mouse, it is a singleton. For other hardwares like game controllers,
  //it should be plugged in and then used.
  static HWKeyboard keyboard = HWKeyboard();
  static HWMouse mouse = HWMouse();

  static bool cursorlocked = false;

  Future<String?> getPlatformVersion() {
    return HardwareSimulatorPlatform.instance.getPlatformVersion();
  }

  static Future<int?> getMonitorCount() async {
    return HardwareSimulatorPlatform.instance.getMonitorCount();
  }

  static Future<bool> registerService() {
    return HardwareSimulatorPlatform.instance.registerService();
  }

  static Future<void> unregisterService() {
    return HardwareSimulatorPlatform.instance.unregisterService();
  }

  static Future<bool?> isRunningAsSystem() {
    return HardwareSimulatorPlatform.instance.isRunningAsSystem();
  }

  static Future<void> showNotification(String content) {
    return HardwareSimulatorPlatform.instance.showNotification(content);
  }

  static Future<void> lockCursor() async {
    cursorlocked = true;
    return HardwareSimulatorPlatform.instance.lockCursor();
  }

  static Future<void> unlockCursor() async {
    if (!cursorlocked) return;
    cursorlocked = false;
    return HardwareSimulatorPlatform.instance.unlockCursor();
  }

  static void addCursorMoved(CursorMovedCallback callback) {
    HardwareSimulatorPlatform.instance.addCursorMoved(callback);
  }

  static void removeCursorMoved(CursorMovedCallback callback) {
    HardwareSimulatorPlatform.instance.removeCursorMoved(callback);
  }

  static void addCursorPressed(CursorPressedCallback callback) {
    HardwareSimulatorPlatform.instance.addCursorPressed(callback);
  }

  static void removeCursorPressed(CursorPressedCallback callback) {
    HardwareSimulatorPlatform.instance.removeCursorPressed(callback);
  }

  static void addCursorWheel(CursorWheelCallback callback) {
    HardwareSimulatorPlatform.instance.addCursorWheel(callback);
  }

  static void removeCursorWheel(CursorWheelCallback callback) {
    HardwareSimulatorPlatform.instance.removeCursorWheel(callback);
  }

  // ignore: constant_identifier_names
  static const int CURSOR_INVISIBLE = 1;
  // ignore: constant_identifier_names
  static const int CURSOR_VISIBLE = 2;
  // ignore: constant_identifier_names
  static const int CURSOR_UPDATED_DEFAULT = 3;
  // ignore: constant_identifier_names
  static const int CURSOR_UPDATED_IMAGE = 4;
  // ignore: constant_identifier_names
  static const int CURSOR_UPDATED_CACHED = 5;

  static void addCursorImageUpdated(
      CursorImageUpdatedCallback callback, int callbackId) {
    HardwareSimulatorPlatform.instance
        .addCursorImageUpdated(callback, callbackId);
  }

  static void removeCursorImageUpdated(int callbackId) {
    HardwareSimulatorPlatform.instance.removeCursorImageUpdated(callbackId);
  }

  HWKeyboard getKeyboard() {
    return HWKeyboard();
  }

  HWMouse getMouse() {
    return HWMouse();
  }

  static void performTouchEvent(double x, double y, int touchId, bool isDown, int screenId) {
    HardwareSimulatorPlatform.instance.performTouchEvent(x, y, touchId, isDown, screenId);
  }

  static void performTouchMove(double x, double y, int touchId, int screenId) {
    HardwareSimulatorPlatform.instance.performTouchMove(x, y, touchId, screenId);
  }

  static Future<GameController?> createGameController() {
    return GameController.createGameController();
  }
}
