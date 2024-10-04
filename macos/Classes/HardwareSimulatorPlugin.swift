import Cocoa
import FlutterMacOS

public class HardwareSimulatorPlugin: NSObject, FlutterPlugin {
  private var methodChannel: FlutterMethodChannel?

  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "hardware_simulator", binaryMessenger: registrar.messenger)
    let instance = HardwareSimulatorPlugin()
    instance.methodChannel = channel
    registrar.addMethodCallDelegate(instance, channel: channel)
  }

  func performMouseMoveAbsl(x: Double, y: Double, screenId: Int) {
      let screens = NSScreen.screens
      guard screens.indices.contains(screenId) else {
          print("Invalid screenId: \(screenId)")
          return
      }

      let targetScreen = screens[screenId]
      let screenFrame = targetScreen.frame
      
      let absoluteX = x * Double(screenFrame.width) + Double(screenFrame.minX)

      let heightFactor = y * Double(screenFrame.height)
      let yOffset = Double(screenFrame.minY)
      let screenDiff = Double(screenFrame.height) - Double(screens[0].frame.height)
      // Haichao: It seems this is how macos works for Y. I don't know why it is so strange.
      let absoluteY = heightFactor - (yOffset + screenDiff)
      let moveEvent = CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: CGPoint(x: absoluteX, y: absoluteY), mouseButton: .left)

      moveEvent?.post(tap: .cghidEventTap)
  }

  func performMouseMoveRelative(dx: Double, dy: Double, screenId: Int) {
      let screens = NSScreen.screens
      guard screens.indices.contains(screenId) else {
          print("Invalid screenId: \(screenId)")
          return
      }

      let targetScreen = screens[screenId]
      let screenFrame = targetScreen.frame

      // Get the current mouse location
      if let currentMouseLocation = CGEvent(source: nil)?.location {
          // Calculate new position based on dx and dy
          let newX = currentMouseLocation.x + CGFloat(dx)
          let newY = currentMouseLocation.y + CGFloat(dy)

          // Ensure the new coordinates are within the screen's bounds
          let clampedX = min(max(newX, screenFrame.minX), screenFrame.maxX)
          let clampedY = min(max(newY, screenFrame.minY), screenFrame.maxY)

          // Create and post the event to move the mouse
          let moveEvent = CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: CGPoint(x: clampedX, y: clampedY), mouseButton: .left)
          moveEvent?.post(tap: .cghidEventTap)
      }
  }

  var monitor: Any?
    
  // 监听鼠标移动并解耦鼠标和光标
  func startTrackingMouse() {
      monitor = NSEvent.addLocalMonitorForEvents(matching: [.mouseMoved, .leftMouseDragged, .rightMouseDragged]) { event -> NSEvent? in
          self.handleMouseMove(event)
          return event
      }
  }
  
  // 停止监听
  func stopTrackingMouse() {
      if let monitor = monitor {
          NSEvent.removeMonitor(monitor)
      }
  }
  
  // 处理鼠标移动事件
  private func handleMouseMove(_ event: NSEvent) {
      let deltaX = event.deltaX
      let deltaY = event.deltaY
      
      methodChannel?.invokeMethod("onCursorMoved", arguments: [
          "dx": deltaX,
          "dy": deltaY,
      ])
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getPlatformVersion":
      result("macOS " + ProcessInfo.processInfo.operatingSystemVersionString)
    case "getMonitorCount":
      result(NSScreen.screens.count)
    case "mouseMoveA":
      if let args = call.arguments as? [String: Any],
        let percentX = args["x"] as? Double,
        let percentY = args["y"] as? Double,
        let screenId = args["screenId"] as? Int {
        // 调用鼠标移动函数
        performMouseMoveAbsl(x: percentX, y: percentY, screenId:screenId)
        result(nil) // 表示成功执行，不返回值
      } else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments", details: nil))
      }
    case "mouseMoveR":
      if let args = call.arguments as? [String: Any],
        let percentX = args["x"] as? Double,
        let percentY = args["y"] as? Double,
        let screenId = args["screenId"] as? Int {
        // 调用鼠标移动函数
        performMouseMoveRelative(dx: percentX, dy: percentY, screenId:screenId)
        result(nil) // 表示成功执行，不返回值
      } else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments", details: nil))
      }
    case "lockCursor":
      CGAssociateMouseAndMouseCursorPosition(0)
      NSCursor.hide()
      startTrackingMouse()
      result(nil)
    case "unlockCursor":
      CGAssociateMouseAndMouseCursorPosition(1)
      NSCursor.unhide()
      stopTrackingMouse()
      result(nil)
    default:
      print("Hardware Simulator: Method called but not implemented: \(call.method)")
      if let arguments = call.arguments {
          print("Arguments: \(arguments)")
      } else {
          print("No arguments provided.")
      }
      result(nil);
    }
  }
}
