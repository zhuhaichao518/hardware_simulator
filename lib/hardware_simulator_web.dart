// In order to *not* need this ignore, consider extracting the "web" version
// of your plugin as a separate package, instead of inlining it in the same
// package as the core of your plugin.
// ignore: avoid_web_libraries_in_flutter

import 'package:hardware_simulator/hardware_simulator_platform_interface.dart';
import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:web/web.dart' as web;
import 'dart:html';
import 'dart:js_util' as js_util;

/// A web implementation of the HardwareSimulatorPlatform of the HardwareSimulator plugin.
class HardwareSimulatorPluginWeb extends HardwareSimulatorPlatform {
  /// Constructs a HardwareSimulatorWeb
  HardwareSimulatorPluginWeb();

  static void registerWith(Registrar registrar) {
    HardwareSimulatorPlatform.instance = HardwareSimulatorPluginWeb();
  }

  /// Returns a [String] containing the version of the platform.
  @override
  Future<String?> getPlatformVersion() async {
    final version = web.window.navigator.userAgent;
    return version;
  }

  @override
  Future<void> lockCursor() async {
    js_util.callMethod(document.body!, 'requestPointerLock', []);
  }

  @override
  Future<void> unlockCursor() async {
    js_util.callMethod(document, 'exitPointerLock', []);
  }

  bool isinitialized = false;

  void init() {
    // 监听鼠标移动事件
    document.onMouseMove.listen((event) {
      // 使用 movementX 和 movementY 获取相对位移
      double dx = event.movement.x.toDouble();
      double dy = event.movement.y.toDouble();
      for (var callback in cursorMovedCallbacks){
        callback(dx,dy);
      }
    });
    isinitialized = true;
  }

  final List<CursorMovedCallback> cursorMovedCallbacks = [];

  @override
  void addCursorMoved(CursorMovedCallback callback) {
    if (!isinitialized) init();
    cursorMovedCallbacks.add(callback);
  }

  @override
  void removeCursorMoved(CursorMovedCallback callback) {
    cursorMovedCallbacks.remove(callback);
  }
}
