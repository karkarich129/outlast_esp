#include "overlay.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "../config.hpp"
#include <thread>
#include <chrono>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Overlay::Overlay() = default;

Overlay::~Overlay() {
    Shutdown();
}

bool Overlay::Initialize(const wchar_t* targetWindowTitle, const wchar_t* targetClassName) {
    m_targetHwnd = FindWindowW(targetClassName, targetWindowTitle);
    if (!m_targetHwnd) {
        // Fallback: search by class name or substring
        m_targetHwnd = FindWindowW(L"LaunchUnrealUWindowsClient", nullptr);
    }

    if (!CreateOverlayWindow()) {
        return false;
    }

    if (!CreateDeviceD3D()) {
        CleanupDeviceD3D();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.13f, 0.92f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.24f, 0.52f, 0.95f, 0.5f);

    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_d3dDevice, m_d3dContext);

    return true;
}

void Overlay::SetClickThrough(bool enable) {
    if (m_isClickThrough == enable) return;
    m_isClickThrough = enable;

    LONG_PTR exStyle = GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE);
    if (enable) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(m_hWnd, GWL_EXSTYLE, exStyle);
}

void Overlay::UpdateTargetPosition() {
    if (!m_targetHwnd || !IsWindow(m_targetHwnd)) {
        // Attempt to find window again
        m_targetHwnd = FindWindowW(L"LaunchUnrealUWindowsClient", nullptr);
        if (!m_targetHwnd) {
            m_targetHwnd = FindWindowW(nullptr, L"Outlast");
        }
        if (!m_targetHwnd) return;
    }

    RECT clientRect{};
    if (GetClientRect(m_targetHwnd, &clientRect)) {
        POINT pt{ clientRect.left, clientRect.top };
        ClientToScreen(m_targetHwnd, &pt);

        int newWidth = clientRect.right - clientRect.left;
        int newHeight = clientRect.bottom - clientRect.top;

        if (newWidth > 0 && newHeight > 0) {
            if (pt.x != m_posX || pt.y != m_posY || newWidth != m_width || newHeight != m_height) {
                m_posX = pt.x;
                m_posY = pt.y;
                m_width = newWidth;
                m_height = newHeight;

                SetWindowPos(m_hWnd, HWND_TOPMOST, m_posX, m_posY, m_width, m_height, SWP_SHOWWINDOW);

                if (m_swapChain) {
                    CleanupRenderTarget();
                    m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
                    CreateRenderTarget();
                }
            }
        }
    }
}

void Overlay::Run(const std::function<void()>& renderCallback) {
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        // Toggle click-through based on menu visibility
        SetClickThrough(!g_Settings.menuOpen);

        UpdateTargetPosition();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (renderCallback) {
            renderCallback();
        }

        ImGui::Render();
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_d3dContext->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
        m_d3dContext->ClearRenderTargetView(m_renderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_swapChain->Present(1, 0);

        if (g_Settings.maxFps > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / g_Settings.maxFps));
        }
    }
}

void Overlay::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        UnregisterClassW(L"OutlastOverlayClass", GetModuleHandleW(nullptr));
        m_hWnd = nullptr;
    }
}

bool Overlay::CreateOverlayWindow() {
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandleW(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"OutlastOverlayClass",
        nullptr
    };
    RegisterClassExW(&wc);

    m_width = GetSystemMetrics(SM_CXSCREEN);
    m_height = GetSystemMetrics(SM_CYSCREEN);

    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Outlast Overlay",
        WS_POPUP,
        0, 0, m_width, m_height,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!m_hWnd) return false;

    // Set transparency and composition
    SetLayeredWindowAttributes(m_hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hWnd, &margins);

    ShowWindow(m_hWnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hWnd);

    return true;
}

bool Overlay::CreateDeviceD3D() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_swapChain,
        &m_d3dDevice,
        &featureLevel,
        &m_d3dContext
    );

    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void Overlay::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_d3dContext) { m_d3dContext->Release(); m_d3dContext = nullptr; }
    if (m_d3dDevice) { m_d3dDevice->Release(); m_d3dDevice = nullptr; }
}

void Overlay::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
        pBackBuffer->Release();
    }
}

void Overlay::CleanupRenderTarget() {
    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
}

LRESULT WINAPI Overlay::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
    case WM_SIZE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
