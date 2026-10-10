#include "esp.hpp"
#include "offsets.hpp"
#include "../config.hpp"
#include <cstdio>
#include <algorithm>
#include <windows.h>
#include <psapi.h>

template<typename T>
inline bool SafeReadRef(uintptr_t address, T& outVal) {
    if (!address) return false;
    __try {
        outVal = *reinterpret_cast<const T*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<typename T>
inline T SafeRead(uintptr_t address, T defaultVal = {}) {
    T val = defaultVal;
    SafeReadRef(address, val);
    return val;
}

inline bool SafeReadBuf(uintptr_t address, void* buffer, size_t size) {
    if (!address || !buffer || size == 0) return false;
    __try {
        memcpy(buffer, reinterpret_cast<const void*>(address), size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static inline bool IsValidPointer(uintptr_t ptr) {
    return (ptr >= 0x10000ULL && ptr <= 0x7FFFFFFFFFFFULL && (ptr & 0x3) == 0);
}

static bool ContainsCi(const std::string& str, std::string_view sub) {
    if (sub.empty() || str.size() < sub.size()) return false;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2)); }
    );
    return it != str.end();
}

static ImU32 ToImU32(const ColorRGB& c) {
    return ImColor(c.r, c.g, c.b, c.a);
}

Esp::Esp() = default;

Esp::~Esp() {
    Stop();
}

void Esp::Start() {
    if (m_running.exchange(true)) return;
    m_workerThread = std::thread(&Esp::WorkerLoop, this);
}

void Esp::Stop() {
    if (!m_running.exchange(false)) return;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void Esp::WorkerLoop() {
    while (m_running.load()) {
        if (m_gWorldAddress == 0 || m_gNamesAddress == 0) {
            ResolveAddresses();
        }
        ReadGameData();
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // ~50 Hz update rate
    }
}

std::string Esp::GetNameFromIndex(int32_t index) {
    if (index < 0 || m_gNamesAddress == 0) return "";

    // Fast O(1) hash lookup from cache
    auto it = m_nameCache.find(index);
    if (it != m_nameCache.end()) {
        return it->second;
    }

    // Bounds check using GNames.Num if available
    if (m_gNamesNumAddress != 0) {
        int32_t maxNames = SafeRead<int32_t>(m_gNamesNumAddress, 0);
        if (maxNames > 0 && index >= maxNames) {
            return "";
        }
    } else if (index > 65536) {
        return "";
    }

    uintptr_t gNamesPtr = SafeRead<uintptr_t>(m_gNamesAddress);
    if (!gNamesPtr) return "";

    uintptr_t entry = SafeRead<uintptr_t>(gNamesPtr + index * sizeof(uintptr_t));
    if (!entry) return "";

    uint32_t flags = SafeRead<uint32_t>(entry + 8);
    char nameBuf[64] = { 0 };

    if (flags & 1) {
        // Unicode (wchar_t) string - single batch read of 64 wchars
        wchar_t wbuf[64] = { 0 };
        if (SafeReadBuf(entry + 0x14, wbuf, sizeof(wbuf))) {
            for (int i = 0; i < 63 && wbuf[i] != L'\0'; ++i) {
                if (wbuf[i] >= 32 && wbuf[i] <= 126) {
                    nameBuf[i] = static_cast<char>(wbuf[i]);
                } else {
                    break;
                }
            }
        }
    } else {
        // Ansi string - single batch read of 64 chars
        char cbuf[64] = { 0 };
        if (SafeReadBuf(entry + 0x14, cbuf, sizeof(cbuf))) {
            for (int i = 0; i < 63 && cbuf[i] != '\0'; ++i) {
                if (static_cast<unsigned char>(cbuf[i]) >= 32 && static_cast<unsigned char>(cbuf[i]) <= 126) {
                    nameBuf[i] = cbuf[i];
                } else {
                    break;
                }
            }
        }
    }

    std::string result(nameBuf);
    m_nameCache[index] = result;
    return result;
}

std::string Esp::GetActorClassName(uintptr_t actor) {
    if (!actor) return "";
    uintptr_t uClass = SafeRead<uintptr_t>(actor + Offsets::UE3::ObjectClass);
    if (!uClass) return "";

    auto it = m_classCache.find(uClass);
    if (it != m_classCache.end()) {
        return it->second;
    }

    int32_t classIndex = SafeRead<int32_t>(uClass + Offsets::UE3::ObjectName);
    std::string className = GetNameFromIndex(classIndex);
    if (!className.empty()) {
        m_classCache[uClass] = className;
    }
    return className;
}

std::string Esp::GetActorName(uintptr_t actor) {
    if (!actor) return "";
    int32_t nameIndex = SafeRead<int32_t>(actor + Offsets::UE3::ObjectName);
    return GetNameFromIndex(nameIndex);
}

void Esp::ResolveAddresses() {
    uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!moduleBase) return;

    m_gWorldAddress    = moduleBase + Offsets::UE3::GWorld;
    m_gEngineAddress   = moduleBase + Offsets::UE3::GEngine;
    m_gNamesAddress    = moduleBase + Offsets::UE3::GNames;
    m_gNamesNumAddress = moduleBase + Offsets::UE3::GNames + 0x8;
}

void Esp::Update() {
    // Ensure background worker thread is alive
    if (!m_running.load()) {
        Start();
    }
}

CameraData Esp::ReadCamera() {
    CameraData cam{};
    if (m_gWorldAddress == 0 || m_gEngineAddress == 0) {
        ResolveAddresses();
    }
    if (m_gWorldAddress == 0 && m_gEngineAddress == 0) return cam;

    uintptr_t uEngine = m_gEngineAddress ? SafeRead<uintptr_t>(m_gEngineAddress) : 0;
    uintptr_t uWorld  = m_gWorldAddress  ? SafeRead<uintptr_t>(m_gWorldAddress)  : 0;

    uintptr_t localPlayer = 0;
    if (uEngine) {
        uintptr_t playersData = SafeRead<uintptr_t>(uEngine + Offsets::UE3::EngineGamePlayers);
        int32_t playersCount  = SafeRead<int32_t>(uEngine + Offsets::UE3::EnginePlayersCount);
        if (playersData && playersCount > 0) {
            localPlayer = SafeRead<uintptr_t>(playersData);
        }
    }
    if (!localPlayer && uWorld) {
        uintptr_t worldPlayers = SafeRead<uintptr_t>(uWorld + Offsets::UE3::WorldGamePlayers);
        if (worldPlayers) {
            uintptr_t playersData = SafeRead<uintptr_t>(worldPlayers + 0x60);
            if (playersData) localPlayer = SafeRead<uintptr_t>(playersData);
            if (!localPlayer) localPlayer = SafeRead<uintptr_t>(worldPlayers);
        }
    }

    if (!localPlayer) return cam;

    uintptr_t playerController = SafeRead<uintptr_t>(localPlayer + Offsets::UE3::LocalPlayerController);
    if (!playerController) {
        playerController = SafeRead<uintptr_t>(localPlayer + 0x70);
    }
    if (!playerController) return cam;

    float defaultFov = SafeRead<float>(playerController + Offsets::UE3::PlayerDefaultFOV, 80.0f);
    if (defaultFov < 20.0f || defaultFov > 160.0f) defaultFov = 80.0f;

    uintptr_t playerCamera = SafeRead<uintptr_t>(playerController + Offsets::UE3::PlayerCamera);
    bool camFound = false;

    if (playerCamera) {
        // Fast path: test previously cached camera offset
        if (m_cachedCameraOffset != 0) {
            Vector3 testLoc = SafeRead<Vector3>(playerCamera + m_cachedCameraOffset);
            FRotator testRot = SafeRead<FRotator>(playerCamera + m_cachedCameraOffset + 0x0C);
            float testFov = SafeRead<float>(playerCamera + m_cachedCameraOffset + 0x18);

            if (testFov >= 35.0f && testFov <= 140.0f && testLoc.Length() > 10.0f) {
                cam.location = testLoc;
                cam.rotation = testRot;
                cam.fov = testFov;
                cam.isValid = true;
                camFound = true;
                return cam;
            }
            m_cachedCameraOffset = 0;
        }

        // Test common UE3 camera / POV offsets (including +4 for TimeStamp in FCameraCacheEntry)
        static const uint32_t candidateOffsets[] = {
            0x458u, 0x46Cu, 0x470u, 0x468u, 0x474u, 0x478u, 0x45Cu, 0x460u, 0x464u,
            0x448u, 0x44Cu, 0x450u, 0x480u, 0x484u, 0x488u, 0x498u, 0x4A0u,
            0x26Cu, 0x270u, 0x268u, 0x274u, 0x278u, 0x258u, 0x25Cu, 0x260u, 0x264u,
            0x248u, 0x24Cu, 0x250u
        };

        for (uint32_t offset : candidateOffsets) {
            Vector3 testLoc = SafeRead<Vector3>(playerCamera + offset);
            FRotator testRot = SafeRead<FRotator>(playerCamera + offset + 0x0C);
            float testFov = SafeRead<float>(playerCamera + offset + 0x18);

            if (testFov >= 35.0f && testFov <= 140.0f && testLoc.Length() > 10.0f) {
                cam.location = testLoc;
                cam.rotation = testRot;
                cam.fov = testFov;
                cam.isValid = true;
                camFound = true;
                m_cachedCameraOffset = offset;
                break;
            }
        }

        // Search full offset range if not found in candidates
        if (!camFound) {
            for (uint32_t offset = 0x200u; offset <= 0x580u; offset += 4u) {
                Vector3 testLoc = SafeRead<Vector3>(playerCamera + offset);
                FRotator testRot = SafeRead<FRotator>(playerCamera + offset + 0x0C);
                float testFov = SafeRead<float>(playerCamera + offset + 0x18);

                if (testFov >= 35.0f && testFov <= 140.0f && testLoc.Length() > 10.0f) {
                    cam.location = testLoc;
                    cam.rotation = testRot;
                    cam.fov = testFov;
                    cam.isValid = true;
                    camFound = true;
                    m_cachedCameraOffset = offset;
                    break;
                }
            }
        }
    }

    if (!camFound) {
        uintptr_t playerPawn = SafeRead<uintptr_t>(playerController + Offsets::UE3::PlayerPawn);
        Vector3 camLoc{};
        if (playerPawn) {
            camLoc = SafeRead<Vector3>(playerPawn + Offsets::UE3::ActorLocation);
            camLoc.z += 64.0f; // Eye height in Outlast (OLHero base eye height)
        }
        if (camLoc.Length() < 10.0f) {
            camLoc = SafeRead<Vector3>(playerController + Offsets::UE3::ActorLocation);
        }

        FRotator camRot = SafeRead<FRotator>(playerController + Offsets::UE3::ActorRotation);

        if (camLoc.Length() > 10.0f) {
            cam.location = camLoc;
            cam.rotation = camRot;
            cam.fov = defaultFov;
            cam.isValid = true;
        }
    }

    return cam;
}

void Esp::ReadGameData() {
    if (m_gWorldAddress == 0) return;

    uintptr_t uWorld = SafeRead<uintptr_t>(m_gWorldAddress);
    if (!uWorld) return;

    CameraData newCam = ReadCamera();

    // Collect levels from LevelsArray
    uintptr_t levelsArray = SafeRead<uintptr_t>(uWorld + Offsets::UE3::LevelsArray);
    int32_t levelsCount = SafeRead<int32_t>(uWorld + Offsets::UE3::LevelsCount);

    std::vector<uintptr_t> levels;
    if (levelsArray && levelsCount > 0 && levelsCount <= 64) {
        for (int32_t i = 0; i < levelsCount; ++i) {
            uintptr_t lvl = SafeRead<uintptr_t>(levelsArray + (i * sizeof(uintptr_t)));
            if (lvl) levels.push_back(lvl);
        }
    }

    // Fallback: check PersistentLevel
    uintptr_t persistentLevel = SafeRead<uintptr_t>(uWorld + Offsets::UE3::PersistentLevel);
    if (persistentLevel) {
        bool alreadyIncluded = false;
        for (uintptr_t lvl : levels) {
            if (lvl == persistentLevel) {
                alreadyIncluded = true;
                break;
            }
        }
        if (!alreadyIncluded) {
            levels.push_back(persistentLevel);
        }
    }

    if (levels.empty()) return;

    std::vector<Entity> newEntities;
    Vector3 heroLocation{};
    FRotator heroRotation{};
    bool foundHero = false;

    for (uintptr_t level : levels) {
        uintptr_t actorsArray = SafeRead<uintptr_t>(level + Offsets::UE3::ActorsArray);
        int32_t actorsCount = SafeRead<int32_t>(level + Offsets::UE3::ActorsCount);

        if (!actorsArray || actorsCount <= 0 || actorsCount > 8192) continue;

        for (int32_t i = 0; i < actorsCount; ++i) {
            uintptr_t actor = SafeRead<uintptr_t>(actorsArray + (i * sizeof(uintptr_t)));
            if (!IsValidPointer(actor)) continue;

            // Validate UObject: Class pointer must be valid user-mode address
            uintptr_t uClass = SafeRead<uintptr_t>(actor + Offsets::UE3::ObjectClass);
            if (!IsValidPointer(uClass)) continue;

            std::string className = GetActorClassName(actor);
            if (className.empty()) continue;

            // Read Location with RootComponent fallback for mesh/interactive actors
            Vector3 loc = SafeRead<Vector3>(actor + Offsets::UE3::ActorLocation);
            if (loc.Length() < 1.0f || std::isnan(loc.x) || std::isinf(loc.x)) {
                uintptr_t rootComp = SafeRead<uintptr_t>(actor + Offsets::UE3::RootComponent);
                if (IsValidPointer(rootComp)) {
                    loc = SafeRead<Vector3>(rootComp + Offsets::UE3::ActorLocation);
                }
            }
            if (loc.Length() < 1.0f || std::isnan(loc.x) || std::isinf(loc.x)) continue;

            // If player (Miles Upshur)
            if (className.find("OLHero") != std::string::npos) {
                heroLocation = loc + Vector3(0.0f, 0.0f, 64.0f); // Eye height offset
                heroRotation = SafeRead<FRotator>(actor + Offsets::UE3::ActorRotation);
                foundHero = true;
                continue;
            }

            // Filter out non-interactive non-physical and utility actors
            if (ContainsCi(className, "Sound") || ContainsCi(className, "Audio") ||
                ContainsCi(className, "Light") || ContainsCi(className, "Emitter") ||
                ContainsCi(className, "Decal") || ContainsCi(className, "Fog") ||
                ContainsCi(className, "PathNode") || ContainsCi(className, "NavMesh") ||
                ContainsCi(className, "Brush") || ContainsCi(className, "TargetPoint") ||
                ContainsCi(className, "Route") || ContainsCi(className, "Marker") ||
                ContainsCi(className, "Info") || ContainsCi(className, "Manager") ||
                ContainsCi(className, "Controller") || ContainsCi(className, "Camera") ||
                ContainsCi(className, "Spline") || ContainsCi(className, "Navigation") ||
                ContainsCi(className, "SeqAct") || ContainsCi(className, "Sequence")) {
                continue;
            }

            // Check AActor flags: bit 1 (0x2) = bHidden, bit 3 (0x8) = bDeleteMe
            uint32_t actorFlags = SafeRead<uint32_t>(actor + Offsets::UE3::ActorFlags, 0);
            bool bHidden = (actorFlags & 0x02) != 0;
            bool bDeleteMe = (actorFlags & 0x08) != 0;

            if (bDeleteMe) continue;

            std::string actorName = GetActorName(actor);

            bool isTriggerOrVolume = (ContainsCi(className, "Trigger") || ContainsCi(className, "Volume") ||
                                      ContainsCi(actorName, "Trigger") || ContainsCi(actorName, "Volume"));

            Entity ent{};
            ent.address = actor;
            ent.className = className;
            ent.actorName = actorName;
            ent.location = loc;
            ent.rotation = SafeRead<FRotator>(actor + Offsets::UE3::ActorRotation);
            ent.health = SafeRead<float>(actor + Offsets::UE3::Health, 100.0f);
            ent.maxHealth = SafeRead<float>(actor + Offsets::UE3::MaxHealth, 100.0f);

            if (ent.maxHealth <= 0.0f || ent.maxHealth > 10000.0f) {
                ent.maxHealth = 100.0f;
            }
            if (ent.health <= 0.0f || ent.health > 10000.0f) {
                ent.health = 100.0f;
            }

            // Checks for Interactive objects (Valves, Wheels, Pumps, Generators, Breakers, Levers, Switches, Fuses)
            bool isValve = (ContainsCi(className, "valve") || ContainsCi(actorName, "valve") ||
                            ContainsCi(className, "wheel") || ContainsCi(actorName, "wheel") ||
                            ContainsCi(actorName, "crank") || ContainsCi(className, "sprinkler"));
            bool isPump = (ContainsCi(className, "pump") || ContainsCi(actorName, "pump") ||
                           ContainsCi(className, "drain") || ContainsCi(actorName, "drain"));
            bool isGen = (ContainsCi(className, "generator") || ContainsCi(actorName, "generator") ||
                          ContainsCi(className, "breaker") || ContainsCi(actorName, "breaker"));
            bool isSwitch = (ContainsCi(className, "switch") || ContainsCi(actorName, "switch") ||
                             ContainsCi(className, "lever") || ContainsCi(actorName, "lever"));
            bool isButton = (ContainsCi(className, "button") || ContainsCi(actorName, "button"));
            bool isFuse = (ContainsCi(className, "fuse") || ContainsCi(actorName, "fuse"));
            bool isElevator = (ContainsCi(className, "elevator") || ContainsCi(actorName, "elevator") ||
                               ContainsCi(className, "lift") || ContainsCi(actorName, "lift"));
            bool isInteractable = (ContainsCi(className, "interact") || ContainsCi(actorName, "interact") ||
                                   ContainsCi(className, "pushable") || ContainsCi(actorName, "pushable"));

            // Check if already used / completed (Outlast interaction state offsets: 0x248 Max, 0x29C Current)
            int32_t maxInteractions = SafeRead<int32_t>(actor + 0x248, 0);
            int32_t curInteractions = SafeRead<int32_t>(actor + 0x29C, 0);
            bool isCompleted = (maxInteractions > 0 && curInteractions >= maxInteractions);

            // Checks for Hiding Spots
            bool isLocker = (ContainsCi(className, "locker") || ContainsCi(actorName, "locker"));
            bool isBed = (ContainsCi(className, "bed") || ContainsCi(actorName, "bed"));
            bool isHide = (ContainsCi(className, "hidespot") || ContainsCi(actorName, "hidespot") ||
                           ContainsCi(className, "hiding") || ContainsCi(actorName, "hiding"));

            // Classification
            if (ContainsCi(className, "OLEnemySoldier") || ContainsCi(className, "OLEnemyCannibal") ||
                ContainsCi(className, "Walker") || ContainsCi(className, "Chris")) {
                ent.type = EntityType::Enemy;
                ent.name = "Chris Walker";
                ent.color = IM_COL32(255, 30, 30, 255);
                ent.isValid = true;
            } else if (ContainsCi(className, "OLEnemySurgeon")) {
                ent.type = EntityType::Enemy;
                ent.name = "Dr. Trager";
                ent.color = IM_COL32(255, 120, 0, 255);
                ent.isValid = true;
            } else if (ContainsCi(className, "OLEnemyGroom")) {
                ent.type = EntityType::Enemy;
                ent.name = "Eddie Gluskin (Groom)";
                ent.color = IM_COL32(255, 80, 180, 255);
                ent.isValid = true;
            } else if (ContainsCi(className, "OLEnemyPriest")) {
                ent.type = EntityType::Enemy;
                ent.name = "Father Martin";
                ent.color = IM_COL32(200, 200, 50, 255);
                ent.isValid = true;
            } else if (ContainsCi(className, "OLEnemyNanoCloud")) {
                ent.type = EntityType::Enemy;
                ent.name = "Walrider (Nano)";
                ent.color = IM_COL32(180, 50, 255, 255);
                ent.isValid = true;
            } else if (ContainsCi(className, "OLEnemyGenericPatient") || ContainsCi(className, "OLEnemy") || ContainsCi(className, "OLPawn")) {
                ent.type = EntityType::Enemy;
                ent.name = "Variant / Patient";
                ent.color = IM_COL32(230, 40, 40, 255);
                ent.isValid = true;
            } else if (ContainsCi(className, "Batter") || ContainsCi(actorName, "Batter")) {
                if (!isTriggerOrVolume && (!g_Settings.filterHiddenItems || !bHidden)) {
                    ent.type = EntityType::Battery;
                    ent.name = "Battery [NV]";
                    ent.color = IM_COL32(255, 225, 0, 255);
                    ent.isValid = true;
                }
            } else if (!isTriggerOrVolume &&
                       (ContainsCi(className, "Collectible") || ContainsCi(className, "Document") ||
                        ContainsCi(actorName, "Collectible") || (ContainsCi(actorName, "Doc") && !ContainsCi(actorName, "Doctor")))) {
                // Physical Document folders in Outlast (OLCollectiblePickup)
                if (!g_Settings.filterHiddenItems || !bHidden) {
                    ent.type = EntityType::Document;
                    ent.name = "Document [Folder]";
                    ent.color = IM_COL32(0, 200, 255, 255);
                    ent.isValid = true;
                }
            } else if (ContainsCi(className, "Note") || ContainsCi(actorName, "Note")) {
                // In Outlast, notes are not physical pickups; they are events recorded with the camcorder via invisible trigger zones
                if (g_Settings.itemNoteEvents) {
                    ent.type = EntityType::NoteEvent;
                    ent.name = "[Camcorder] Note Event";
                    ent.color = IM_COL32(100, 220, 255, 220);
                    ent.isValid = true;
                }
            } else if (ContainsCi(className, "Key") || ContainsCi(actorName, "Key")) {
                if (!isTriggerOrVolume && (!g_Settings.filterHiddenItems || !bHidden)) {
                    ent.type = EntityType::KeyItem;
                    ent.name = "Key";
                    ent.color = IM_COL32(50, 255, 50, 255);
                    ent.isValid = true;
                }
            } else if (!isTriggerOrVolume &&
                       (ContainsCi(className, "Pickup") || ContainsCi(className, "Item") || ContainsCi(actorName, "Pickup"))) {
                if (!g_Settings.filterHiddenItems || !bHidden) {
                    ent.type = EntityType::KeyItem;
                    ent.name = "Item";
                    ent.color = IM_COL32(50, 200, 255, 255);
                    ent.isValid = true;
                }
            } else if (isLocker || isBed || isHide) {
                ent.type = EntityType::HideSpot;
                if (isLocker) {
                    ent.name = "[Hide] Locker";
                } else if (isBed) {
                    ent.name = "[Hide] Bed";
                } else {
                    ent.name = "[Hide] Spot";
                }
                ent.color = ToImU32(g_Settings.hideSpotColor);
                ent.isValid = true;
            } else if (isValve || isPump || isGen || isSwitch || isButton || isFuse || isElevator || isInteractable) {
                ent.type = EntityType::Interactive;
                if (isValve) {
                    ent.name = isCompleted ? "[Done] Valve" : "[Quest] Valve";
                } else if (isPump) {
                    ent.name = isCompleted ? "[Done] Pump" : "[Quest] Pump";
                } else if (isGen) {
                    ent.name = isCompleted ? "[Done] Generator" : "[Quest] Generator";
                } else if (isFuse) {
                    ent.name = isCompleted ? "[Done] Fuse Box" : "[Quest] Fuse Box";
                } else if (isSwitch) {
                    ent.name = isCompleted ? "[Used] Switch" : "[Use] Switch";
                } else if (isButton) {
                    ent.name = isCompleted ? "[Used] Button" : "[Use] Button";
                } else if (isElevator) {
                    ent.name = "[Use] Elevator";
                } else {
                    ent.name = "[Use] Object";
                }
                ent.color = isCompleted ? IM_COL32(140, 140, 140, 180) : ToImU32(g_Settings.interactiveColor);
                ent.isValid = true;
            } else if (ContainsCi(className, "Door") || ContainsCi(actorName, "Door") || ContainsCi(actorName, "Gate")) {
                if (!ContainsCi(actorName, "Snd") && !ContainsCi(actorName, "Sound")) {
                    ent.type = EntityType::Door;
                    ent.name = "Door";
                    ent.color = IM_COL32(160, 160, 160, 200);
                    ent.isValid = true;
                }
            }

            if (ent.isValid) {
                newEntities.push_back(ent);
            }
        }
    }

    // Fallback: If camera from PlayerCamera was not resolved, use player pawn's eye position and rotation!
    if (!newCam.isValid && foundHero) {
        newCam.location = heroLocation;
        newCam.rotation = heroRotation;
        newCam.fov = 90.0f;
        newCam.isValid = true;
    }

    // Calculate distance for all valid entities
    if (newCam.isValid) {
        for (auto& ent : newEntities) {
            ent.distance = ent.location.DistanceTo(newCam.location) / 100.0f;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_camera = newCam;
        m_entities = std::move(newEntities);
    }
}

void Esp::DrawBox2D(ImDrawList* drawList, const Vector2& top, const Vector2& bottom, float width, ImU32 color, bool filled) {
    float x1 = std::floor(top.x - width * 0.5f);
    float x2 = std::floor(top.x + width * 0.5f);
    float y1 = std::floor(top.y);
    float y2 = std::floor(bottom.y);

    if (filled) {
        ImU32 fillColor = (color & 0x00FFFFFF) | 0x30000000;
        drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), fillColor);
    }

    drawList->AddRect(ImVec2(x1 - 1.0f, y1 - 1.0f), ImVec2(x2 + 1.0f, y2 + 1.0f), IM_COL32(0, 0, 0, 220), 0.0f, 0, 1.0f);
    drawList->AddRect(ImVec2(x1 + 1.0f, y1 + 1.0f), ImVec2(x2 - 1.0f, y2 - 1.0f), IM_COL32(0, 0, 0, 220), 0.0f, 0, 1.0f);
    drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), color, 0.0f, 0, 1.5f);
}

