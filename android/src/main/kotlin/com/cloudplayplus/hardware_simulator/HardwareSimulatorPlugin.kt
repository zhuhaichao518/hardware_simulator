package com.cloudplayplus.hardware_simulator

import android.app.Activity
import android.os.Build
import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import androidx.annotation.NonNull
import androidx.annotation.RequiresApi
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result

/** HardwareSimulatorPlugin */
class HardwareSimulatorPlugin: FlutterPlugin, MethodCallHandler, ActivityAware {
  /// The MethodChannel that will the communication between Flutter and native Android
  ///
  /// This local reference serves to register the plugin with the Flutter Engine and unregister it
  /// when the Flutter Engine is detached from the Activity
  private lateinit var channel : MethodChannel
  private var activity: Activity? = null
  private var isCursorLocked = false
  private var flutterView: View? = null

  override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
    channel = MethodChannel(flutterPluginBinding.binaryMessenger, "hardware_simulator")
    channel.setMethodCallHandler(this)
  }

  override fun onMethodCall(call: MethodCall, result: Result) {
    when (call.method) {
      "getPlatformVersion" -> {
        result.success("Android ${android.os.Build.VERSION.RELEASE}")
      }
      "isMouseConnected" -> {
        result.success(isMouseConnected())
      }
      "lockCursor" -> {
        lockCursor()
        result.success(null)
      }
      "unlockCursor" -> {
        unlockCursor()
        result.success(null)
      }
      else -> {
        result.notImplemented()
      }
    }
  }

  private fun isMouseConnected(): Boolean {
    val inputDevices = InputDevice.getDeviceIds()
    return inputDevices.any { deviceId ->
      InputDevice.getDevice(deviceId)?.sources?.and(InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE
    }
  }

  private fun lockCursor() {
    flutterView?.let { view ->
      if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
        view.isFocusable = true
        view.requestFocus()
        view.requestPointerCapture()
        isCursorLocked = true
      }
    }
  }

  private fun unlockCursor() {
    flutterView?.let { view ->
      if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
        view.releasePointerCapture()
        isCursorLocked = false
      }
    }
  }

  override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
    channel.setMethodCallHandler(null)
  }

  @RequiresApi(Build.VERSION_CODES.O)
  override fun onAttachedToActivity(binding: ActivityPluginBinding) {
    activity = binding.activity
    
    // 获取Flutter的根视图
    flutterView = binding.activity.window.decorView.findViewById<View>(android.R.id.content)
    
    // 设置视图可聚焦
    flutterView?.isFocusable = true
    flutterView?.isFocusableInTouchMode = true
    
    // 设置捕获指针监听器
    flutterView?.setOnCapturedPointerListener { _, event ->
      when (event.source) {
        InputDevice.SOURCE_MOUSE_RELATIVE -> {
          // 处理鼠标相对移动
          channel.invokeMethod("onCursorMoved", mapOf(
            "dx" to event.x,
            "dy" to event.y
          ))
          true
        }
        InputDevice.SOURCE_TOUCHPAD -> {
          // 处理触控板事件
          val relativeX = event.getAxisValue(MotionEvent.AXIS_RELATIVE_X)
          val relativeY = event.getAxisValue(MotionEvent.AXIS_RELATIVE_Y)
          channel.invokeMethod("onCursorMoved", mapOf(
            "dx" to relativeX,
            "dy" to relativeY
          ))
          true
        }
        else -> false
      }
    }

    // 监听窗口焦点变化
    binding.activity.window.decorView.setOnFocusChangeListener { _, hasFocus ->
      if (hasFocus && isCursorLocked) {
        flutterView?.requestPointerCapture()
      }
    }
  }

  override fun onDetachedFromActivityForConfigChanges() {
    activity = null
    flutterView = null
  }

  override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
    activity = binding.activity
    flutterView = binding.activity.window.decorView.findViewById<View>(android.R.id.content)
  }

  override fun onDetachedFromActivity() {
    activity = null
    flutterView = null
  }
}
