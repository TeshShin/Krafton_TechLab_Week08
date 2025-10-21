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

public:
    // UPROPERTY 시스템 테스트 - 다양한 메타데이터 활용
    UPROPERTY(float, TestField1)  // 1. 표시 안됨 (플래그 없음)

    // 2. FLAGS 매크로 사용 - 간단한 플래그 지정
    UPROPERTY_WITHMETA(int32, TestField2, UPROPERTY_FLAGS(EPropertyFlags::VisibleAnywhere))

    // 3. FLAGS 매크로 - 여러 플래그 조합
    UPROPERTY_INIT_WITHMETA(float, TestField3, 25.0f,
    	UPROPERTY_FLAGS(EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame))

    // 4. 완전한 메타데이터 - FPropertyMetadata 직접 사용
    UPROPERTY_INIT_WITHMETA(float, TestField4, 3.5f, FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .Min = 0.0,
        .Max = 100.0,
        .Tooltip = "Test field with full metadata",
        .DisplayName = "Test Field 4"
    }))
};
