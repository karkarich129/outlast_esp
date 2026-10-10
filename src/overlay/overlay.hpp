#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <functional>

class Overlay {
public:
    Overlay();
    ~Overlay();

    bool Initialize(const wchar_t* targetWindowTitle, const wchar_t* targetClassName = nullptr);
    void Run(const std::function<void()>& renderCallback);
    void Shutdown();

    HWND GetHwnd() const { return m_hWnd; }
    HWND GetTargetHwnd() const { return m_targetHwnd; }
    float GetWidth() const { return static_cast<float>(m_width); }
    float GetHeight() const { return static_cast<float>(m_height); }

    void SetClickThrough(bool enable);

private:
    HWND m_hWnd = nullptr;
    HWND m_targetHwnd = nullptr;
    int m_width = 1920;
    int m_height = 1080;
    int m_posX = 0;
    int m_posY = 0;
    bool m_isClickThrough = true;

    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;

    bool CreateOverlayWindow();
    bool CreateDeviceD3D();
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void UpdateTargetPosition();

    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

inline Overlay g_Overlay;
