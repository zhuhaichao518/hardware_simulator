import 'hardware_simulator_platform_interface.dart';

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

class HardwareSimulator {
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
    return keyboard;
  }

  HWMouse getMouse() {
    return mouse;
  }
}
