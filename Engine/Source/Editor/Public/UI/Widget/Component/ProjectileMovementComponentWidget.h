#pragma once
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"

class UProjectileMovementComponentWidget : public UComponentWidget
{
    DECLARE_CLASS(UProjectileMovementComponentWidget, UComponentWidget)

public:
    void Initialize() override {}
    void Update() override {}
    void RenderWidget() override;
};
