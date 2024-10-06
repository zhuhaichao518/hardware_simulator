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

  void performMouseScroll(int direction, int delta) {
    HardwareSimulatorPlatform.instance.performMouseScroll(direction, delta);
  }
}

class HardwareSimulator {
  //For keyboard and mouse, it is a singleton. For other hardwares like game controllers,
  //it should be plugged in and then used.
  static HWKeyboard keyboard = HWKeyboard();
  static HWMouse mouse = HWMouse();

  Future<String?> getPlatformVersion() {
    return HardwareSimulatorPlatform.instance.getPlatformVersion();
  }

  static Future<int?> getMonitorCount() async {
    return HardwareSimulatorPlatform.instance.getMonitorCount();
  }

  static Future<void> lockCursor() async {
    return HardwareSimulatorPlatform.instance.lockCursor();
  }

  static Future<void> unlockCursor() async {
    return HardwareSimulatorPlatform.instance.unlockCursor();
  }

  static void addCursorMoved(CursorMovedCallback callback) {
    HardwareSimulatorPlatform.instance.addCursorMoved(callback);
  }

  static void removeCursorMoved(CursorMovedCallback callback) {
    HardwareSimulatorPlatform.instance.removeCursorMoved(callback);
  }

  HWKeyboard getKeyboard() {
    return keyboard;
  }

  HWMouse getMouse() {
    return mouse;
  }
}
