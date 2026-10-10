#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "overlay/overlay.hpp"
#include "memory/memory.hpp"
#include "game/esp.hpp"
#include "game/offsets.hpp"
#include "config.hpp"

std::atomic<bool> g_Running{ true };

void MemoryWorker() {
    while (g_Running) {
        g_Esp.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void InputWorker() {
    bool insertWasPressed = false;

    while (g_Running) {
        // Toggle menu with INSERT key
        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertWasPressed) {
                g_Settings.menuOpen = !g_Settings.menuOpen;
                insertWasPressed = true;
            }
        } else {
            insertWasPressed = false;
        }

        // Exit with END key
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            g_Running = false;
            PostQuitMessage(0);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

int main() {
    std::cout << "[Outlast ESP] Initializing overlay..." << std::endl;

    // Initialize Overlay
    if (!g_Overlay.Initialize(Offsets::WindowTitle)) {
        std::cerr << "[Outlast ESP] Failed to create overlay window or DirectX 11 device." << std::endl;
        MessageBoxW(nullptr, L"Failed to create overlay window or DirectX 11 device.", L"Outlast ESP Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    std::cout << "[Outlast ESP] Overlay active! Waiting for Outlast 1 (OLGame.exe)..." << std::endl;
    std::cout << "[Outlast ESP] Press INSERT to toggle menu. Press END to quit." << std::endl;

    // Start background threads
    std::thread memThread(MemoryWorker);
    std::thread inputThread(InputWorker);

    // Main render loop
    g_Overlay.Run([]() {
        g_Esp.Render();
        g_Esp.RenderMenu();
    });

    g_Running = false;
    if (memThread.joinable()) memThread.join();
    if (inputThread.joinable()) inputThread.join();

    g_Overlay.Shutdown();
    g_Memory.Detach();

    std::cout << "[Outlast ESP] Exited cleanly." << std::endl;
    return 0;
}
