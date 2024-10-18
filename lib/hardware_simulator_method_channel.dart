import 'dart:io';

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
        for (var callback in cursorMovedCallbacks) {
          callback(call.arguments['dx'], call.arguments['dy']);
        }
      }
      if (call.method == "onCursorImageMessage") {
        int callbackID = call.arguments['callbackID'];
        if (cursorImageCallbacks.containsKey(callbackID)) {
          cursorImageCallbacks[callbackID]!(call.arguments['message'],
              call.arguments['msg_info'], call.arguments['cursorImage']);
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
    if (kIsWeb || Platform.isAndroid || Platform.isIOS) return 1;
    final count = await methodChannel.invokeMethod('getMonitorCount');
    return count;
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

  final Map<int, CursorImageUpdatedCallback> cursorImageCallbacks = {};

  @override
  void addCursorImageUpdated(
      CursorImageUpdatedCallback callback, int callbackId) {
    if (kIsWeb || Platform.isIOS || Platform.isAndroid) {
      return;
    }
    if (!isinitialized) init();
    cursorImageCallbacks[callbackId] = callback;
    methodChannel.invokeMethod('hookCursorImage', {
      'callbackID': callbackId,
    });
  }

  @override
  void removeCursorImageUpdated(int callbackId) {
    if (cursorImageCallbacks.containsKey(callbackId)) {
      cursorImageCallbacks.remove(callbackId);
    }
    methodChannel.invokeMethod('unhookCursorImage', {
      'callbackID': callbackId,
    });
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
      'buttonId': buttonId,
      'isDown': isDown,
    });
  }

  @override
  Future<void> performMouseScroll(double dx, double dy) async {
    await methodChannel.invokeMethod('mouseScroll', {
      'dx': dx,
      'dy': dy,
    });
  }
}
