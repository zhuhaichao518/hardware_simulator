package com.cloudplayplus.hardware_simulator

import android.app.Activity
import android.content.pm.PackageManager
import android.os.Build
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import androidx.annotation.RequiresApi
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result
import org.flame_engine.gamepads_android.GamepadsCompatibleActivity

/** HardwareSimulatorPlugin */
class HardwareSimulatorPlugin: FlutterPlugin, MethodCallHandler, ActivityAware {
  /// The MethodChannel that will the communication between Flutter and native Android
  ///
  /// This local reference serves to register the plugin with the Flutter Engine and unregister it
  /// when the Flutter Engine is detached from the Activity
  private lateinit var channel : MethodChannel
  private var activity: Activity? = null
  private var isCursorLocked = false
  private var isAndroidTV = false
  private var flutterView: View? = null

  override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
    channel = MethodChannel(flutterPluginBinding.binaryMessenger, "hardware_simulator")
    channel.setMethodCallHandler(this)
  }

  @RequiresApi(Build.VERSION_CODES.O)
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

  @RequiresApi(Build.VERSION_CODES.O)
  private fun lockCursor() {
    if (isCursorLocked) return;
    isCursorLocked = true;
    flutterView = activity?.window?.decorView?.findViewById<View>(android.R.id.content)
    flutterView?.isFocusable = true
    flutterView?.isFocusableInTouchMode = true
    flutterView?.let { view ->
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        //view.isFocusable = true
        view.requestFocus()
        view.requestPointerCapture()
      }
    }
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
        MotionEvent.ACTION_BUTTON_PRESS -> {
          // 处理鼠标按键按下
          val buttonId = when (event.actionButton) {
            MotionEvent.BUTTON_PRIMARY -> 1  // 左键
            MotionEvent.BUTTON_SECONDARY -> 3  // 右键
            MotionEvent.BUTTON_TERTIARY -> 2  // 中键
            else -> event.actionButton
          }
          if (isAndroidTV && event.actionButton == MotionEvent.BUTTON_BACK) {
            // For Android TV, mouse right click is recognized as BUTTON_BACK.
            // It is already handled in registerLockedKeyEventHandler.
            return@setOnCapturedPointerListener true
          }
          channel.invokeMethod("onCursorButton", mapOf(
            "buttonId" to buttonId,
            "isDown" to true
          ))
          true
        }
        MotionEvent.ACTION_BUTTON_RELEASE -> {
          // 处理鼠标按键释放
          val buttonId = when (event.actionButton) {
            MotionEvent.BUTTON_PRIMARY -> 1  // 左键
            MotionEvent.BUTTON_SECONDARY -> 3  // 右键
            MotionEvent.BUTTON_TERTIARY -> 2  // 中键
            else -> event.actionButton
          }
          if (isAndroidTV && event.actionButton == MotionEvent.BUTTON_BACK) {
            // For Android TV, mouse right click is recognized as BUTTON_BACK.
            // It is already handled in registerLockedKeyEventHandler.
            return@setOnCapturedPointerListener true
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
    /*flutterView?.let { view ->
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        //view.isFocusable = true
        view.requestFocus()
        view.requestPointerCapture()
        isCursorLocked = true
      }
    }*/
  }

  private fun unlockCursor() {
    if (!isCursorLocked) return;
    flutterView?.let { view ->
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        view.releasePointerCapture()
        isCursorLocked = false
      }
    }
    flutterView?.isFocusable = false
    flutterView?.isFocusableInTouchMode = false
  }

  override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
    channel.setMethodCallHandler(null)
  }

  private fun isDpadKey(keyCode: Int): Boolean {
    return keyCode == KeyEvent.KEYCODE_DPAD_CENTER//in KeyEvent.KEYCODE_DPAD_UP..KeyEvent.KEYCODE_DPAD_CENTER
  }

  @RequiresApi(Build.VERSION_CODES.O)
  override fun onAttachedToActivity(binding: ActivityPluginBinding) {
    activity = binding.activity
    isAndroidTV = activity!!.packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)

    val compatibleActivity = activity as GamepadsCompatibleActivity
    compatibleActivity.registerLockedKeyEventHandler { event ->
      if (!isCursorLocked) {
        return@registerLockedKeyEventHandler false
      }
      val device = InputDevice.getDevice(event.deviceId)
      if (isAndroidTV && event.source and InputDevice.SOURCE_KEYBOARD != InputDevice.SOURCE_KEYBOARD) {
        // mouse right button
        if (event.keyCode == 4) {
          val isDown = when (event.action) {
            KeyEvent.ACTION_DOWN -> true
            KeyEvent.ACTION_UP -> false
            else -> false
          }
          channel.invokeMethod("onCursorButton", mapOf(
            "buttonId" to 3,
            "isDown" to isDown
          ))
          return@registerLockedKeyEventHandler true
        }
      }
      if (event.source and InputDevice.SOURCE_KEYBOARD == InputDevice.SOURCE_KEYBOARD) {
        // 这是键盘事件
        if (isAndroidTV && event.keyCode == KeyEvent.KEYCODE_MENU) {
          val isDown = when (event.action) {
            KeyEvent.ACTION_DOWN -> true
            KeyEvent.ACTION_UP -> false
            else -> false
          }
          channel.invokeMethod("onCursorButton", mapOf(
            "buttonId" to 3,
            "isDown" to isDown
          ))
        }
        if (isDpadKey(event.keyCode) ) {
          if (isAndroidTV) {
            val isDown = when (event.action) {
              KeyEvent.ACTION_DOWN -> true
              KeyEvent.ACTION_UP -> false
              else -> false
            }
            if (event.keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
              //mouse left or middle button
              if (event.scanCode == 0) {
                // We don't need to invoke for android TV.
                /*channel.invokeMethod("onCursorButton", mapOf(
                  "buttonId" to 2/*right button id*/,
                  "isDown" to isDown
                ))*/
              } else {
                //DPad OK button
                channel.invokeMethod("onKeyboardButton", mapOf(
                  //enter key for android
                  "buttonId" to 66,
                  "isDown" to isDown
                ))
              }
            }
          }
          return@registerLockedKeyEventHandler true
        }
        val isDown = when (event.action) {
          KeyEvent.ACTION_DOWN -> true
          KeyEvent.ACTION_UP -> false
          else -> false
        }
        channel.invokeMethod("onKeyboardButton", mapOf(
          "buttonId" to event.keyCode,
          "isDown" to isDown
        ))
        return@registerLockedKeyEventHandler true
      } else {
        return@registerLockedKeyEventHandler false
      }
    }
    // 获取Flutter的根视图
    flutterView = binding.activity.window.decorView.findViewById<View>(android.R.id.content)
    
    // 设置视图可聚焦 不设置无法移动
    //这两个只要设置一个 安卓TV Dpad就无法正常工作
    //flutterView?.isFocusable = true
    //flutterView?.isFocusableInTouchMode = true
    
    // 设置捕获指针监听器，处理所有类型的事件

    // 监听窗口焦点变化
    /*binding.activity.window.decorView.setOnFocusChangeListener { _, hasFocus ->
      if (hasFocus && isCursorLocked) {
        flutterView?.requestPointerCapture()
      }
    }*/
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