void Esp::DrawCornerBox(ImDrawList* drawList, const Vector2& top, const Vector2& bottom, float width, ImU32 color) {
    float x1 = std::floor(top.x - width * 0.5f);
    float x2 = std::floor(top.x + width * 0.5f);
    float y1 = std::floor(top.y);
    float y2 = std::floor(bottom.y);

    float lineW = std::floor((x2 - x1) * 0.25f);
    float lineH = std::floor((y2 - y1) * 0.25f);

    auto DrawCornerLines = [&](ImU32 col, float thickness, float offset) {
        drawList->AddLine(ImVec2(x1 - offset, y1 - offset), ImVec2(x1 + lineW, y1 - offset), col, thickness);
        drawList->AddLine(ImVec2(x1 - offset, y1 - offset), ImVec2(x1 - offset, y1 + lineH), col, thickness);

        drawList->AddLine(ImVec2(x2 + offset, y1 - offset), ImVec2(x2 - lineW, y1 - offset), col, thickness);
        drawList->AddLine(ImVec2(x2 + offset, y1 - offset), ImVec2(x2 + offset, y1 + lineH), col, thickness);

        drawList->AddLine(ImVec2(x1 - offset, y2 + offset), ImVec2(x1 + lineW, y2 + offset), col, thickness);
        drawList->AddLine(ImVec2(x1 - offset, y2 + offset), ImVec2(x1 - offset, y2 - lineH), col, thickness);

        drawList->AddLine(ImVec2(x2 + offset, y2 + offset), ImVec2(x2 - lineW, y2 + offset), col, thickness);
        drawList->AddLine(ImVec2(x2 + offset, y2 + offset), ImVec2(x2 + offset, y2 - lineH), col, thickness);
    };

    DrawCornerLines(IM_COL32(0, 0, 0, 220), 3.0f, 0.0f);
    DrawCornerLines(color, 1.5f, 0.0f);
}

