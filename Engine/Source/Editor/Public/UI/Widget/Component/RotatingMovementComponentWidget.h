#pragma once
#include "Editor/Public/UI/Widget/Widget.h"

class URotatingMovementComponentWidget : public UWidget
{
    DECLARE_CLASS(URotatingMovementComponentWidget, UWidget)

public:
    void Initialize() override {}
    void Update() override {}
    void RenderWidget() override;
};
