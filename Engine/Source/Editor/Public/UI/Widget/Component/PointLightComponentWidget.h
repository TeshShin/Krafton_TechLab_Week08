#pragma once
#include "ComponentWidget.h"
#include "Editor/Public/UI/Widget/Widget.h"

class UClass;
class UPointLightComponent;

UCLASS()
class UPointLightComponentWidget : public UComponentWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UPointLightComponentWidget, UComponentWidget)

public:
    UPointLightComponentWidget() = default;
    virtual ~UPointLightComponentWidget() = default;

    /*-----------------------------------------------------------------------------
        UWidget Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

    /*-----------------------------------------------------------------------------
        UPointLightComponentWidget Features
     -----------------------------------------------------------------------------*/
protected:
    UPointLightComponent* PointLightComponent = nullptr;
};