void Esp::DrawSnapline(ImDrawList* drawList, const Vector2& screenPos, ImU32 color) {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 origin = (g_Settings.snaplineOrigin == 0)
        ? ImVec2(std::floor(displaySize.x * 0.5f), displaySize.y)
        : ImVec2(std::floor(displaySize.x * 0.5f), std::floor(displaySize.y * 0.5f));

    drawList->AddLine(origin, ImVec2(std::floor(screenPos.x), std::floor(screenPos.y)), color, 1.2f);
}

void Esp::DrawHealthBar(ImDrawList* drawList, const Vector2& top, const Vector2& bottom, float width, float health, float maxHealth) {
    float boxHeight = bottom.y - top.y;
    float x = std::floor(top.x - width * 0.5f - 6.0f);

    float pct = std::clamp(health / maxHealth, 0.0f, 1.0f);
    float barHeight = std::floor(boxHeight * pct);

    drawList->AddRectFilled(ImVec2(x - 2, std::floor(top.y) - 1), ImVec2(x + 2, std::floor(bottom.y) + 1), IM_COL32(0, 0, 0, 180));

    ImU32 hpColor = ImColor(1.0f - pct, pct, 0.2f, 1.0f);
    drawList->AddRectFilled(ImVec2(x - 1, std::floor(bottom.y) - barHeight), ImVec2(x + 1, std::floor(bottom.y)), hpColor);
}

