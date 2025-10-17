#pragma once
#include "Editor/Public/UI/Widget/Widget.h"

class ULightComponentWidget: public UWidget
{
    DECLARE_CLASS(ULightComponentWidget, UWidget);
    
public:
    ULightComponentWidget() = default;
    
    virtual ~ULightComponentWidget() = default;
    
    /*-----------------------------------------------------------------------------
        UWidget Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Initialize() override;
    virtual void Update() override;
    virtual void RenderWidget() override;

};
