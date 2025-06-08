import Cocoa
import FlutterMacOS 
import CommonCrypto

class CursorConstants {    
  static let cursorUpdatedDefault = 3    
  static let cursorUpdatedImage = 4    
  static let cursorUpdatedCached = 5
}

public class HardwareSimulatorPlugin: NSObject, FlutterPlugin {
  private var methodChannel: FlutterMethodChannel?
  private var defaultCursorHasher: CursorHasher?

  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "hardware_simulator", binaryMessenger: registrar.messenger)
    let instance = HardwareSimulatorPlugin()
    instance.methodChannel = channel
    instance.defaultCursorHasher = CursorHasher()
    registrar.addMethodCallDelegate(instance, channel: channel)
  }

  // Reverse mapping: Windows code to macOS code
  let windowsToMacKeyMap: [Int: Int] = [
      0x41: 0x00, // A
      0x53: 0x01, // S
      0x44: 0x02, // D
      0x46: 0x03, // F
      0x48: 0x04, // H
      0x47: 0x05, // G
      0x5A: 0x06, // Z
      0x58: 0x07, // X
      0x43: 0x08, // C
      0x56: 0x09, // V
      0x42: 0x0B, // B
      0x51: 0x0C, // Q
      0x57: 0x0D, // W
      0x45: 0x0E, // E
      0x52: 0x0F, // R
      0x59: 0x10, // Y
      0x54: 0x11, // T
      0x31: 0x12, // 1
      0x32: 0x13, // 2
      0x33: 0x14, // 3
      0x34: 0x15, // 4
      0x36: 0x16, // 6
      0x35: 0x17, // 5
      0xBB: 0x18, // Equals
      0x39: 0x19, // 9
      0x37: 0x1A, // 7
      0xBD: 0x1B, // Minus
      0x38: 0x1C, // 8
      0x30: 0x1D, // 0
      0xDD: 0x1E, // Right Bracket
      0x4F: 0x1F, // O
      0x55: 0x20, // U
      0xDB: 0x21, // Left Bracket
      0x49: 0x22, // I
      0x50: 0x23, // P
      0x0D: 0x24, // Return
      0x4C: 0x25, // L
      0x4A: 0x26, // J
      0xDE: 0x27, // Quote
      0x4B: 0x28, // K
      0xBA: 0x29, // Semicolon
      0xDC: 0x2A, // Backslash
      0xBC: 0x2B, // Comma
      0xBF: 0x2C, // Slash
      0x4E: 0x2D, // N
      0x4D: 0x2E, // M
      0xBE: 0x2F, // Period
      0x09: 0x30, // Tab
      0x20: 0x31, // Space
      0xC0: 0x32, // Back Quote
      0x08: 0x33, // Delete
      //0x0D: 0x34, // Enter
      0x1B: 0x35, // Escape
      //0x5B: 0x36, // Right Windows
      0x5B: 0x37, // Left Windows
      0x10: 0x38, // Shift
      0x14: 0x39, // Caps Lock
      0x12: 0x3A, // Menu
      0x11: 0x3B, // Control
      0xA1: 0x3C, // Right Shift
      0xA5: 0x3D, // Right Menu
      0xA3: 0x3E, // Right Control
      0x18: 0x3F, // Final
      0x84: 0x40, // F17
      0x6C: 0x41, // Separator
      0x6A: 0x43, // Multiply
      0x6B: 0x45, // Add
      0x90: 0x47, // Num Lock
      0xAF: 0x48, // Volume Up
      0xAE: 0x49, // Volume Down
      0xAD: 0x4A, // Volume Mute
      0x6F: 0x4B, // Divide
      //0x0D: 0x4C, // Numpad Enter
      0x6D: 0x4E, // Subtract
      0x85: 0x4F, // F18
      0x86: 0x50, // F19
      0x60: 0x52, // Numpad 0
      0x61: 0x53, // Numpad 1
      0x62: 0x54, // Numpad 2
      0x63: 0x55, // Numpad 3
      0x64: 0x56, // Numpad 4
      0x65: 0x57, // Numpad 5
      0x66: 0x58, // Numpad 6
      0x67: 0x59, // Numpad 7
      //0x87: 0x5A, // F20
      0x68: 0x5B, // Numpad 8
      0x69: 0x5C, // Numpad 9
      0xFFE5: 0x5D, // Yen (JIS layout)
      //0xBF: 0x5E, // Underscore (JIS layout)
      //0xBC: 0x5F, // Keypad Comma/Separator (JIS layout)
      0xE5: 0x66, // Eisu (JIS layout)
      0x15: 0x68, // Kana (JIS layout)
      //0x87: 0x6A, // F16
      0x5D: 0x6E, // Apps
      0x5F: 0x7F  // Sleep
  ]

  // Function to perform key event based on Windows key code
  func PerformKeyEvent(code: Int, isDown: Bool) {
      // Reverse mapping from Windows key code to macOS key code
      if let macKeyCode = windowsToMacKeyMap[code] {
          let keyCode = CGKeyCode(macKeyCode)

          // Create and post the keyboard event
          let event = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: isDown)
          event?.post(tap: .cghidEventTap)
      } else {
          print("Key code \(code) not found in mapping.")
      }
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

      var eventType: CGEventType = .mouseMoved
      let pressedButtons = NSEvent.pressedMouseButtons
      if pressedButtons & (1 << 0) != 0 {
          eventType = .leftMouseDragged
      } else if pressedButtons & (1 << 1) != 0 {
          eventType = .rightMouseDragged
      }
      let moveEvent = CGEvent(mouseEventSource: nil, mouseType: eventType, mouseCursorPosition: CGPoint(x: absoluteX, y: absoluteY), mouseButton: .left)

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
      
      var eventType: CGEventType = .mouseMoved
      let pressedButtons = NSEvent.pressedMouseButtons
      if pressedButtons & (1 << 0) != 0 {
          eventType = .leftMouseDragged
      } else if pressedButtons & (1 << 1) != 0 {
          eventType = .rightMouseDragged
      }

      // Get the current mouse location
      if let currentMouseLocation = CGEvent(source: nil)?.location {
          // Calculate new position based on dx and dy
          let newX = currentMouseLocation.x + CGFloat(dx)
          let newY = currentMouseLocation.y + CGFloat(dy)

          // Ensure the new coordinates are within the screen's bounds
          let clampedX = min(max(newX, screenFrame.minX), screenFrame.maxX)
          let clampedY = min(max(newY, screenFrame.minY), screenFrame.maxY)

          // Create and post the event to move the mouse
          let moveEvent = CGEvent(mouseEventSource: nil, mouseType: eventType, mouseCursorPosition: CGPoint(x: clampedX, y: clampedY), mouseButton: .left)
          moveEvent?.post(tap: .cghidEventTap)
      }
  }

  func performMouseButton(buttonId: Int, isDown: Bool) {
      let eventSource = CGEventSource(stateID: .hidSystemState)

      var mouseEventType: CGEventType
      var mouseButton: CGMouseButton
      
      // 根据 buttonId 确定是左键还是右键
      if buttonId == 1 {
          mouseButton = .left
          mouseEventType = isDown ? .leftMouseDown : .leftMouseUp
      } else if buttonId == 3 {
          mouseButton = .right
          mouseEventType = isDown ? .rightMouseDown : .rightMouseUp
      } else {
          print("Unsupported buttonId")
          return
      }
      
      // 获取当前鼠标位置
      if let currentLocation = CGEvent(source: nil)?.location {
          // 创建鼠标事件
          let mouseEvent = CGEvent(mouseEventSource: eventSource, 
                                  mouseType: mouseEventType, 
                                  mouseCursorPosition: currentLocation, 
                                  mouseButton: mouseButton)
          
          // 发送事件
          mouseEvent?.post(tap: .cghidEventTap)
      } else {
          print("Failed to get current mouse location")
      }
  }

  func performMouseScroll(dx: Double, dy: Double) {
      let eventSource = CGEventSource(stateID: .hidSystemState)
      
      if dy != 0, let scrollEventY = CGEvent(scrollWheelEvent2Source: eventSource, units: .pixel, wheelCount: 1, wheel1: Int32(dy), wheel2: 0, wheel3: 0) {
          scrollEventY.post(tap: .cghidEventTap)
      }

      if dx != 0, let scrollEventX = CGEvent(scrollWheelEvent2Source: eventSource, units: .pixel, wheelCount: 1, wheel1: 0, wheel2: Int32(dx), wheel3: 0) {
          scrollEventX.post(tap: .cghidEventTap)
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

  var mouseMovedMonitor: Any?
  var previousCursorImage: NSImage?
  var previousCursorImageHashes: String = ""
  var cursorChangedCallbacks = Set<Int>()
  var jsHashWithImageHash = Dictionary<String, UInt32>()
  var hookAllCursorImage = Dictionary<Int, Bool>()

  func JSHash(buffer: [UInt8], size: Int) -> UInt32 {
    var hash: UInt32 = 1315423911
    for i in 0..<size {
        hash ^= ((hash << 5) &+ UInt32(buffer[i]) &+ (hash >> 2))
    }
    return hash & 0x7FFFFFFF
  }

  func getBitMapInt8(bitmapRep: NSBitmapImageRep) -> [UInt8]?{
    let width = bitmapRep.pixelsWide
    let height = bitmapRep.pixelsHigh
    let samplesPerPixel = bitmapRep.samplesPerPixel

    guard let bitmapData = bitmapRep.bitmapData else {
        print("No bitmap Data")
        return nil
    }

    var pixels: [UInt8] = Array(repeating: 0, count: width * height*4)
    for pixelIndex in 0..<width*height{
          pixels[pixelIndex*4+1]  = bitmapData[pixelIndex*samplesPerPixel + 2]
          pixels[pixelIndex*4+2] = bitmapData[pixelIndex*samplesPerPixel + 1]
          pixels[pixelIndex*4+3] =  bitmapData[pixelIndex*samplesPerPixel ]
          pixels[pixelIndex*4+0] = samplesPerPixel == 4 ? bitmapData[pixelIndex*4 + 3] : 255
    }

    return pixels
  }

  private func checkMouseCursor() {
   
    let currentCursor = NSCursor.currentSystem 
    if(currentCursor == nil){
        return;
    }
    
    let cursorImage = currentCursor?.image
    let hotSpot = currentCursor?.hotSpot

    let cursorImageHashes = sha256ForAllBitmapReps(in: cursorImage!)

    if(cursorImageHashes == previousCursorImageHashes){
        return;
    }
    previousCursorImageHashes = cursorImageHashes
    previousCursorImage = cursorImage

    //system default
    if defaultCursorHasher?.getHashMap().keys.contains(cursorImageHashes) ?? false {
      let cursorIndex = defaultCursorHasher?.getHashMap()[cursorImageHashes]
        //print("default: \(cursorIndex!)");
        for callbackID in cursorChangedCallbacks {
            if !(hookAllCursorImage[callbackID] ?? false) {
                let message: [String: Any] = [
                    "callbackID": callbackID,
                    "message": CursorConstants.cursorUpdatedDefault,
                    "msg_info": cursorIndex,
                    "cursorImage": FlutterStandardTypedData.init(bytes: Data([]))
                ]
                methodChannel?.invokeMethod("onCursorImageMessage", arguments: message)
            } else {
                // For hookAll=true, treat it as if it's not a default cursor
                let imagedataInt8 = getBitMapInt8(bitmapRep: cursorImage!.representations[0] as! NSBitmapImageRep)
                let messageHash = JSHash(buffer: imagedataInt8!, size: imagedataInt8!.count)
                jsHashWithImageHash[cursorImageHashes] = messageHash

                var int8Image:[UInt8] = [9];
                int8Image.append(contentsOf:[0,0,0,UInt8(cursorImage!.representations[0].pixelsWide)])
                int8Image.append(contentsOf:[0,0,0,UInt8(cursorImage!.representations[0].pixelsHigh)])
                int8Image.append(contentsOf:[0,0,0,UInt8(hotSpot!.x)])
                int8Image.append(contentsOf:[0,0,0,UInt8(hotSpot!.y)])
                int8Image.append(UInt8((messageHash >> 24) & 0xFF))
                int8Image.append(UInt8((messageHash >> 16) & 0xFF)) 
                int8Image.append(UInt8((messageHash >> 8) & 0xFF))
                int8Image.append(UInt8(messageHash & 0xFF))
                int8Image.append(contentsOf: imagedataInt8!)

                let message: [String: Any] = [
                    "callbackID": callbackID,
                    "message": CursorConstants.cursorUpdatedImage,
                    "msg_info": messageHash,
                    "cursorImage": FlutterStandardTypedData.init(bytes: Data(int8Image))
                ]
                methodChannel?.invokeMethod("onCursorImageMessage", arguments: message)
            }
        }
        return ;
    }
      
    //cache iamge
    if jsHashWithImageHash.keys.contains(cursorImageHashes) {
      //print("cache: \(jsHashWithImageHash[cursorImageHashes])");
      let messageHash = jsHashWithImageHash[cursorImageHashes]
      for callbackID in cursorChangedCallbacks {
        let message: [String: Any] = [
            "callbackID": callbackID,
            "message": CursorConstants.cursorUpdatedCached,
            "msg_info": messageHash,
            "cursorImage": FlutterStandardTypedData.init(bytes: Data([]))
        ]
        methodChannel?.invokeMethod("onCursorImageMessage", arguments: message)
      }
      return ;
    }
      
    let imagedataInt8 = getBitMapInt8(bitmapRep: cursorImage!.representations[0] as! NSBitmapImageRep)
    let messageHash = JSHash(buffer: imagedataInt8!, size: imagedataInt8!.count)
    jsHashWithImageHash[cursorImageHashes] = messageHash

    //print("[messageHash:\(messageHash)] \n cursorImagepixelsWide:\(cursorImage!.representations[0].pixelsWide) \n  cursorImagepixelsWide: \(cursorImage!.representations[0].pixelsHigh)\n")
    var int8Image:[UInt8] = [9];
    int8Image.append(contentsOf:[0,0,0,UInt8(cursorImage!.representations[0].pixelsWide)])
    int8Image.append(contentsOf:[0,0,0,UInt8(cursorImage!.representations[0].pixelsHigh)])
    int8Image.append(contentsOf:[0,0,0,UInt8(hotSpot!.x)])
    int8Image.append(contentsOf:[0,0,0,UInt8(hotSpot!.y)])
    int8Image.append(UInt8((messageHash >> 24) & 0xFF))
    int8Image.append(UInt8((messageHash >> 16) & 0xFF)) 
    int8Image.append(UInt8((messageHash >> 8) & 0xFF))
    int8Image.append(UInt8(messageHash & 0xFF))
    int8Image.append(contentsOf: imagedataInt8!)

    for callbackID in cursorChangedCallbacks {
      let message: [String: Any] = [
          "callbackID": callbackID,
          "message": CursorConstants.cursorUpdatedImage,
          "msg_info": messageHash,
          "cursorImage": FlutterStandardTypedData.init(bytes: Data(int8Image))
      ]
      methodChannel?.invokeMethod("onCursorImageMessage", arguments: message)
    }
  }
  
  var activities: [String: NSObjectProtocol] = [:]

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getPlatformVersion":
      result("macOS " + ProcessInfo.processInfo.operatingSystemVersionString)
    case "beginActivity":
      let key = UUID().uuidString
      let reason = call.arguments as? String ?? "Prevent nap to do an important task."
      let activity = ProcessInfo.processInfo.beginActivity(options: [.idleDisplaySleepDisabled, .userInitiated], reason: reason)
      activities[key] = activity
      result(key)
    case "endActivity":
      let key = call.arguments as? String ?? ""
      if let activity = activities[key] {
        ProcessInfo.processInfo.endActivity(activity)
        activities[key] = nil 
        result(true)
        return
      }
      result(FlutterError(code: "Unknown Activity", message: nil, details: nil))
    case "getMonitorCount":
      let key = UUID().uuidString
      let reason = call.arguments as? String ?? "Prevent nap to do an important task."
      let activity = ProcessInfo.processInfo.beginActivity(options: [.idleDisplaySleepDisabled, .userInitiated], reason: reason)
      activities[key] = activity
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
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for MouseMove ABSL", details: nil))
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
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for MouseMove Relative", details: nil))
      }
    case "mousePress":
      if let args = call.arguments as? [String: Any],
        let buttonId = args["buttonId"] as? Int,
        let isDown = args["isDown"] as? Bool {
        // 调用鼠标移动函数
        performMouseButton(buttonId: buttonId, isDown: isDown)
        result(nil) // 表示成功执行，不返回值
      } else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for Mouse Press", details: nil))
      }
    case "mouseScroll":
      if let args = call.arguments as? [String: Any],
        let dx = args["dx"] as? Double,
        let dy = args["dy"] as? Double {
        // 调用鼠标移动函数
        performMouseScroll(dx: dx, dy: dy)
        result(nil) // 表示成功执行，不返回值
      } else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for Mouse Scroll", details: nil))
      }
    case "KeyPress":
        if let args = call.arguments as? [String: Any],
        let keyCode = args["code"] as? Int,
        let isDown = args["isDown"] as? Bool {
        // 调用鼠标移动函数
        PerformKeyEvent(code: keyCode, isDown: isDown)
        result(nil) // 表示成功执行，不返回值
      } else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for KeyPress", details: nil))
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
    case "hookCursorImage":
      if let args = call.arguments as? [String: Any],
          let callbackID = args["callbackID"] as? Int,
          let hookAll = args["hookAll"] as? Bool {
          if cursorChangedCallbacks.count == 0 {
            mouseMovedMonitor = NSEvent.addGlobalMonitorForEvents(matching: .mouseMoved) { [weak self] event in  self?.checkMouseCursor() }
          }
          cursorChangedCallbacks.insert(callbackID)
          hookAllCursorImage[callbackID] = hookAll

          // If hookAll is true, trigger an immediate callback
          if hookAll {
              let currentCursor = NSCursor.currentSystem
              if let cursorImage = currentCursor?.image,
                 let hotSpot = currentCursor?.hotSpot {
                  let cursorImageHashes = sha256ForAllBitmapReps(in: cursorImage)
                  let imagedataInt8 = getBitMapInt8(bitmapRep: cursorImage.representations[0] as! NSBitmapImageRep)
                  let messageHash = JSHash(buffer: imagedataInt8!, size: imagedataInt8!.count)
                  jsHashWithImageHash[cursorImageHashes] = messageHash

                  var int8Image:[UInt8] = [9]
                  int8Image.append(contentsOf:[0,0,0,UInt8(cursorImage.representations[0].pixelsWide)])
                  int8Image.append(contentsOf:[0,0,0,UInt8(cursorImage.representations[0].pixelsHigh)])
                  int8Image.append(contentsOf:[0,0,0,UInt8(hotSpot.x)])
                  int8Image.append(contentsOf:[0,0,0,UInt8(hotSpot.y)])
                  int8Image.append(UInt8((messageHash >> 24) & 0xFF))
                  int8Image.append(UInt8((messageHash >> 16) & 0xFF)) 
                  int8Image.append(UInt8((messageHash >> 8) & 0xFF))
                  int8Image.append(UInt8(messageHash & 0xFF))
                  int8Image.append(contentsOf: imagedataInt8!)

                  let message: [String: Any] = [
                      "callbackID": callbackID,
                      "message": CursorConstants.cursorUpdatedImage,
                      "msg_info": messageHash,
                      "cursorImage": FlutterStandardTypedData.init(bytes: Data(int8Image))
                  ]
                  methodChannel?.invokeMethod("onCursorImageMessage", arguments: message)
              }
          }
         }
      else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for hookCursorImage", details: nil))
      }
    case "unhookCursorImage":
      if let args = call.arguments as? [String: Any],
        let callbackID = args["callbackID"] as? Int{
          cursorChangedCallbacks.remove(callbackID)
          hookAllCursorImage.removeValue(forKey: callbackID)
          if cursorChangedCallbacks.count == 0{
              if let monitor = mouseMovedMonitor {
               NSEvent.removeMonitor(monitor)
            }
          }
         }
      else {
        result(FlutterError(code: "BAD_ARGS", message: "Missing or incorrect arguments for unhookCursorImage", details: nil))
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

class CursorHasher {
    private var cursorHashMap: [String: Int] = [:]
    
    init() {
        calculateHashesForDefaultCursors()
    }
    
    private func calculateHashesForDefaultCursors() {
        let defaultCursors: [NSCursor : Int] = [
         .arrow : 32512,                // IDC_ARROW
         .iBeam : 32513,                // IDC_IBEAM
         .crosshair : 32515,             // IDC_CROSS
         .pointingHand:32649,           // IDC_HAND
         .resizeLeftRight:32644,        // IDC_SIZEWE
         .resizeUpDown:32645,           // IDC_SIZENS
         .operationNotAllowed:32648,    // IDC_NO
         .closedHand: 32401,          // custom grabbing
         .openHand: 32402,            // custom grab
         .resizeUp:32403,             // custom resizeUp
         .resizeDown:32404,           // custom resizeDown
         .resizeLeft:32405,           // custom resizeLeft
         .resizeRight:32406,          // custom resizeRight
         .disappearingItem:32407,     // custom disappearing
         .contextualMenu:32408,       // custom contextMenu
         .dragLink:  32409,           // custom alias
         .dragCopy:  32410,          // custom copy
         .iBeamCursorForVerticalLayout : 32411 // custom verticalText
      ]

      for (cursor,index) in defaultCursors {
          if(cursor == nil || cursor.image == nil){
            continue
          }
          let hash = sha256ForAllBitmapReps(in: cursor.image)
          cursorHashMap[hash] = index
      }
    }
    
    func getHashMap() -> [String: Int] {
        return cursorHashMap
    }
}

func sha256ForAllBitmapReps(in image: NSImage) -> String {
    var alldata = Data()
    for rep in image.representations {
        if let bitmapRep = rep as? NSBitmapImageRep {
            guard let pixelData = bitmapRep.bitmapData else { continue }
            let dataSize = bitmapRep.bytesPerRow * bitmapRep.pixelsHigh
            let data = Data(bytes: pixelData, count: dataSize)
            alldata.append(data)
        }
    }
    
    var hash = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
    alldata.withUnsafeBytes {
        _ = CC_SHA256($0.baseAddress, CC_LONG(alldata.count), &hash)
    }
    
    let hashString = hash.map { String(format: "%02x", $0) }.joined()
    return hashString
}

