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
	UPROPERTY_INIT_WITHMETA(float, OuterConeAngle, 45.0f, FPropertyMetadata({
		.Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
		.Min = 0.0f,
		.Max = 90.0f,
		.DisplayName = "Outer Cone Angle"
	}))
    //float OuterConeAngle = 45.0f;  // 45 degrees

public:
    // UPROPERTY 시스템 테스트 - 자동 직렬화/복제 테스트

    // 4. 편집 가능 + 직렬화됨 + 완전한 메타데이터
    UPROPERTY_INIT_WITHMETA(int, TestField4, 42, FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .Tooltip = "Test field with full metadata",
        .DisplayName = "Test Field 4"
    }));

    // 5. 복제 시 리셋됨 (DuplicateTransient)
    UPROPERTY_INIT_WITHMETA(FString, TestField5, "asdf",
        UPROPERTY_FLAGS(EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame))

    // 6. FVector2 테스트
    UPROPERTY_INIT_WITHMETA(FVector2, TestVector2, FVector2(1.0f, 2.0f), FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .DisplayName = "Test Vector2"
    }));

    // 7. FVector 테스트
    UPROPERTY_INIT_WITHMETA(FVector, TestVector, FVector(1.0f, 2.0f, 3.0f), FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .DisplayName = "Test Vector"
    }));

    // 8. FVector4 테스트
    UPROPERTY_INIT_WITHMETA(FVector4, TestVector4, FVector4(1.0f, 2.0f, 3.0f, 4.0f), FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .DisplayName = "Test Vector4"
    }));

    // 9. FLinearColor3 (RGB) 테스트
    UPROPERTY_INIT_WITHMETA(FLinearColor3, TestColor3, FLinearColor3(1.0f, 0.5f, 0.25f), FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .DisplayName = "Test Color (RGB)"
    }));

    // 10. FLinearColor (RGBA) 테스트
    UPROPERTY_INIT_WITHMETA(FLinearColor, TestColor, FLinearColor(0.5f, 0.75f, 1.0f, 0.8f), FPropertyMetadata({
        .Flags = EPropertyFlags::EditAnywhere | EPropertyFlags::SaveGame,
        .DisplayName = "Test Color (RGBA)"
    }));
};
