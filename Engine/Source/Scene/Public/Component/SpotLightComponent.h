#pragma once

#include "LightComponent.h"
#include "PointLightComponent.h"
#include "Core/Public/Object/Property.h"

UCLASS()
class USpotLightComponent : public UPointLightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

public:
    USpotLightComponent();

    virtual ~USpotLightComponent() = default;

    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;

    virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override { Super::BeginPlay(); }

    virtual void EndPlay() override { Super::EndPlay(); }

    virtual UClass* GetSpecificWidgetClass() const override;

    /*-----------------------------------------------------------------------------
        ULightComponentBase Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Spot; }

    virtual struct FUnifiedDynamicLight GetUnifiedLightData() const override;

public:
	virtual void DrawDebugArrow(TArray<FName>& InOutLabels) override;

protected:
	virtual void DrawDebugLines() override;

    /*-----------------------------------------------------------------------------
        USpotLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---
	float GetInnerConeAngle() const { return InnerConeAngle; }
	float GetOuterConeAngle() const { return OuterConeAngle; }
	void SetInnerConeAngle(float InInnerConeAngle);
	void SetOuterConeAngle(float InOuterConeAngle);

protected:
	UTexture* GetLightBillboardTexture() override;

private:
    // TODO: Add SpotLight specific member variables
    float InnerConeAngle = 30.0f;  // 30 degrees
    float OuterConeAngle = 45.0f;  // 45 degrees

	/*
	UPROPERTY_INIT_WITHMETA(float, OuterConeAngle, 45.0f, FPropertyMetadata({
		.Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
		.Min = 0.0f,
		.Max = 90.0f,
		.DisplayName = "Outer Cone Angle"
	}))
	 */

	/*-----------------------------------------------------------------------------
		Shadow Features
	 -----------------------------------------------------------------------------*/
public:
    const FMatrix& GetLightViewProjectionMatrix() const override;

	bool IsUsingPSM() const { return bUsePSM; }
	void SetUsePSM(bool bInUsePSM) { bUsePSM = bInUsePSM; bIsLightVPDirty = true; }

	// 카메라 의존형 PSM Light VP 계산 (라이트 콘과 카메라 절두체를 기반으로 타이트하게 맞춤)
	FMatrix ComputePSMLightViewProjection(const class UCamera& InCamera) const;

	// PSM 조절 파라미터
	float GetPSMFovScale() const { return PSMFovScale; }
	void SetPSMFovScale(float InScale) { PSMFovScale = std::clamp(InScale, 0.5f, 1.5f); bIsLightVPDirty = true; }

	float GetPSMNearOffset() const { return PSMNearOffset; }
	void SetPSMNearOffset(float InOffset) { PSMNearOffset = InOffset; bIsLightVPDirty = true; }

	float GetPSMFarOffset() const { return PSMFarOffset; }
	void SetPSMFarOffset(float InOffset) { PSMFarOffset = InOffset; bIsLightVPDirty = true; }
private:
	bool bUsePSM = false;
	float PSMFovScale = 1.0f;   // 0.5~1.5 권장
	float PSMNearOffset = 0.0f; // 단위: world units
	float PSMFarOffset = 0.0f;  // 단위: world units
};