void Esp::DrawItemESP(ImDrawList* drawList, const Entity& ent, const Vector2& screenPos) {
    float sx = std::floor(screenPos.x);
    float sy = std::floor(screenPos.y);
    float r = 4.0f;
    drawList->AddCircleFilled(ImVec2(sx, sy), r, ent.color);
    drawList->AddCircle(ImVec2(sx, sy), r + 1.0f, IM_COL32(0, 0, 0, 220), 12, 1.0f);

    char text[128];
    if (g_Settings.itemDistance) {
        snprintf(text, sizeof(text), "%s [%.0fm]", ent.name.c_str(), ent.distance);
    } else {
        snprintf(text, sizeof(text), "%s", ent.name.c_str());
    }

    ImVec2 sz = ImGui::CalcTextSize(text);
    float tx = std::floor(sx - sz.x * 0.5f);
    float ty = std::floor(sy - sz.y - 6.0f);

    drawList->AddText(ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, 255), text);
    drawList->AddText(ImVec2(tx, ty), ent.color, text);

    if (g_Settings.showDebugEntityInfo && (!ent.className.empty() || !ent.actorName.empty())) {
        char dbgText[256];
        snprintf(dbgText, sizeof(dbgText), "%s | %s", ent.className.c_str(), ent.actorName.c_str());
        ImVec2 dbgSz = ImGui::CalcTextSize(dbgText);
        float dtx = std::floor(sx - dbgSz.x * 0.5f);
        float dty = std::floor(sy + 6.0f);
        drawList->AddText(ImVec2(dtx + 1, dty + 1), IM_COL32(0, 0, 0, 255), dbgText);
        drawList->AddText(ImVec2(dtx, dty), IM_COL32(200, 200, 200, 220), dbgText);
    }
}

