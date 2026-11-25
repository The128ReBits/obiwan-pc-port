/* Lista los mandos que ve el port en Windows, con la misma configuracion de
 * SDL2 que src/input/xinput_device.c (XBOXRECOMP_SDL_PADS), y muestra en
 * vivo los botones del primero que no sea de Xbox.
 *
 *   listar_mandos.exe            lista y sale
 *   listar_mandos.exe --vivo     ademas muestra botones y ejes (Ctrl+C sale)
 *
 * Los mandos de Xbox van por XInput, como en el juego; el resto por SDL2. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "xinput.lib")

static const char *tipo(SDL_GameControllerType t)
{
    switch (t) {
    case SDL_CONTROLLER_TYPE_XBOX360:  return "Xbox 360 (va por XInput)";
    case SDL_CONTROLLER_TYPE_XBOXONE:  return "Xbox One (va por XInput)";
    case SDL_CONTROLLER_TYPE_PS3:      return "PlayStation 3";
    case SDL_CONTROLLER_TYPE_PS4:      return "PlayStation 4";
    case SDL_CONTROLLER_TYPE_PS5:      return "PlayStation 5";
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "Switch Pro";
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Joy-Con";
    case SDL_CONTROLLER_TYPE_GOOGLE_STADIA: return "Stadia";
    case SDL_CONTROLLER_TYPE_AMAZON_LUNA:   return "Luna";
    case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD: return "Shield";
    case SDL_CONTROLLER_TYPE_VIRTUAL:  return "virtual";
    default:                           return "generico";
    }
}

int main(int argc, char **argv)
{
    int vivo = argc > 1 && strcmp(argv[1], "--vivo") == 0;
    SDL_GameController *pad = NULL;
    int n;

    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL2 no arranca: %s\n", SDL_GetError());
        return 1;
    }
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_GameControllerEventState(SDL_IGNORE);
    SDL_Delay(500);                 /* da tiempo a HIDAPI a enumerar */
    SDL_GameControllerUpdate();

    for (DWORD i = 0; i < 4; i++) {
        XINPUT_STATE s;
        if (XInputGetState(i, &s) == ERROR_SUCCESS)
            printf("XInput %lu: mando de Xbox conectado\n", (unsigned long)i);
    }

    n = SDL_NumJoysticks();
    printf("SDL2 %d.%d.%d ve %d dispositivo(s)\n", SDL_MAJOR_VERSION,
           SDL_MINOR_VERSION, SDL_PATCHLEVEL, n);
    for (int i = 0; i < n; i++) {
        char guid[33];
        const char *name = SDL_JoystickNameForIndex(i);
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof guid);
        printf("  %d: %s | %s | %s | GUID %s\n", i, name ? name : "?",
               tipo(SDL_GameControllerTypeForIndex(i)),
               SDL_IsGameController(i) ? "con mapeo" : "SIN mapeo (no se usa)",
               guid);
        if (!pad && SDL_IsGameController(i)) {
            SDL_GameControllerType t = SDL_GameControllerTypeForIndex(i);
            if (t != SDL_CONTROLLER_TYPE_XBOX360 && t != SDL_CONTROLLER_TYPE_XBOXONE)
                pad = SDL_GameControllerOpen(i);
        }
    }

    if (vivo && pad) {
        printf("Mostrando %s (Ctrl+C para salir)\n", SDL_GameControllerName(pad));
        for (;;) {
            static const char *nb[] = { "A", "B", "X", "Y", "Back", "Guia",
                "Start", "L3", "R3", "LB", "RB", "Arriba", "Abajo",
                "Izq", "Der" };
            SDL_GameControllerUpdate();
            printf("\r");
            for (int b = 0; b < (int)(sizeof nb / sizeof nb[0]); b++)
                if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b))
                    printf("%s ", nb[b]);
            printf("| LX %6d LY %6d RX %6d RY %6d LT %5d RT %5d      ",
                   SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX),
                   SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY),
                   SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX),
                   SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY),
                   SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                   SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
            fflush(stdout);
            SDL_Delay(50);
        }
    } else if (vivo) {
        printf("No hay ningun mando que no sea de Xbox para mostrar.\n");
    }
    SDL_Quit();
    return 0;
}
