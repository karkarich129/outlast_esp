#pragma once
#include <cstdint>

struct ColorRGB {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Settings {
    // Menu state
    bool menuOpen = true;

    // Cheats (Native MinHook)
    bool godMode = false;

    // ESP Master toggle
    bool espEnabled = true;

    // Enemy ESP
    bool enemyEsp = true;
    bool enemyBox = true;
    bool enemyBoxFilled = false;
    int enemyBoxType = 0; // 0 = 2D Box, 1 = Corner Box
    bool enemySnaplines = true;
    int snaplineOrigin = 0; // 0 = Bottom Screen, 1 = Center Screen
    bool enemyDistance = true;
    bool enemyHealth = true;
    bool enemyName = true;
    float enemyMaxDistance = 100.0f; // meters

    // Item ESP
    bool itemEsp = true;
    bool itemBatteries = true;
    bool itemDocuments = true;        // Physical Document Pickups
    bool itemNoteEvents = false;      // Camcorder Note Recording Spots (Triggers)
    bool itemKeys = true;
    bool itemDistance = true;
    float itemMaxDistance = 35.0f;
    bool filterHiddenItems = true;    // Ignore bHidden (already collected / invisible) actors
    bool showDebugEntityInfo = false; // Display ClassName and ActorName on screen

    // Hiding Spots & Interactive ESP
    bool hideSpotsEsp = true;       // Lockers & Beds
    float hideSpotMaxDistance = 40.0f;
    bool interactiveEsp = true;     // Valves, Wheels, Levers, Buttons, Switches, Pumps, Generators
    float interactiveMaxDistance = 60.0f;
    bool doorEsp = false;           // Doors
    float doorMaxDistance = 30.0f;

    // Crosshair & Visuals
    bool drawCrosshair = false;
    float crosshairSize = 6.0f;

    // Colors
    ColorRGB enemyColor{ 1.0f, 0.2f, 0.2f, 1.0f };       // Red
    ColorRGB friendlyColor{ 0.2f, 0.8f, 0.2f, 1.0f };    // Green
    ColorRGB itemColor{ 0.2f, 0.6f, 1.0f, 1.0f };        // Blue
    ColorRGB batteryColor{ 1.0f, 0.9f, 0.2f, 1.0f };     // Yellow
    ColorRGB snaplineColor{ 1.0f, 0.3f, 0.3f, 0.8f };
    ColorRGB hideSpotColor{ 0.75f, 0.45f, 1.0f, 1.0f };  // Purple/Magenta
    ColorRGB interactiveColor{ 1.0f, 0.65f, 0.0f, 1.0f };// Orange/Gold

    // Overlay FPS limit
    int maxFps = 144;
};

inline Settings g_Settings;
