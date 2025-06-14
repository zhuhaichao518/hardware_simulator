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
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        view.isFocusable = true
        view.requestFocus()
        view.requestPointerCapture()
        isCursorLocked = true
      }
    }
  }

  private fun unlockCursor() {
    flutterView?.let { view ->
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
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
    
    // 设置捕获指针监听器，处理所有类型的事件
    flutterView?.setOnCapturedPointerListener { _, event ->
      when (event.actionMasked) {
        MotionEvent.ACTION_HOVER_MOVE, MotionEvent.ACTION_MOVE -> {
          // 处理鼠标移动
          val relX = event.getAxisValue(MotionEvent.AXIS_RELATIVE_X)
          val relY = event.getAxisValue(MotionEvent.AXIS_RELATIVE_Y)
          val dx = if (relX != 0f) relX else event.x
          val dy = if (relY != 0f) relY else event.y
          channel.invokeMethod("onCursorMoved", mapOf(
            "dx" to dx,
            "dy" to dy
          ))
          true
        }
        MotionEvent.ACTION_SCROLL -> {
          // 处理滚轮事件
          val scrollX = event.getAxisValue(MotionEvent.AXIS_HSCROLL)
          val scrollY = event.getAxisValue(MotionEvent.AXIS_VSCROLL)
          channel.invokeMethod("onCursorScroll", mapOf(
            "dx" to scrollX,
            "dy" to scrollY
          ))
          true
        }
        MotionEvent.ACTION_DOWN, MotionEvent.ACTION_BUTTON_PRESS -> {
          // 处理鼠标按键按下
          val buttonId = when (event.actionButton) {
            MotionEvent.BUTTON_PRIMARY -> 1  // 左键
            MotionEvent.BUTTON_SECONDARY -> 3  // 右键
            MotionEvent.BUTTON_TERTIARY -> 2  // 中键
            else -> event.actionButton
          }
          channel.invokeMethod("onCursorButton", mapOf(
            "buttonId" to buttonId,
            "isDown" to true
          ))
          true
        }
        MotionEvent.ACTION_UP, MotionEvent.ACTION_BUTTON_RELEASE -> {
          // 处理鼠标按键释放
          val buttonId = when (event.actionButton) {
            MotionEvent.BUTTON_PRIMARY -> 1  // 左键
            MotionEvent.BUTTON_SECONDARY -> 3  // 右键
            MotionEvent.BUTTON_TERTIARY -> 2  // 中键
            else -> event.actionButton
          }
          channel.invokeMethod("onCursorButton", mapOf(
            "buttonId" to buttonId,
            "isDown" to false
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
