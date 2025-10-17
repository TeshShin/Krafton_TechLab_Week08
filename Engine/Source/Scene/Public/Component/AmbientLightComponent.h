#pragma once
#include "LightComponent.h"

class UAmbientLightComponent : ULightComponent
{
    DECLARE_CLASS(UAmbientLightComponent, ULightComponent)
    
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Ambient; }
};