void Esp::Render() {
    if (!g_Settings.espEnabled) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float screenW = displaySize.x;
    float screenH = displaySize.y;

    if (screenW <= 0.0f || screenH <= 0.0f) return;

    if (g_Settings.drawCrosshair) {
        float cx = std::floor(screenW * 0.5f);
        float cy = std::floor(screenH * 0.5f);
        float sz = g_Settings.crosshairSize;
        drawList->AddLine(ImVec2(cx - sz, cy), ImVec2(cx + sz + 1.0f, cy), IM_COL32(0, 255, 0, 220), 1.5f);
        drawList->AddLine(ImVec2(cx, cy - sz), ImVec2(cx, cy + sz + 1.0f), IM_COL32(0, 255, 0, 220), 1.5f);
    }

    // Always fetch FRESH camera data synchronously on the render frame!
    CameraData cam = ReadCamera();

    // Fallback to background camera if synchronous read failed
    if (!cam.isValid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        cam = m_camera;
    }

    if (!cam.isValid) return;

    std::vector<Entity> entitiesCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        entitiesCopy = m_entities;
    }

    for (const auto& ent : entitiesCopy) {
        if (!ent.isValid) continue;

        // Items (Batteries, Documents, Note triggers, Keys)
        if (ent.type == EntityType::Battery || ent.type == EntityType::Document ||
            ent.type == EntityType::NoteEvent || ent.type == EntityType::KeyItem) {
            if (!g_Settings.itemEsp) continue;
            float dist = ent.location.DistanceTo(cam.location) / 100.0f;
            if (dist > g_Settings.itemMaxDistance) continue;
            if (ent.type == EntityType::Battery && !g_Settings.itemBatteries) continue;
            if (ent.type == EntityType::Document && !g_Settings.itemDocuments) continue;
            if (ent.type == EntityType::NoteEvent && !g_Settings.itemNoteEvents) continue;
            if (ent.type == EntityType::KeyItem && !g_Settings.itemKeys) continue;

            Vector2 itemScreen;
            if (Math::WorldToScreen(ent.location, cam.location, cam.rotation, cam.fov, screenW, screenH, itemScreen)) {
                Entity drawEnt = ent;
                drawEnt.distance = dist;
                if (ent.type == EntityType::Battery) drawEnt.color = ToImU32(g_Settings.batteryColor);
                else if (ent.type == EntityType::NoteEvent) drawEnt.color = ent.color;
                else drawEnt.color = ToImU32(g_Settings.itemColor);
                DrawItemESP(drawList, drawEnt, itemScreen);
            }
            continue;
        }

        // Hiding spots (Lockers, Beds)
        if (ent.type == EntityType::HideSpot) {
            if (!g_Settings.hideSpotsEsp) continue;
            float dist = ent.location.DistanceTo(cam.location) / 100.0f;
            if (dist > g_Settings.hideSpotMaxDistance) continue;

            Vector2 itemScreen;
            if (Math::WorldToScreen(ent.location, cam.location, cam.rotation, cam.fov, screenW, screenH, itemScreen)) {
                Entity drawEnt = ent;
                drawEnt.distance = dist;
                drawEnt.color = ToImU32(g_Settings.hideSpotColor);
                DrawItemESP(drawList, drawEnt, itemScreen);
            }
            continue;
        }

        // Interactive objects (Switches, Buttons, Valves, Elevators, Fuse Boxes)
        if (ent.type == EntityType::Interactive) {
            if (!g_Settings.interactiveEsp) continue;
            float dist = ent.location.DistanceTo(cam.location) / 100.0f;
            if (dist > g_Settings.interactiveMaxDistance) continue;

            Vector2 itemScreen;
            if (Math::WorldToScreen(ent.location, cam.location, cam.rotation, cam.fov, screenW, screenH, itemScreen)) {
                Entity drawEnt = ent;
                drawEnt.distance = dist;
                drawEnt.color = ent.color;
                DrawItemESP(drawList, drawEnt, itemScreen);
            }
            continue;
        }

        // Doors
        if (ent.type == EntityType::Door) {
            if (!g_Settings.doorEsp) continue;
            float dist = ent.location.DistanceTo(cam.location) / 100.0f;
            if (dist > g_Settings.doorMaxDistance) continue;

            Vector2 itemScreen;
            if (Math::WorldToScreen(ent.location, cam.location, cam.rotation, cam.fov, screenW, screenH, itemScreen)) {
                Entity drawEnt = ent;
                drawEnt.distance = dist;
                DrawItemESP(drawList, drawEnt, itemScreen);
            }
            continue;
        }

        // Enemy rendering
        if (ent.type == EntityType::Enemy) {
            // Live position read for enemy directly on render frame
            Vector3 entLoc = SafeRead<Vector3>(ent.address + Offsets::UE3::ActorLocation, ent.location);
            if (entLoc.Length() < 1.0f || std::isnan(entLoc.x) || std::isinf(entLoc.x)) {
                entLoc = ent.location;
            }

            float dist = entLoc.DistanceTo(cam.location) / 100.0f;
            if (dist > g_Settings.enemyMaxDistance) continue;

            Vector3 headWorld = entLoc + Vector3(0.0f, 0.0f, 85.0f);
            Vector3 feetWorld = entLoc - Vector3(0.0f, 0.0f, 85.0f);

            Vector2 headScreen, feetScreen;
            if (!Math::WorldToScreen(headWorld, cam.location, cam.rotation, cam.fov, screenW, screenH, headScreen))
                continue;
            if (!Math::WorldToScreen(feetWorld, cam.location, cam.rotation, cam.fov, screenW, screenH, feetScreen))
                continue;

            if (headScreen.x < -300.0f || headScreen.x > screenW + 300.0f ||
                headScreen.y < -300.0f || headScreen.y > screenH + 300.0f)
                continue;

            float boxHeight = std::abs(feetScreen.y - headScreen.y);
            if (boxHeight < 4.0f || boxHeight > screenH * 1.8f)
                continue;

            float boxWidth = boxHeight * 0.55f;
            if (boxWidth < 2.0f || boxWidth > screenW * 1.8f)
                continue;

            ImU32 enemyCol = ent.color;
            ImU32 snapCol  = ToImU32(g_Settings.snaplineColor);

            if (g_Settings.enemySnaplines) {
                DrawSnapline(drawList, feetScreen, snapCol);
            }

            if (g_Settings.enemyBox) {
                if (g_Settings.enemyBoxType == 0) {
                    DrawBox2D(drawList, headScreen, feetScreen, boxWidth, enemyCol, g_Settings.enemyBoxFilled);
                } else {
                    DrawCornerBox(drawList, headScreen, feetScreen, boxWidth, enemyCol);
                }
            }

            if (g_Settings.enemyHealth) {
                float curHp = SafeRead<float>(ent.address + Offsets::UE3::Health, ent.health);
                DrawHealthBar(drawList, headScreen, feetScreen, boxWidth, curHp, ent.maxHealth);
            }

            char infoText[128];
            if (g_Settings.enemyName && g_Settings.enemyDistance) {
                snprintf(infoText, sizeof(infoText), "%s [%.0fm]", ent.name.c_str(), dist);
            } else if (g_Settings.enemyName) {
                snprintf(infoText, sizeof(infoText), "%s", ent.name.c_str());
            } else if (g_Settings.enemyDistance) {
                snprintf(infoText, sizeof(infoText), "[%.0fm]", dist);
            } else {
                infoText[0] = '\0';
            }

            if (infoText[0] != '\0') {
                ImVec2 textSize = ImGui::CalcTextSize(infoText);
                float textX = std::floor(headScreen.x - textSize.x * 0.5f);
                float textY = std::floor(headScreen.y - textSize.y - 4.0f);

                drawList->AddText(ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 255), infoText);
                drawList->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), infoText);
            }
        }
    }
}

