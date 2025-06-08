import Flutter
import UIKit
import GameController
import ObjectiveC

@available(iOS 13.4, *)
extension UIViewController {
    private static var swizzled = false
    private static var originalPrefersPointerLockedIMP: IMP?
    
    static func swizzlePrefersPointerLocked() {
        if #available(iOS 14.0, *) {
            let originalSelector = #selector(getter: UIViewController.prefersPointerLocked)
            let swizzledSelector = #selector(UIViewController.swizzled_prefersPointerLocked)
            
            guard let originalMethod = class_getInstanceMethod(UIViewController.self, originalSelector),
                  let swizzledMethod = class_getInstanceMethod(UIViewController.self, swizzledSelector) else {
                return
            }
            
            // 如果已经 swizzled，先恢复原始实现
            if swizzled {
                if let originalIMP = originalPrefersPointerLockedIMP {
                    method_setImplementation(originalMethod, originalIMP)
                }
            } else {
                // 保存原始方法的实现
                originalPrefersPointerLockedIMP = method_getImplementation(originalMethod)
            }
            
            // 设置新的实现
            method_setImplementation(originalMethod, method_getImplementation(swizzledMethod))
            swizzled = true
        }
    }
    
    static func unswizzlePrefersPointerLocked() {
        guard swizzled, let originalIMP = originalPrefersPointerLockedIMP else { return }
        
        if #available(iOS 14.0, *) {
            let originalSelector = #selector(getter: UIViewController.prefersPointerLocked)
            
            guard let originalMethod = class_getInstanceMethod(UIViewController.self, originalSelector) else {
                return
            }
            
            // 直接设置回原始实现
            method_setImplementation(originalMethod, originalIMP)
            swizzled = false
        }
    }
    
    @available(iOS 14.0, *)
    @objc func swizzled_prefersPointerLocked() -> Bool {
        print("swizzled_prefersPointerLocked called")
        return !GCMouse.mice().isEmpty
    }
    
    /*func disableSystemGestures() {
        if #available(iOS 11.0, *) {
            self.modalPresentationStyle = .fullScreen
            self.edgesForExtendedLayout = []
            self.setNeedsUpdateOfScreenEdgesDeferringSystemGestures()
        }
    }*/
    
    /*override open func preferredScreenEdgesDeferringSystemGestures() -> UIRectEdge {
        return .all
    }*/
}

@available(iOS 13.4, *)
extension UIView {
    func hidePointer() {
        let interaction = UIPointerInteraction(delegate: self)
        addInteraction(interaction)
    }
    
    func unhidePointer() {
        for interaction in interactions {
            if let pointerInteraction = interaction as? UIPointerInteraction {
                removeInteraction(pointerInteraction)
            }
        }
    }
}

@available(iOS 13.4, *)
extension UIView: UIPointerInteractionDelegate {
    public func pointerInteraction(_ interaction: UIPointerInteraction, regionFor request: UIPointerRegionRequest, defaultRegion: UIPointerRegion) -> UIPointerRegion? {
        return nil; //UIPointerRegion(rect: bounds)
    }
    
    public func pointerInteraction(_ interaction: UIPointerInteraction, styleFor region: UIPointerRegion) -> UIPointerStyle? {
        return UIPointerStyle.hidden()
    }
}

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
        case "lockCursor":
            if #available(iOS 11.0, *) {
                if let window = UIApplication.shared.windows.first {
                    if #available(iOS 13.4, *) {
                        window.rootViewController?.view.hidePointer()
                    }
                    if #available(iOS 14.0, *) {
                        UIViewController.swizzlePrefersPointerLocked()
                        window.rootViewController?.setNeedsUpdateOfPrefersPointerLocked()
                    }
                }
            }
            result(nil)
        case "unlockCursor":
            if #available(iOS 11.0, *) {
                if let window = UIApplication.shared.windows.first {
                    if #available(iOS 13.4, *) {
                        window.rootViewController?.view.unhidePointer()
                    }
                    if #available(iOS 14.0, *) {
                        UIViewController.unswizzlePrefersPointerLocked()
                        window.rootViewController?.setNeedsUpdateOfPrefersPointerLocked()
                    }
                }
            }
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
            return
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
