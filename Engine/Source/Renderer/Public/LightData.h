#pragma once
#include "Core/Public/CoreTypes.h"

/**
 * @file LightData.h
 * @brief GPU StructuredBuffer에 사용되는 라이트 데이터 구조체 정의
 * @note Forward Rendering을 위한 라이트 정보 전달용
 */

/**
 * @brief Dynamic Light Type Enumeration
 * @note Used in FUnifiedDynamicLight for type identification in shaders
 */
enum class EDynamicLightType : uint32
{
    Directional = 0,  // Directional light (infinite distance, parallel rays)
    Point = 1,        // Point light (omnidirectional)
    Spot = 2,         // Spot light (cone-shaped)
	Ambient = 3,         // Ambient light (global illumination)
    Max = 4
};

/**
 * @brief Unified Dynamic Light Data for Forward+ Rendering
 * @note 16-byte alignment guaranteed (total 80 bytes)
 * @details Single unified structure for all dynamic light types
 *          Designed for efficient light culling and distance-based sorting
 *
 * Usage:
 * - Directional Light: Uses Direction, Intensity, Color (Position unused, infinite distance)
 * - Point Light: Uses Position, Intensity, Color, AttenuationRadius, FalloffExponent
 * - Spot Light:  Uses all fields except Param2
 *   - Param0 = InnerConeAngle
 *   - Param1 = OuterConeAngle
 * - Rect Light:  Reserved for future (Param0=Width, Param1=Height)
 */
struct FUnifiedDynamicLight
{
    FVector Position;           // 12 bytes - World space light position
    float Intensity;            // 4 bytes  - Light intensity (0.0 - 20.0)
    FVector Color;              // 12 bytes - RGB color filter (0.0 - 1.0 per channel)
    float AttenuationRadius;    // 4 bytes  - Light influence radius
    FVector Direction;          // 12 bytes - Light direction (Spot only, unused for Point)
    float FalloffExponent;      // 4 bytes  - Radial falloff exponent (2.0 - 16.0)
    float Param0;               // 4 bytes  - Spot: InnerConeAngle (radians)
    float Param1;               // 4 bytes  - Spot: OuterConeAngle (radians)
    float Param2;               // 4 bytes  - Reserved for future use
    uint32 LightType;           // 4 bytes  - EDynamicLightType enum value
	/// Shadow ///
	FMatrix LightView; // 64 bytes - Light View Projection Matrix
	FMatrix LightProjection; // 64 bytes - Light View Projection Matrix
	float ShadowBias;            // 4 bytes - Shadow Bias
	uint32 bCastShadows;         // 4 bytes - Light Does Cast Shadows
	int32 ShadowMapIndex;        // 4 bytes - Shadow Texture2D Array Index, 나중에 ShadowAtlas를 위해서.
	float Padding;        // 4 bytes - four Byte
};
static_assert(sizeof(FUnifiedDynamicLight) == 208, "FUnifiedDynamicLight must be 208 bytes for proper GPU alignment");
