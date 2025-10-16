#pragma once
#include "Editor/Public/UI/Widget/Widget.h"

class UProjectileMovementComponentWidget : public UWidget
{
    DECLARE_CLASS(UProjectileMovementComponentWidget, UWidget)

public:
    void Initialize() override {}
    void Update() override {}
    void RenderWidget() override;
};
