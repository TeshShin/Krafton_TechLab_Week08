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
     * @brief Returns unified light data for ambient light
     * @note Ambient light is now handled through StructuredBuffer like all other lights
     */
    virtual struct FUnifiedDynamicLight GetUnifiedLightData() const override;

protected:
	class UTexture* GetLightBillboardTexture() override;
};
