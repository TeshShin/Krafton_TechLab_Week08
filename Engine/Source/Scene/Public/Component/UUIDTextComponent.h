#pragma once
#include "Scene/Public/Component/TextComponent.h"

class AActor;

UCLASS()
class UUUIDTextComponent : public UTextComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UUUIDTextComponent, UTextComponent)
public:
	UUUIDTextComponent();
	~UUUIDTextComponent() override;

	virtual void OnSelected() override;
	virtual void OnDeselected() override;

	void UpdateRotationMatrix(const FVector& InCameraForward) override;
	void SetOffset(float Offset) { ZOffset = Offset; }

	FMatrix GetRTMatrix() const override { return RTMatrix; }
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	UClass* GetSpecificWidgetClass() const override;
private:
	FMatrix RTMatrix;
	float ZOffset;
};