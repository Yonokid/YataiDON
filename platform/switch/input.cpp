#include "input.h"

#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <switch.h>

#include "controller-layout.h"

namespace {
    struct Controller {
        PadState pad{};
        SDL_Joystick* joystick = nullptr;
    };
    std::array<Controller, 9> controllers;
    bool initialized = false;
    constexpr std::array<u64, 28> buttons{
        HidNpadButton_A,          HidNpadButton_B,        HidNpadButton_X,           HidNpadButton_Y,
        HidNpadButton_StickL,     HidNpadButton_StickR,   HidNpadButton_L,           HidNpadButton_R,
        HidNpadButton_ZL,         HidNpadButton_ZR,       HidNpadButton_Plus,        HidNpadButton_Minus,
        HidNpadButton_Left,       HidNpadButton_Up,       HidNpadButton_Right,       HidNpadButton_Down,
        HidNpadButton_StickLLeft, HidNpadButton_StickLUp, HidNpadButton_StickLRight, HidNpadButton_StickLDown,
        HidNpadButton_StickRLeft, HidNpadButton_StickRUp, HidNpadButton_StickRRight, HidNpadButton_StickRDown,
        HidNpadButton_LeftSL,     HidNpadButton_LeftSR,   HidNpadButton_RightSL,     HidNpadButton_RightSR};

    void disconnect(Controller& controller) {
        const auto id = SDL_JoystickInstanceID(controller.joystick);
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_JoystickGetDeviceInstanceID(i) == id) {
                SDL_JoystickDetachVirtual(i);
                break;
            }
        }
        SDL_JoystickClose(controller.joystick);
        controller.joystick = nullptr;
    }
} // namespace

extern "C" void yataidon_switch_poll_controllers() {
    if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER)) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
            return;
        }
    }
    if (!initialized) {
        padConfigureInput(8, HidNpadStyleSet_NpadStandard);
        padInitialize(&controllers[0].pad, HidNpadIdType_Handheld);
        for (unsigned i = 1; i < controllers.size(); ++i) {
            padInitialize(&controllers[i].pad, static_cast<HidNpadIdType>(HidNpadIdType_No1 + i - 1));
        }
        initialized = true;
        SDL_JoystickEventState(SDL_ENABLE);
        SDL_GameControllerEventState(SDL_ENABLE);
    }
    for (unsigned slot = 0; slot < controllers.size(); ++slot) {
        auto& controller = controllers[slot];
        padUpdate(&controller.pad);
        if (!padIsConnected(&controller.pad)) {
            if (controller.joystick) {
                disconnect(controller);
            }
            continue;
        }
        if (!controller.joystick) {
            const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 4, buttons.size(), 0);
            if (index < 0) {
                continue;
            }
            char guid[33], mapping[512];
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
            std::snprintf(mapping, sizeof(mapping), "%s,%s", guid, switch_controls::mapping);
            if (SDL_GameControllerAddMapping(mapping) < 0) {
                SDL_JoystickDetachVirtual(index);
                continue;
            }
            controller.joystick = SDL_JoystickOpen(index);
            if (!controller.joystick) {
                SDL_JoystickDetachVirtual(index);
                continue;
            }
        }
        const u64 held = padGetButtons(&controller.pad);
        for (unsigned button = 0; button < buttons.size(); ++button) {
            SDL_JoystickSetVirtualButton(controller.joystick, button, (held & buttons[button]) != 0);
        }
        for (unsigned stick = 0; stick < 2; ++stick) {
            const auto position = padGetStickPos(&controller.pad, stick);
            SDL_JoystickSetVirtualAxis(controller.joystick, stick * 2, std::clamp(position.x, -32768, 32767));
            SDL_JoystickSetVirtualAxis(controller.joystick, stick * 2 + 1, std::clamp(-position.y, -32768, 32767));
        }
    }
}

extern "C" void yataidon_switch_close_controllers() {
    for (auto& controller : controllers) {
        if (controller.joystick) {
            disconnect(controller);
        }
    }
    initialized = false;
}
