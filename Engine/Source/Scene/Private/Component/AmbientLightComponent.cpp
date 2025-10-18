#include "pch.h"
#include "Scene/Public/Component/AmbientLightComponent.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

FUnifiedDynamicLight UAmbientLightComponent::GetUnifiedLightData() const
{
    // [UNIFIED FORWARD RENDERING] Ambient light now uses StructuredBuffer like other lights
    FUnifiedDynamicLight UnifiedLight = {};

    UnifiedLight.Position = FVector(0.0f, 0.0f, 0.0f);      // Not used for ambient
    UnifiedLight.Intensity = GetIntensity();
    UnifiedLight.Color = GetLightColor();
    UnifiedLight.SourceRadius = 0.0f;                        // Not used for ambient
    UnifiedLight.Direction = FVector(0.0f, 0.0f, 0.0f);     // Not used for ambient
    UnifiedLight.FalloffExponent = 0.0f;                     // Not used for ambient
    UnifiedLight.Param0 = 0.0f;
    UnifiedLight.Param1 = 0.0f;
    UnifiedLight.Param2 = 0.0f;
    UnifiedLight.LightType = static_cast<uint32>(EDynamicLightType::Ambient);

    // Explicitly zero-initialize padding for Release mode stability
    UnifiedLight.Padding[0] = 0.0f;
    UnifiedLight.Padding[1] = 0.0f;
    UnifiedLight.Padding[2] = 0.0f;
    UnifiedLight.Padding[3] = 0.0f;

    return UnifiedLight;
}
