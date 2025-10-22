#pragma once
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"

class UClass;
class UTextComponent;

class USetTextComponentWidget : public UComponentWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(USetTextComponentWidget, UComponentWidget)
public:
	USetTextComponentWidget() = default;
	~USetTextComponentWidget() override = default;
	
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

private:
	UTextComponent* SelectedTextComponent = nullptr;
};