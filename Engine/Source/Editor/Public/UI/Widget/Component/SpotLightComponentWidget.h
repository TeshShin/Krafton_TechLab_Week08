#pragma once
#include "PointLightComponentWidget.h"

class UClass;
class USpotLightComponent;

UCLASS()
class USpotLightComponentWidget : public UPointLightComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponentWidget, UPointLightComponentWidget)

public:
    USpotLightComponentWidget() = default;

    virtual ~USpotLightComponentWidget() = default;

    /*-----------------------------------------------------------------------------
        UWidget Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

    /*-----------------------------------------------------------------------------
        USpotLightComponentWidget Features
     -----------------------------------------------------------------------------*/
private:
    USpotLightComponent* SpotLightComponent = nullptr;
};
