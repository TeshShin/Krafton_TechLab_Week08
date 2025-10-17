#pragma once
#include "Core/Public/CoreTypes.h"

/**
 * @file LightData.h
 * @brief GPU StructuredBuffer에 사용되는 라이트 데이터 구조체 정의
 * @note Forward Rendering을 위한 라이트 정보 전달용
 */

/**
 * @brief GPU StructuredBuffer용 PointLight 데이터
 * @note 16-byte alignment 보장 (총 48 bytes)
 * @details PointLightComponent의 데이터를 GPU로 전달하기 위한 구조체
 */
struct FPointLightData
{
    FVector LightLocation;      // 12 bytes - World space light position
    float LightIntensity;       // 4 bytes  - Light intensity (0.0 - 20.0)
    FVector LightColor;         // 12 bytes - RGB color filter (0.0 - 1.0 per channel)
    float SourceRadius;         // 4 bytes  - Physical radius of light source (for specular highlights)
    float LightFalloffExtent;   // 4 bytes  - Falloff exponent (2.0 - 16.0, controls radial attenuation)
    float Padding[3];           // 12 bytes - Padding for 16-byte alignment
};
static_assert(sizeof(FPointLightData) == 48, "FPointLightData must be 48 bytes for proper GPU alignment");

/**
 * @brief GPU StructuredBuffer용 SpotLight 데이터
 * @note 16-byte alignment 보장 (총 64 bytes)
 * @details SpotLightComponent의 데이터를 GPU로 전달하기 위한 구조체
 */
struct FSpotLightData
{
    FVector LightLocation;      // 12 bytes - World space light position
    float LightIntensity;       // 4 bytes  - Light intensity (0.0 - 20.0)
    FVector LightColor;         // 12 bytes - RGB color filter (0.0 - 1.0 per channel)
    float SourceRadius;         // 4 bytes  - Physical radius of light source
    FVector LightDirection;     // 12 bytes - World space normalized direction vector
    float LightFalloffExtent;   // 4 bytes  - Falloff exponent (2.0 - 16.0)
    float InnerConeAngle;       // 4 bytes  - Inner cone angle in radians (full brightness)
    float OuterConeAngle;       // 4 bytes  - Outer cone angle in radians (falloff to zero)
    float Padding[2];           // 8 bytes  - Padding for 16-byte alignment
};
static_assert(sizeof(FSpotLightData) == 64, "FSpotLightData must be 64 bytes for proper GPU alignment");
