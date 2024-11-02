#include "gamecontroller_manager.h"

#include <sstream>
#include <Xinput.h>

PVIGEM_CLIENT GameControllerManager::vigem_client = nullptr;
bool GameControllerManager::initialized = false;
std::array<PVIGEM_TARGET, 4> GameControllerManager::controllers = {};

int GameControllerManager::InitializeVigem() {
  vigem_client = vigem_alloc();
  if (vigem_client == nullptr) {
    std::cerr << "Uh, not enough memory to do that?!" << std::endl;
    return -1;
  }

  const auto retval = vigem_connect(vigem_client);
  if (!VIGEM_SUCCESS(retval)) {
    std::cerr << "ViGEm Bus connection failed with error code: 0x"
              << std::hex << retval << std::endl;
    return -1;
  }
  initialized = true;
  return 0;
}

int GameControllerManager::CreateGameController() {
  if (!initialized && InitializeVigem() != 0) {
    return -1;
  }

  for (int i = 0; i < 4; ++i) {
    if (controllers[i] == nullptr) {
      const auto pad = vigem_target_x360_alloc();
      controllers[i] = pad;

      //
      // Add client to the bus, this equals a plug-in event
      //
      const auto pir = vigem_target_add(vigem_client, pad);

      //
      // Error handling
      //
      if (!VIGEM_SUCCESS(pir)) {
        std::cerr << "Target plugin failed with error code: 0x" << std::hex
                << pir << std::endl;
        return -1;
      }
      std::cout << "GameController created in slot " << i + 1 << std::endl;
      return i + 1;
    }
  }

  std::cerr << "No available slot for GameController!" << std::endl;
  return -1;
}

bool GameControllerManager::RemoveGameController(int id) {
  if (id < 1 || id > 4) {
    std::cerr << "Invalid slot id: " << id << std::endl;
    return false;
  }

  int index = id - 1;
  if (controllers[index] != nullptr) {
    const auto pir = vigem_target_remove(vigem_client, controllers[index]);
    //
    // Error handling
    //
    if (!VIGEM_SUCCESS(pir)) {
        std::cerr << "GameController remove failed with error code: 0x" << std::hex
            << pir << std::endl;
        return false;
    }
    std::cout << "GameController removed from slot " << id << std::endl;
    return true;
  }

  std::cerr << "Slot " << id << " is already empty!" << std::endl;
  return false;
}

bool GameControllerManager::DoControllerAction(int id, std::string& action) {
    std::istringstream iss(action);

    _XINPUT_GAMEPAD gamepad;

    iss >> gamepad.wButtons;
    int nextparam;
    iss >> nextparam;
    gamepad.bLeftTrigger = (BYTE)nextparam;
    iss >> nextparam;
    gamepad.bRightTrigger = (BYTE)nextparam;

    iss >> gamepad.sThumbLX;
    iss >> gamepad.sThumbLY;
    iss >> gamepad.sThumbRX;
    iss >> gamepad.sThumbRY;

    const auto pir = vigem_target_x360_update(
        vigem_client, controllers[id - 1],
        *reinterpret_cast<XUSB_REPORT*>(&gamepad));

    if (!VIGEM_SUCCESS(pir)) {
        std::cerr << "GameController remove failed with error code: 0x" << std::hex
            << pir << std::endl;
        return false;
    }
    std::cout << "GameController removed from slot " << id << std::endl;
    return true;
}