#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    Vector2() = default;
    Vector2(float _x, float _y) : x(_x), y(_y) {}
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator+(const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3 operator-(const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
    Vector3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }

    float Dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }

    float DistanceTo(const Vector3& o) const {
        return (*this - o).Length();
    }
};

struct FRotator {
    int32_t pitch = 0; // Look up/down (Unreal units: 65536 = 360 deg)
    int32_t yaw = 0;   // Look left/right
    int32_t roll = 0;  // Tilt

    FRotator() = default;
    FRotator(int32_t p, int32_t y, int32_t r) : pitch(p), yaw(y), roll(r) {}

    float PitchDegrees() const { return static_cast<float>(pitch) * (360.0f / 65536.0f); }
    float YawDegrees()   const { return static_cast<float>(yaw)   * (360.0f / 65536.0f); }
    float RollDegrees()  const { return static_cast<float>(roll)  * (360.0f / 65536.0f); }
};

namespace Math {
    // Converts UE3 Rotator to direction vectors (Forward, Right, Up)
    // Matches Unreal Engine 3 FRotationMatrix exactly
    inline void GetAxes(const FRotator& rot, Vector3& vForward, Vector3& vRight, Vector3& vUp) {
        const float radPitch = rot.PitchDegrees() * (static_cast<float>(M_PI) / 180.0f);
        const float radYaw   = rot.YawDegrees()   * (static_cast<float>(M_PI) / 180.0f);
        const float radRoll  = rot.RollDegrees()  * (static_cast<float>(M_PI) / 180.0f);

        const float SP = std::sin(radPitch);
        const float CP = std::cos(radPitch);
        const float SY = std::sin(radYaw);
        const float CY = std::cos(radYaw);
        const float SR = std::sin(radRoll);
        const float CR = std::cos(radRoll);

        // Forward (X Axis in UE3)
        vForward.x = CP * CY;
        vForward.y = CP * SY;
        vForward.z = SP;

        // Right (Y Axis in UE3)
        vRight.x = CY * SP * SR - SY * CR;
        vRight.y = SY * SP * SR + CY * CR;
        vRight.z = -SR * CP;

        // Up (Z Axis in UE3)
        vUp.x = -(CR * SP * CY + SR * SY);
        vUp.y = CY * SR - CR * SP * SY;
        vUp.z = CR * CP;
    }

    // World to Screen projection for Unreal Engine 3
    inline bool WorldToScreen(
        const Vector3& worldPos,
        const Vector3& cameraLoc,
        const FRotator& cameraRot,
        float fov,
        float screenWidth,
        float screenHeight,
        Vector2& screenPos
    ) {
        Vector3 vForward, vRight, vUp;
        GetAxes(cameraRot, vForward, vRight, vUp);

        Vector3 delta = worldPos - cameraLoc;

        Vector3 transformed;
        transformed.x = delta.Dot(vRight);
        transformed.y = delta.Dot(vUp);
        transformed.z = delta.Dot(vForward);

        // Object is behind or too close to the camera near plane
        if (transformed.z <= 1.0f) {
            return false;
        }

        const float halfWidth  = screenWidth * 0.5f;
        const float halfHeight = screenHeight * 0.5f;
        const float fovRad     = fov * (static_cast<float>(M_PI) / 360.0f);
        const float tanFov     = std::tan(fovRad);

        if (tanFov == 0.0f) return false;

        screenPos.x = halfWidth + (transformed.x / (transformed.z * tanFov)) * halfWidth;
        screenPos.y = halfHeight - (transformed.y / (transformed.z * tanFov)) * halfWidth;

        return true;
    }
}
