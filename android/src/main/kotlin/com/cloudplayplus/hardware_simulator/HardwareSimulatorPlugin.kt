package com.cloudplayplus.hardware_simulator

import android.app.Activity
import android.view.InputDevice
import android.view.MotionEvent
import android.view.View
import androidx.annotation.NonNull
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
    activity?.let { act ->
      act.window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
        or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        or View.SYSTEM_UI_FLAG_FULLSCREEN
        or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
      isCursorLocked = true
    }
  }

  private fun unlockCursor() {
    activity?.let { act ->
      act.window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
      isCursorLocked = false
    }
  }

  override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
    channel.setMethodCallHandler(null)
  }

  override fun onAttachedToActivity(binding: ActivityPluginBinding) {
    activity = binding.activity
    binding.activity.window.decorView.setOnGenericMotionListener { _, event ->
      if (event.source and InputDevice.SOURCE_MOUSE == InputDevice.SOURCE_MOUSE) {
        when (event.action) {
          MotionEvent.ACTION_HOVER_MOVE -> {
            channel.invokeMethod("onCursorMoved", mapOf(
              "dx" to event.x,
              "dy" to event.y
            ))
            true
          }
          MotionEvent.ACTION_BUTTON_PRESS -> {
            channel.invokeMethod("onCursorButton", mapOf(
              "buttonId" to event.buttonState,
              "isDown" to true
            ))
            true
          }
          MotionEvent.ACTION_BUTTON_RELEASE -> {
            channel.invokeMethod("onCursorButton", mapOf(
              "buttonId" to event.buttonState,
              "isDown" to false
            ))
            true
          }
          MotionEvent.ACTION_SCROLL -> {
            val scrollY = event.getAxisValue(MotionEvent.AXIS_VSCROLL)
            val scrollX = event.getAxisValue(MotionEvent.AXIS_HSCROLL)
            channel.invokeMethod("onCursorScroll", mapOf(
              "dx" to scrollX,
              "dy" to scrollY
            ))
            true
          }
          else -> false
        }
      } else {
        false
      }
    }
  }

  override fun onDetachedFromActivityForConfigChanges() {
    activity = null
  }

  override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {
    activity = binding.activity
  }

  override fun onDetachedFromActivity() {
    activity = null
  }
}
