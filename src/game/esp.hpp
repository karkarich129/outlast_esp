#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "sdk.hpp"
#include "imgui.h"

class Esp {
public:
    Esp();
    ~Esp();

    void Start();
    void Stop();

    void Update();
    void Render();
    void RenderMenu();

    CameraData ReadCamera();
    std::string GetNameFromIndex(int32_t index);
    std::string GetActorClassName(uintptr_t actor);
    std::string GetActorName(uintptr_t actor);

private:
    std::vector<Entity> m_entities;
    CameraData m_camera;
    std::mutex m_mutex;

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    std::unordered_map<int32_t, std::string> m_nameCache;
    std::unordered_map<uintptr_t, std::string> m_classCache;

    uintptr_t m_gWorldAddress    = 0;
    uintptr_t m_gEngineAddress   = 0;
    uintptr_t m_gNamesAddress    = 0;
    uintptr_t m_gNamesNumAddress = 0;
    uint32_t m_cachedCameraOffset = 0;
    bool m_isAttached = false;

    void WorkerLoop();
    void ResolveAddresses();
    void ReadGameData();

    void DrawBox2D(ImDrawList* drawList, const Vector2& top, const Vector2& bottom, float width, ImU32 color, bool filled);
    void DrawCornerBox(ImDrawList* drawList, const Vector2& top, const Vector2& bottom, float width, ImU32 color);
    void DrawSnapline(ImDrawList* drawList, const Vector2& screenPos, ImU32 color);
    void DrawHealthBar(ImDrawList* drawList, const Vector2& top, const Vector2& bottom, float width, float health, float maxHealth);
    void DrawItemESP(ImDrawList* drawList, const Entity& ent, const Vector2& screenPos);
};

inline Esp g_Esp;
