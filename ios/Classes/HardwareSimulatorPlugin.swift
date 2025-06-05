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
    //no need to stopTrackingMouse()
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getPlatformVersion":
      result("iOS " + UIDevice.current.systemVersion)
    default:
      result(FlutterMethodNotImplemented)
    }
  }
  
  private func startTrackingMouse() {
    if #available(iOS 14.0, *) {
      // 注册鼠标连接和断开连接的观察者
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
      
      // 为已连接的鼠标注册回调
      for mouse in GCMouse.mice() {
        registerMouseCallbacks(mouse)
      }
    }
  }
  
  private func stopTrackingMouse() {
    if #available(iOS 14.0, *) {
      if let observer = mouseConnectObserver {
        NotificationCenter.default.removeObserver(observer)
      }
      if let observer = mouseDisconnectObserver {
        NotificationCenter.default.removeObserver(observer)
      }
      
      for mouse in GCMouse.mice() {
        unregisterMouseCallbacks(mouse)
      }
    }
  }
  
  @available(iOS 14.0, *)
  private func registerMouseCallbacks(_ mouse: GCMouse) {
    guard let mouseInput = mouse.mouseInput else { return }
    
    mouseInput.mouseMovedHandler = { [weak self] mouse, deltaX, deltaY in
      guard let self = self else { return }
      
      self.accumulatedDeltaX += deltaX
      self.accumulatedDeltaY += -deltaY
      
      let truncatedDeltaX = Int16(self.accumulatedDeltaX)
      let truncatedDeltaY = Int16(self.accumulatedDeltaY)
      
      if truncatedDeltaX != 0 || truncatedDeltaY != 0 {
        self.channel?.invokeMethod("onCursorMoved", arguments: [
          "dx": truncatedDeltaX,
          "dy": truncatedDeltaY
        ])
        
        self.accumulatedDeltaX -= Float(truncatedDeltaX)
        self.accumulatedDeltaY -= Float(truncatedDeltaY)
      }
    }
  }
  
  @available(iOS 14.0, *)
  private func unregisterMouseCallbacks(_ mouse: GCMouse) {
    guard let mouseInput = mouse.mouseInput else { return }
    mouseInput.mouseMovedHandler = nil
  }
}
