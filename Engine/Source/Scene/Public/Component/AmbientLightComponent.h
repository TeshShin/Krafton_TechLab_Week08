#pragma once
#include "LightComponent.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UAmbientLightComponent, ULightComponent)

public:
    UAmbientLightComponent() = default;
    virtual ~UAmbientLightComponent() = default;

    /*-----------------------------------------------------------------------------
        ULightComponentBase Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Ambient; }

    /**
     * @brief Ambient light doesn't use unified buffer, returns dummy data
     * @note Ambient light is handled separately via ConstantBuffer
     */
    virtual struct FUnifiedDynamicLight GetUnifiedLightData() const override;
};
