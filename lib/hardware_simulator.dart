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

  void performMouseMoveAbsl(double x, double y, int screenId) {
    HardwareSimulatorPlatform.instance.performMouseMoveAbsl(x, y, screenId);
  }

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

  HWKeyboard getKeyboard() {
    return keyboard;
  }

  HWMouse getMouse() {
    return mouse;
  }
}
