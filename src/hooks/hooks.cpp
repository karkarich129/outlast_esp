#include "hooks.hpp"
#include <iostream>
#include <d3d9.h>
#include <d3d11.h>
#include "../../ext/minhook/include/MinHook.h"
#include "../../ext/imgui/imgui.h"
#include "../../ext/imgui/backends/imgui_impl_win32.h"
#include "../../ext/imgui/backends/imgui_impl_dx9.h"
#include "../../ext/imgui/backends/imgui_impl_dx11.h"
#include "../game/esp.hpp"
#include "../config.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// D3D9 Types
typedef HRESULT(WINAPI* EndScene_t)(IDirect3DDevice9* pDevice);
typedef HRESULT(WINAPI* Reset_t)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

// D3D11 Types
typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(WINAPI* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

// Gameplay Types (Outlast 1 AOLHero)
typedef __int64 (__fastcall* TakeDamage_t)(void* thisPtr, int damage, void* instigator, void* hitLoc, void* dmgType);
typedef __int64 (__fastcall* TryKillHero_t)(void* thisPtr, void* enemyPawn, float* a3, float* a4);
typedef __int64 (__fastcall* TryDecapitate_t)(void* thisPtr, void* enemyPawn);

static EndScene_t oEndScene = nullptr;
static Reset_t oReset = nullptr;
static Present_t oPresent = nullptr;
static ResizeBuffers_t oResizeBuffers = nullptr;
static WNDPROC oWndProc = nullptr;

static TakeDamage_t oTakeDamage = nullptr;
static TryKillHero_t oTryKillHero = nullptr;
static TryDecapitate_t oTryDecapitate = nullptr;

static __int64 __fastcall hkTakeDamage(void* thisPtr, int damage, void* instigator, void* hitLoc, void* dmgType) {
    if (g_Settings.godMode) {
        return 0;
    }
    return oTakeDamage(thisPtr, damage, instigator, hitLoc, dmgType);
}

static __int64 __fastcall hkTryKillHero(void* thisPtr, void* enemyPawn, float* a3, float* a4) {
    if (g_Settings.godMode) {
        return 0;
    }
    return oTryKillHero(thisPtr, enemyPawn, a3, a4);
}

static __int64 __fastcall hkTryDecapitate(void* thisPtr, void* enemyPawn) {
    if (g_Settings.godMode) {
        return 0;
    }
    return oTryDecapitate(thisPtr, enemyPawn);
}

static bool g_D3D9Initialized = false;
static bool g_D3D11Initialized = false;

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool g_WndProcHooked = false;

namespace Hooks {
    HWND g_hWindow = nullptr;

    static LRESULT WINAPI hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static void HookWindowProc(HWND hWnd) {
        if (!g_WndProcHooked && hWnd) {
            WNDPROC prev = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hkWndProc)));
            if (prev && prev != reinterpret_cast<WNDPROC>(hkWndProc)) {
                oWndProc = prev;
            }
            g_WndProcHooked = true;
        }
    }

    static LRESULT WINAPI hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
            g_Settings.menuOpen = !g_Settings.menuOpen;
            return 0;
        }

        if (g_Settings.menuOpen) {
            ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
                return 1;
            }
        }

        if (oWndProc && oWndProc != reinterpret_cast<WNDPROC>(hkWndProc)) {
            return CallWindowProcW(oWndProc, hWnd, uMsg, wParam, lParam);
        }
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    // ==========================================
    // DirectX 9 Hooks (Outlast default renderer)
    // ==========================================
    static HRESULT WINAPI hkEndScene(IDirect3DDevice9* pDevice) {
        if (g_D3D11Initialized) {
            return oEndScene(pDevice);
        }

        if (!g_D3D9Initialized) {
            D3DDEVICE_CREATION_PARAMETERS cp;
            pDevice->GetCreationParameters(&cp);
            g_hWindow = cp.hFocusWindow;

            if (g_hWindow) {
                HookWindowProc(g_hWindow);

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

                ImGui::StyleColorsDark();
                ImGuiStyle& style = ImGui::GetStyle();
                style.WindowRounding = 6.0f;
                style.FrameRounding = 4.0f;
                style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.13f, 0.94f);
                style.Colors[ImGuiCol_Border] = ImVec4(0.24f, 0.52f, 0.95f, 0.5f);

                ImGui_ImplWin32_Init(g_hWindow);
                ImGui_ImplDX9_Init(pDevice);

                g_D3D9Initialized = true;
            }
        }

        if (g_D3D9Initialized) {
            g_Esp.Update();

            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            g_Esp.Render();
            g_Esp.RenderMenu();

            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }

        return oEndScene(pDevice);
    }

    static HRESULT WINAPI hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
        if (g_D3D9Initialized) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
        }
        HRESULT hr = oReset(pDevice, pPresentationParameters);
        if (g_D3D9Initialized && SUCCEEDED(hr)) {
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        return hr;
    }

    // ==========================================
    // DirectX 11 Hooks (For -d3d11 mode)
    // ==========================================
    static void CreateRenderTarget(IDXGISwapChain* pSwapChain) {
        ID3D11Texture2D* pBackBuffer = nullptr;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
    }

    static void CleanupRenderTarget() {
        if (g_mainRenderTargetView) {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
    }

    static HRESULT WINAPI hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
        CleanupRenderTarget();
        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        CreateRenderTarget(pSwapChain);
        return hr;
    }

    static HRESULT WINAPI hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (g_D3D9Initialized) {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }

        if (!g_D3D11Initialized) {
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_pd3dDevice)))) {
                g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

                DXGI_SWAP_CHAIN_DESC desc;
                pSwapChain->GetDesc(&desc);
                g_hWindow = desc.OutputWindow;

                CreateRenderTarget(pSwapChain);

                HookWindowProc(g_hWindow);

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

                ImGui::StyleColorsDark();
                ImGui_ImplWin32_Init(g_hWindow);
                ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

                g_D3D11Initialized = true;
            }
        }

        if (g_D3D11Initialized && g_mainRenderTargetView) {
            g_Esp.Update();

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            g_Esp.Render();
            g_Esp.RenderMenu();

            ImGui::Render();
            g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    bool Initialize() {
        if (MH_Initialize() != MH_OK) {
            return false;
        }

        WNDCLASSEXW wc = {
            sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW, 0L, 0L,
            GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
            L"DummyWindowClass", nullptr
        };
        RegisterClassExW(&wc);
        HWND dummyHwnd = CreateWindowW(wc.lpszClassName, L"Dummy", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        // 1. Hook DirectX 9
        IDirect3D9* pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
        if (pD3D9) {
            D3DPRESENT_PARAMETERS d3dpp{};
            d3dpp.Windowed = TRUE;
            d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            d3dpp.hDeviceWindow = dummyHwnd;

            IDirect3DDevice9* pDummyDevice9 = nullptr;
            if (SUCCEEDED(pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummyHwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice9))) {
                void** pVTable9 = *reinterpret_cast<void***>(pDummyDevice9);
                void* pResetTarget = pVTable9[16];
                void* pEndSceneTarget = pVTable9[42];

                MH_CreateHook(pResetTarget, reinterpret_cast<void*>(&hkReset), reinterpret_cast<void**>(&oReset));
                MH_CreateHook(pEndSceneTarget, reinterpret_cast<void*>(&hkEndScene), reinterpret_cast<void**>(&oEndScene));

                pDummyDevice9->Release();
            }
            pD3D9->Release();
        }

        // 2. Hook DirectX 11
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = dummyHwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* dummySwapChain = nullptr;
        ID3D11Device* dummyDevice = nullptr;
        ID3D11DeviceContext* dummyContext = nullptr;

        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, 2, D3D11_SDK_VERSION, &sd,
            &dummySwapChain, &dummyDevice, &featureLevel, &dummyContext))) {

            void** pVTable11 = *reinterpret_cast<void***>(dummySwapChain);
            void* pPresentTarget = pVTable11[8];
            void* pResizeBuffersTarget = pVTable11[13];

            MH_CreateHook(pPresentTarget, reinterpret_cast<void*>(&hkPresent), reinterpret_cast<void**>(&oPresent));
            MH_CreateHook(pResizeBuffersTarget, reinterpret_cast<void*>(&hkResizeBuffers), reinterpret_cast<void**>(&oResizeBuffers));

            dummySwapChain->Release();
            dummyContext->Release();
            dummyDevice->Release();
        }

        // Hook native AOLHero gameplay functions in OLGame.exe
        uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (moduleBase) {
            void* pTakeDamage    = reinterpret_cast<void*>(moduleBase + 0xD8A140);
            void* pTryKillHero   = reinterpret_cast<void*>(moduleBase + 0xD69870);
            void* pTryDecapitate = reinterpret_cast<void*>(moduleBase + 0xD697B0);

            MH_CreateHook(pTakeDamage, reinterpret_cast<void*>(&hkTakeDamage), reinterpret_cast<void**>(&oTakeDamage));
            MH_CreateHook(pTryKillHero, reinterpret_cast<void*>(&hkTryKillHero), reinterpret_cast<void**>(&oTryKillHero));
            MH_CreateHook(pTryDecapitate, reinterpret_cast<void*>(&hkTryDecapitate), reinterpret_cast<void**>(&oTryDecapitate));
        }

        MH_EnableHook(MH_ALL_HOOKS);

        DestroyWindow(dummyHwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        return true;
    }

    void Uninitialize() {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        if (oWndProc && g_hWindow) {
            SetWindowLongPtrW(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
            oWndProc = nullptr;
        }

        if (g_D3D9Initialized) {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_D3D9Initialized = false;
        }

        if (g_D3D11Initialized) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            CleanupRenderTarget();
            if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
            if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
            g_D3D11Initialized = false;
        }
    }
}
