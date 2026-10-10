#pragma once
#include <cstdint>

namespace Offsets {
    // Process and Window info
    inline const wchar_t* ProcessName64 = L"OLGame.exe";
    inline const wchar_t* WindowTitle   = L"Outlast";

    // Unreal Engine 3.5 (Outlast 1 Win64 GOG) Verified Offsets
    namespace UE3 {
        // Global Pointers
        // Verified RVAs in OLGame.exe
        inline uintptr_t GWorld  = 0x200B5A8; // UWorld* global pointer
        inline uintptr_t GEngine = 0x2006F00; // UEngine* global pointer
        inline uintptr_t GNames  = 0x1FF8FA0; // FNameTable / GNames (Data at 0x1FF8FA0, Num at 0x1FF8FA8)

        // UWorld Offsets
        inline uint32_t PersistentLevel    = 0x80; // UWorld -> PersistentLevel (ULevel*)
        inline uint32_t LevelsArray        = 0x70; // UWorld -> TArray<ULevel*> Data
        inline uint32_t LevelsCount        = 0x78; // UWorld -> TArray<ULevel*> Num

        // GEngine Offsets
        inline uint32_t EngineGamePlayers  = 0x6E4;// GEngine -> GamePlayers.Data (ULocalPlayer**)
        inline uint32_t EnginePlayersCount = 0x6EC;// GEngine -> GamePlayers.Num (int32)
        inline uint32_t WorldGamePlayers   = 0x80; // GWorld fallback -> GamePlayers (ULocalPlayer**)

        // ULevel Offsets
        inline uint32_t ActorsArray        = 0x60; // ULevel -> TArray<AActor*> Data
        inline uint32_t ActorsCount        = 0x68; // ULevel -> TArray<AActor*> Num

        // UObject Offsets
        inline uint32_t ObjectName         = 0x48; // UObject -> FName (Index at +0x48)
        inline uint32_t ObjectClass        = 0x50; // UObject -> UClass*

        // ULocalPlayer & Camera Chain
        inline uint32_t LocalPlayerController = 0x68; // ULocalPlayer -> PlayerController (APlayerController*)
        inline uint32_t PlayerPawn            = 0x248;// APlayerController -> Pawn / OLHero (0x248)
        inline uint32_t PlayerCamera          = 0x434;// APlayerController -> PlayerCamera
        inline uint32_t PlayerDefaultFOV      = 0x498;// APlayerController -> DefaultFOV (float 80.0f)

        // AActor / APawn Offsets
        inline uint32_t RootComponent      = 0x170;
        inline uint32_t ActorLocation      = 0x80; // FVector Location (X: 0x80, Y: 0x84, Z: 0x88)
        inline uint32_t ActorRotation      = 0x8C; // FRotator Rotation (Pitch: 0x8C, Yaw: 0x90, Roll: 0x94)
        inline uint32_t ActorFlags         = 0xE8; // BITFIELD: bit 1 (0x2) = bHidden, bit 3 (0x8) = bDeleteMe
        inline uint32_t Health             = 0x2B4;// Float Health in OLPawn
        inline uint32_t MaxHealth          = 0x2B8;
    }

    // Pattern signatures for dynamic address resolution
    namespace Signatures {
        // GWorld / GEngine pattern in OLGame.exe:
        // 48 8B 05 ?? ?? ?? ?? 48 8B D9 48 85 C0
        inline const char* GWorldPattern = "48 8B 05 ? ? ? ? 48 8B D9 48 85 C0";
        
        // GNames pattern:
        // 48 8B 05 ?? ?? ?? ?? 48 8B 0C
        inline const char* GNamesPattern = "48 8B 05 ? ? ? ? 48 8B 0C";
    }
}
