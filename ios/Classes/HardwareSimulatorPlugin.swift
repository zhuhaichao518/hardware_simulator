import Flutter
import UIKit
import GameController

public class HardwareSimulatorPlugin: NSObject, FlutterPlugin {
  private var channel: FlutterMethodChannel?
  private var mouseConnectObserver: NSObjectProtocol?
  private var mouseDisconnectObserver: NSObjectProtocol?
  private var accumulatedDeltaX: Float = 0
  private var accumulatedDeltaY: Float = 0
  
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "hardware_simulator", binaryMessenger: registrar.messenger())
    let instance = HardwareSimulatorPlugin()
    instance.channel = channel
    registrar.addMethodCallDelegate(instance, channel: channel)
    
    instance.startTrackingMouse()
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getPlatformVersion":
      result("iOS " + UIDevice.current.systemVersion)
    case "addCursorMoved":
      result(nil)
    case "addCursorButton":
      result(nil)
    case "addCursorScroll":
      result(nil)
    default:
      result(FlutterMethodNotImplemented)
    }
  }
  
  private func startTrackingMouse() {
    if #available(iOS 14.0, *) {
      mouseConnectObserver = NotificationCenter.default.addObserver(
        forName: .GCMouseDidConnect,
        object: nil,
        queue: .main
      ) { [weak self] notification in
        if let mouse = notification.object as? GCMouse {
          self?.registerMouseCallbacks(mouse)
        }
      }
      
      mouseDisconnectObserver = NotificationCenter.default.addObserver(
        forName: .GCMouseDidDisconnect,
        object: nil,
        queue: .main
      ) { [weak self] notification in
        if let mouse = notification.object as? GCMouse {
          self?.unregisterMouseCallbacks(mouse)
        }
      }
      
      for mouse in GCMouse.mice() {
        registerMouseCallbacks(mouse)
      }
    }
  }
  
  @available(iOS 14.0, *)
  private func registerMouseCallbacks(_ mouse: GCMouse) {
    guard let mouseInput = mouse.mouseInput else { return }
    
    mouseInput.mouseMovedHandler = { [weak self] mouse, deltaX, deltaY in
      guard let self = self else { return }
      self.channel?.invokeMethod("onCursorMoved", arguments: [
        "dx": deltaX,
        "dy": -deltaY
      ])
    }
    
    mouseInput.leftButton.pressedChangedHandler = { [weak self] button, value, pressed in
      self?.channel?.invokeMethod("onCursorButton", arguments: [
        "buttonId": 1,  // 左键
        "isDown": pressed
      ])
    }
    
    mouseInput.middleButton?.pressedChangedHandler = { [weak self] button, value, pressed in
      self?.channel?.invokeMethod("onCursorButton", arguments: [
        "buttonId": 2,  // 中键
        "isDown": pressed
      ])
    }
    
    mouseInput.rightButton?.pressedChangedHandler = { [weak self] button, value, pressed in
      self?.channel?.invokeMethod("onCursorButton", arguments: [
        "buttonId": 3,  // 右键
        "isDown": pressed
      ])
    }
    
    mouseInput.scroll.yAxis.valueChangedHandler = { [weak self] axis, value in
      self?.channel?.invokeMethod("onCursorScroll", arguments: [
        "dx": 0,
        "dy": Double(value)
      ])
    }
    
    mouseInput.scroll.xAxis.valueChangedHandler = { [weak self] axis, value in
      self?.channel?.invokeMethod("onCursorScroll", arguments: [
        "dx": Double(value),
        "dy": 0
      ])
    }
  }
  
  @available(iOS 14.0, *)
  private func unregisterMouseCallbacks(_ mouse: GCMouse) {
    guard let mouseInput = mouse.mouseInput else { return }
    mouseInput.mouseMovedHandler = nil
    mouseInput.leftButton.pressedChangedHandler = nil
    mouseInput.middleButton?.pressedChangedHandler = nil
    mouseInput.rightButton?.pressedChangedHandler = nil
    mouseInput.scroll.yAxis.valueChangedHandler = nil
    mouseInput.scroll.xAxis.valueChangedHandler = nil
  }
}
