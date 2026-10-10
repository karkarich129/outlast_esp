#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "math.hpp"

enum class EntityType {
    Unknown,
    Player,
    Enemy,        // Chris Walker, Walrider, Trager, Variant
    Battery,      // Night vision battery
    Document,     // Physical collectible document (folder)
    NoteEvent,    // Camcorder note event (trigger)
    KeyItem,      // Door key or quest item
    HideSpot,     // Locker, Bed
    Interactive,  // Switch, Lever, Button, Valve
    Door          // Doors
};

struct Entity {
    uintptr_t address = 0;
    EntityType type = EntityType::Unknown;
    std::string name = "Actor";
    std::string className = "";
    std::string actorName = "";
    Vector3 location{};
    FRotator rotation{};
    float distance = 0.0f;
    float health = 100.0f;
    float maxHealth = 100.0f;
    uint32_t color = 0xFFFFFFFF; // ImU32 RGBA color
    bool isValid = false;
};

struct CameraData {
    Vector3 location{};
    FRotator rotation{};
    float fov = 90.0f;
    bool isValid = false;
};