void Esp::RenderMenu() {
    if (!g_Settings.menuOpen) return;

    ImGui::SetNextWindowSize(ImVec2(540, 580), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Outlast 1 — Internal Menu [INSERT to hide]", &g_Settings.menuOpen)) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Outlast 1 (Unreal Engine 3.5 — DirectX 9/11)");
        ImGui::Separator();

        if (ImGui::BeginTabBar("Tabs")) {
            if (ImGui::BeginTabItem("Cheats")) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[ Native Engine Exploits ]");
                ImGui::Separator();

                ImGui::Checkbox("God Mode (Invincibility)", &g_Settings.godMode);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Hooks native AOLHero::TakeDamage (0xD8A140), TryKillHero (0xD69870), and TryDecapitate (0xD697B0).\nNegates all damage, hits, fall damage, and execution grabs.");
                }

                ImGui::Spacing();
                ImGui::TextDisabled("Status: %s", g_Settings.godMode ? "IMMUNE TO DAMAGE" : "NORMAL");

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Enemy ESP")) {
                ImGui::Checkbox("Enable Master ESP", &g_Settings.espEnabled);
                ImGui::Separator();

                ImGui::Checkbox("Draw Box", &g_Settings.enemyBox);
                if (g_Settings.enemyBox) {
                    ImGui::Combo("Box Type", &g_Settings.enemyBoxType, "2D Box\0Corner Box\0");
                    ImGui::Checkbox("Fill Box", &g_Settings.enemyBoxFilled);
                }

                ImGui::Checkbox("Snaplines", &g_Settings.enemySnaplines);
                if (g_Settings.enemySnaplines) {
                    ImGui::Combo("Snapline Origin", &g_Settings.snaplineOrigin, "Bottom of Screen\0Center of Screen\0");
                }

                ImGui::Checkbox("Draw Health Bar", &g_Settings.enemyHealth);
                ImGui::Checkbox("Draw Name (Chris Walker, Trager, etc.)", &g_Settings.enemyName);
                ImGui::Checkbox("Draw Distance", &g_Settings.enemyDistance);

                ImGui::SliderFloat("Enemy Max Distance (m)", &g_Settings.enemyMaxDistance, 10.0f, 300.0f, "%.0f m");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Items & World")) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Pickups & Objectives ]");
                ImGui::Checkbox("Item ESP", &g_Settings.itemEsp);
                ImGui::Checkbox("Batteries (Night Vision)", &g_Settings.itemBatteries);
                ImGui::Checkbox("Physical Documents (Folders)", &g_Settings.itemDocuments);
                ImGui::Checkbox("Camcorder Note Events (Trigger Zones)", &g_Settings.itemNoteEvents);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Invisible floor triggers where Miles records notes using the camcorder.\nDisabled by default so they don't point at empty ground.");
                }
                ImGui::Checkbox("Keys & Quest Items", &g_Settings.itemKeys);
                ImGui::Checkbox("Filter Hidden / Collected Items", &g_Settings.filterHiddenItems);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Hides items that have already been picked up or marked hidden by the game engine.");
                }
                ImGui::Checkbox("Show Distance", &g_Settings.itemDistance);
                ImGui::SliderFloat("Item Max Distance (m)", &g_Settings.itemMaxDistance, 5.0f, 150.0f, "%.0f m");

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "[ World & Navigation ]");
                ImGui::Checkbox("Hiding Spots (Lockers & Beds)", &g_Settings.hideSpotsEsp);
                if (g_Settings.hideSpotsEsp) {
                    ImGui::SliderFloat("Hide Spot Max Dist (m)", &g_Settings.hideSpotMaxDistance, 5.0f, 100.0f, "%.0f m");
                }

                ImGui::Checkbox("Interactive (Valves, Pumps, Generators, Switches, Fuse Boxes)", &g_Settings.interactiveEsp);
                if (g_Settings.interactiveEsp) {
                    ImGui::SliderFloat("Interactive Max Dist (m)", &g_Settings.interactiveMaxDistance, 5.0f, 150.0f, "%.0f m");
                }

                ImGui::Checkbox("Doors", &g_Settings.doorEsp);
                if (g_Settings.doorEsp) {
                    ImGui::SliderFloat("Door Max Dist (m)", &g_Settings.doorMaxDistance, 5.0f, 80.0f, "%.0f m");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Visuals & Colors")) {
                ImGui::Checkbox("Crosshair", &g_Settings.drawCrosshair);
                if (g_Settings.drawCrosshair) {
                    ImGui::SliderFloat("Crosshair Size", &g_Settings.crosshairSize, 2.0f, 20.0f);
                }

                ImGui::Separator();
                ImGui::ColorEdit4("Enemy Color", (float*)&g_Settings.enemyColor);
                ImGui::ColorEdit4("Battery Color", (float*)&g_Settings.batteryColor);
                ImGui::ColorEdit4("Item Color", (float*)&g_Settings.itemColor);
                ImGui::ColorEdit4("Hiding Spot Color", (float*)&g_Settings.hideSpotColor);
                ImGui::ColorEdit4("Interactive Color", (float*)&g_Settings.interactiveColor);
                ImGui::ColorEdit4("Snapline Color", (float*)&g_Settings.snaplineColor);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Diagnostics")) {
                uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
                ImGui::Text("Status: INJECTED & RUNNING");
                ImGui::Text("Module Base: 0x%llX", static_cast<unsigned long long>(moduleBase));
                ImGui::Text("GWorld: 0x%llX", static_cast<unsigned long long>(m_gWorldAddress));
                ImGui::Text("GEngine: 0x%llX", static_cast<unsigned long long>(m_gEngineAddress));
                ImGui::Text("GNames: 0x%llX", static_cast<unsigned long long>(m_gNamesAddress));

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    ImGui::Text("Entities Detected: %zu", m_entities.size());
                    ImGui::Text("Camera Status: %s", m_camera.isValid ? "TRACKED" : "WAITING FOR MAP");
                    if (m_camera.isValid) {
                        ImGui::Text("Cam Pos: X:%.1f Y:%.1f Z:%.1f", m_camera.location.x, m_camera.location.y, m_camera.location.z);
                        ImGui::Text("FOV: %.1f", m_camera.fov);
                    }
                }

                ImGui::Separator();
                ImGui::Checkbox("Show Debug Info on ESP (Class | Actor Name)", &g_Settings.showDebugEntityInfo);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Renders the internal UE3 Class and Actor Name under ESP markers to easily identify any object.");
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "[ Nearby Entities (within 30m) ]");
                if (ImGui::BeginTable("NearbyTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 180))) {
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                    ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actor Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    std::vector<Entity> ents;
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        ents = m_entities;
                    }
                    for (const auto& e : ents) {
                        if (e.distance > 30.0f) continue;
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        const char* typeStr = "Unknown";
                        switch (e.type) {
                            case EntityType::Enemy: typeStr = "Enemy"; break;
                            case EntityType::Battery: typeStr = "Battery"; break;
                            case EntityType::Document: typeStr = "Doc"; break;
                            case EntityType::NoteEvent: typeStr = "NoteEvent"; break;
                            case EntityType::KeyItem: typeStr = "Key"; break;
                            case EntityType::HideSpot: typeStr = "Hide"; break;
                            case EntityType::Interactive: typeStr = "Object"; break;
                            case EntityType::Door: typeStr = "Door"; break;
                            default: break;
                        }
                        ImGui::TextUnformatted(typeStr);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.1fm", e.distance);

                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(e.className.c_str());

                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(e.actorName.c_str());
                    }
                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
