#pragma once
#include "Scene/Public/Component/LightComponent.h"

class UDirectionalLightComponent : ULightComponent
{
public:
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
    
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Directional; }
};
