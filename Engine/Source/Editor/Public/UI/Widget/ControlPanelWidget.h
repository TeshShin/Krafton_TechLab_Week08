#pragma once
#include "Widget.h"

class UControlPanelWidget : public UWidget
{
	DECLARE_CLASS(UControlPanelWidget, UWidget)
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Special Member Function
	UControlPanelWidget() = default;
	~UControlPanelWidget() override = default;
};
