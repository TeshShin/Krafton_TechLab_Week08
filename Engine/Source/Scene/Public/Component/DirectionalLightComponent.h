#pragma once
#include "Scene/Public/Component/LightComponent.h"

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

public:
    UDirectionalLightComponent();
    virtual ~UDirectionalLightComponent() = default;

    /*-----------------------------------------------------------------------------
        ULightComponentBase Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Directional; }

    virtual struct FUnifiedDynamicLight GetUnifiedLightData() const override;

	virtual const FMatrix& GetLightViewProjectionMatrix() const override;
public:
	virtual void DrawDebugArrow(TArray<FName>& InOutLabels) override;

protected:
	virtual class UTexture* GetLightBillboardTexture() override;
};
