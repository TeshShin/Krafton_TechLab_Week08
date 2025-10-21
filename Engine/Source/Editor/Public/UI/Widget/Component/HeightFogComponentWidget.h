#pragma once
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"

class UHeightFogComponent;

UCLASS()
class UHeightFogComponentWidget : public UComponentWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UHeightFogComponentWidget, UComponentWidget)
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	UHeightFogComponentWidget() = default;
	~UHeightFogComponentWidget() override = default;

private:
	UHeightFogComponent* FogComponent{};
};