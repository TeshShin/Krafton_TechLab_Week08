#pragma once
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"

class URotatingMovementComponentWidget : public UComponentWidget
{
    DECLARE_CLASS(URotatingMovementComponentWidget, UComponentWidget)

public:
    void Initialize() override {}
    void Update() override {}
    void RenderWidget() override;
};
