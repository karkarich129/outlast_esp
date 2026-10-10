#pragma once
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>

namespace Hooks {
    bool Initialize();
    void Uninitialize();

    extern HWND g_hWindow;
}
