#include "pch.h"
#include "Scene/Public/Component/AmbientLightComponent.h"
#include "Renderer/Public/LightData.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

FUnifiedDynamicLight UAmbientLightComponent::GetUnifiedLightData() const
{
    // Ambient light doesn't use unified buffer, it's handled via ConstantBuffer
    // Return dummy data to satisfy the interface
    return FUnifiedDynamicLight();
}
