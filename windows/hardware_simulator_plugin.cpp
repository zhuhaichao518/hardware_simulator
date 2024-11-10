#include "hardware_simulator_plugin.h"

#include "cursor_monitor.h"
#include "gamecontroller_manager.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <future>
#include <memory>
#include <sstream>

namespace hardware_simulator {

// static
void HardwareSimulatorPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "hardware_simulator",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<HardwareSimulatorPlugin>();

  plugin->channel_ = std::move(channel);

  plugin->channel_->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

HardwareSimulatorPlugin::HardwareSimulatorPlugin() {}

HardwareSimulatorPlugin::~HardwareSimulatorPlugin() {}

//Todo:OpenInputDesktop should fail because we have a window resource in this process.
//We need to create another process to handle this scenario.
HDESK syncThreadDesktop() {
    auto hDesk = OpenInputDesktop(DF_ALLOWOTHERACCOUNTHOOK, FALSE, GENERIC_ALL);
    if (!hDesk) {
        //auto err = GetLastError();
        //BOOST_LOG(error) << "Failed to Open Input Desktop [0x"sv << util::hex(err).to_string_view() << ']';

        return nullptr;
    }

    if (!SetThreadDesktop(hDesk)) {
        //auto err = GetLastError();
        //BOOST_LOG(error) << "Failed to sync desktop to thread [0x"sv << util::hex(err).to_string_view() << ']';
    }

    CloseDesktop(hDesk);

    return hDesk;
}

thread_local HDESK _lastKnownInputDesktop = nullptr;

void async_send_input_retry(INPUT& i) {
    while (true) {
        auto send = SendInput(1, &i, sizeof(INPUT));
        if (send == 1) {
            break;
        }

        auto hDesk = syncThreadDesktop();
        if (_lastKnownInputDesktop != hDesk) {
            _lastKnownInputDesktop = hDesk;
        }
        else {
            break;
        }
    }
}

void send_input(INPUT& i) {
    auto send = SendInput(1, &i, sizeof(INPUT));
    if (send != 1) {
        // 将重试逻辑放到新线程中
        std::future<void> retry_future = std::async(std::launch::async, async_send_input_retry, std::ref(i));
        retry_future.wait();
    }
}

void performMouseButton(int button, bool release) {
    //SHORT KEY_STATE_DOWN = 0x8000;

    INPUT i{};

    i.type = INPUT_MOUSE;
    auto& mi = i.mi;

    int mouse_button;
    if (button == 1) {
        mi.dwFlags = release ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
        mouse_button = VK_LBUTTON;
    }
    else if (button == 2) {
        mi.dwFlags = release ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
        mouse_button = VK_MBUTTON;
    }
    else if (button == 3) {
        mi.dwFlags = release ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
        mouse_button = VK_RBUTTON;
    }
    else if (button == 4) {
        mi.dwFlags = release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
        mi.mouseData = XBUTTON1;
        mouse_button = VK_XBUTTON1;
    }
    else {
        mi.dwFlags = release ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
        mi.mouseData = XBUTTON2;
        mouse_button = VK_XBUTTON2;
    }
    
    //Haichao: what is it used for? It blocks button up for UAC window.
    /*auto key_state = GetAsyncKeyState(mouse_button);
    bool key_state_down = (key_state & 0x8000) != 0;
    if (key_state_down != release) {
        return;
    }*/

    send_input(i);
}

#pragma warning(disable:4244)

void performKeyEvent(uint16_t modcode, bool isDown) {
    INPUT i{};
    i.type = INPUT_KEYBOARD;
    auto& ki = i.ki;

    // For some reason, MapVirtualKey(VK_LWIN, MAPVK_VK_TO_VSC) doesn't seem to work :/
    if (modcode != VK_LWIN && modcode != VK_RWIN && modcode != VK_PAUSE) {
        ki.wScan = MapVirtualKey(modcode, MAPVK_VK_TO_VSC);
        ki.dwFlags = KEYEVENTF_SCANCODE;
    }
    else {
        ki.wVk = modcode;
    }

    // https://docs.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    switch (modcode) {
    case VK_RMENU:
    case VK_RCONTROL:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_DIVIDE:
        ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        break;
    default:
        break;
    }

    if (!isDown) {
        ki.dwFlags |= KEYEVENTF_KEYUP;
    }

    send_input(i);
}

void performMouseMoveRelative(double x,double y){
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_MOVE;
    mi.dx = x;
    mi.dy = y;

    send_input(i);
}

void performMouseMoveAbsl(double x,double y){
    INPUT i {};

    i.type = INPUT_MOUSE;
    auto &mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    mi.dx = x * 65535;
    mi.dy = y * 65535;

    send_input(i);
}

void scroll(int distance) {
    INPUT i{};

    i.type = INPUT_MOUSE;
    auto& mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_WHEEL;
    mi.mouseData = distance;

    send_input(i);
}

void hscroll(int distance) {
    INPUT i{};

    i.type = INPUT_MOUSE;
    auto& mi = i.mi;

    mi.dwFlags = MOUSEEVENTF_HWHEEL;
    mi.mouseData = distance;

    send_input(i);
}

void HardwareSimulatorPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const flutter::EncodableMap* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater()) {
      version_stream << "10+";
    } else if (IsWindows8OrGreater()) {
      version_stream << "8";
    } else if (IsWindows7OrGreater()) {
      version_stream << "7";
    }
    result->Success(flutter::EncodableValue(version_stream.str()));
  } else if (method_call.method_name().compare("KeyPress") == 0) {
        auto keyCode = (args->find(flutter::EncodableValue("code")))->second;
        auto isDown = (args->find(flutter::EncodableValue("isDown")))->second;
        performKeyEvent(static_cast<int>(std::get<int>((keyCode))), static_cast<bool>(std::get<bool>((isDown))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mouseMoveR") == 0) {
        auto deltax = (args->find(flutter::EncodableValue("x")))->second;
        auto deltay = (args->find(flutter::EncodableValue("y")))->second;
        performMouseMoveRelative(static_cast<double>(std::get<double>((deltax))), static_cast<double>(std::get<double>((deltay))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mouseMoveA") == 0) {
        auto percentx = (args->find(flutter::EncodableValue("x")))->second;
        auto percenty = (args->find(flutter::EncodableValue("y")))->second;
        performMouseMoveAbsl(static_cast<double>(std::get<double>((percentx))), static_cast<double>(std::get<double>((percenty))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mousePress") == 0) {
        auto buttonid = (args->find(flutter::EncodableValue("buttonId")))->second;
        auto isDown = (args->find(flutter::EncodableValue("isDown")))->second;
        performMouseButton(static_cast<int>(std::get<int>((buttonid))), !static_cast<bool>(std::get<bool>((isDown))));
        result->Success(nullptr);
  } else if (method_call.method_name().compare("mouseScroll") == 0) {
        auto dx = (args->find(flutter::EncodableValue("dx")))->second;
        auto dy = (args->find(flutter::EncodableValue("dy")))->second;
        int deltax = static_cast<double>(std::get<double>((dx)));
        int deltay = static_cast<double>(std::get<double>((dy)));
        if (deltax!=0){
          hscroll(deltax * 2);
        }
        if (deltay!=0){
          scroll(deltay * 2);
        }
        result->Success(nullptr);
  } else if (method_call.method_name().compare("hookCursorImage") == 0) {
        auto callbackID = static_cast<int>(std::get<int>((args->find(flutter::EncodableValue("callbackID")))->second));
        CursorMonitor::startHook([this, callbackID](int message, int msg_info, const std::vector<uint8_t>& cursorImage) {
            flutter::EncodableMap encoded_message;
            encoded_message[flutter::EncodableValue("callbackID")] = flutter::EncodableValue(callbackID);
            encoded_message[flutter::EncodableValue("message")] = flutter::EncodableValue(message);
            encoded_message[flutter::EncodableValue("msg_info")] = flutter::EncodableValue(msg_info);
            encoded_message[flutter::EncodableValue("cursorImage")] = flutter::EncodableValue(std::vector<uint8_t>(cursorImage.begin(), cursorImage.end()));
            if (channel_) {
                channel_->InvokeMethod("onCursorImageMessage", 
                    std::make_unique<flutter::EncodableValue>(encoded_message));
            }
        }, callbackID);
        result->Success(nullptr);
  } else if (method_call.method_name().compare("unhookCursorImage") == 0) {
        auto callbackID = static_cast<int>(std::get<int>((args->find(flutter::EncodableValue("callbackID")))->second));
        CursorMonitor::endHook(callbackID);
        result->Success(nullptr);
  } else if (method_call.method_name().compare("createGameController") == 0) {
        int hr = GameControllerManager::CreateGameController();
        result->Success(flutter::EncodableValue(hr));
  } else if (method_call.method_name().compare("removeGameController") == 0) {
        auto id = static_cast<int>(std::get<int>((args->find(flutter::EncodableValue("id")))->second));
        int hr = GameControllerManager::RemoveGameController(id);
        result->Success(flutter::EncodableValue(hr));
  } else if (method_call.method_name().compare("doControlAction") == 0) {
    if (args) {
        auto id_iter = args->find(flutter::EncodableValue("id"));
        auto action_iter = args->find(flutter::EncodableValue("action"));
        
        if (id_iter != args->end() && action_iter != args->end() &&
            std::holds_alternative<int>(id_iter->second) &&
            std::holds_alternative<std::string>(action_iter->second)) {
            
            int id = std::get<int>(id_iter->second);
            std::string action = std::get<std::string>(action_iter->second);

            GameControllerManager::DoControllerAction(id, action);
            result->Success(flutter::EncodableValue());
        } else {
            result->Error("InvalidArguments", "Missing or invalid arguments for doControlAction");
        }
    } else {
        result->Error("NullArguments", "Arguments are null for doControlAction");
    }
  }
  else {
    result->NotImplemented();
  }
}

}  // namespace hardware_simulator
