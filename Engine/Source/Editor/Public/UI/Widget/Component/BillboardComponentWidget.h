#pragma once
#include "Editor/Public/UI/Widget/Component/ComponentWidget.h"

class UBillBoardComponent;

UCLASS()
class UBillboardComponentWidget : public UComponentWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UBillboardComponentWidget, UComponentWidget)
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Special Member Function
	UBillboardComponentWidget() = default;
	~UBillboardComponentWidget() override = default;

private:
	void SetSprite(FString NewSprite);

	inline static uint32 WidgetNum = 0;
};