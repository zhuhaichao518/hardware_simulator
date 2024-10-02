import Cocoa
import FlutterMacOS

public class HardwareSimulatorPlugin: NSObject, FlutterPlugin {
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "hardware_simulator", binaryMessenger: registrar.messenger)
    let instance = HardwareSimulatorPlugin()
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

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getPlatformVersion":
      result("macOS " + ProcessInfo.processInfo.operatingSystemVersionString)
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
