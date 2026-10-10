#include <windows.h>
#include <iostream>
#include "hooks/hooks.hpp"
#include "game/esp.hpp"

DWORD WINAPI MainThread(LPVOID lpParam) {
    HMODULE hModule = static_cast<HMODULE>(lpParam);

    // Initialize DirectX hooks
    if (!Hooks::Initialize()) {
        MessageBoxW(nullptr, L"Failed to initialize DirectX hooks in Outlast.", L"Outlast Internal ESP", MB_ICONERROR | MB_OK);
        FreeLibraryAndExitThread(hModule, 1);
        return 1;
    }

    // Start background reader thread
    g_Esp.Start();

    // Wait until user presses END key to unload
    while (true) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            break;
        }
        Sleep(100);
    }

    // Stop background worker before unhooking
    g_Esp.Stop();

    // Safely restore hooks and exit
    Hooks::Uninitialize();
    Sleep(200);

    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
