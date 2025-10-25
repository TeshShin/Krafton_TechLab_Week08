#pragma once

#include "SceneComponent.h"

UENUM()
enum class ELightComponentType
{
    LightType_Ambient     = 0,
    LightType_Directional = 1,
    LightType_Point       = 2,
    LightType_Spot        = 3,
    LightType_Max         = 4,
};
DECLARE_ENUM_REFLECTION(ELightComponentType)

UCLASS()
class ULightComponentBase : public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(ULightComponentBase, USceneComponent)

public:
    ULightComponentBase() = default;

    ~ULightComponentBase() override;

    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;

    virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

    virtual UClass* GetSpecificWidgetClass() const override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override;

    virtual void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }

	virtual void EndPlay() override { Super::EndPlay(); }

	virtual void OnSelected() override;

	virtual void OnDeselected() override;

	/*-----------------------------------------------------------------------------
		USceneComponent Features
	 -----------------------------------------------------------------------------*/
protected:
	virtual void MarkAsDirty() override;

    /*-----------------------------------------------------------------------------
        ULightComponentBase Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    float GetIntensity() const { return Intensity; }

    FVector GetLightColor() const { return LightColor;}

    virtual ELightComponentType GetLightType() const { return ELightComponentType::LightType_Max; }

    /**
     * @brief Create unified light data for GPU StructuredBuffer
     * @return FUnifiedDynamicLight data for this light component
     * @note Each derived class implements this to provide its specific light data
     */
    virtual struct FUnifiedDynamicLight GetUnifiedLightData() const = 0;

    // --- [UE Style] ---

    // virtual FBox GetBoundingBox() const;

    // virtual FSphere GetBoundingSphere() const;

    /** @note Sets the light intensity and clamps it to the same range as Unreal Engine (0.0 - 20.0). */
    void SetIntensity(float InIntensity) { Intensity = std::clamp(InIntensity, 0.0f, 20.0f); }

    void SetLightColor(const FVector& InLightColor);

private:
    /** Total energy that the light emits. */
    float Intensity = 1.0f;

    /**
     * Filter color of the light.
     * @todo Change type of this variable into FLinearColor
     */
    FVector LightColor = { 1.0f, 1.0f, 1.0f };

	/*-----------------------------------------------------------------------------
		Visualization Features
	 -----------------------------------------------------------------------------*/
public:
	virtual void DrawDebugArrow(TArray<FName>& InOutLabels) {}

protected:
	virtual void DrawDebugLines() {}
	virtual void ClearDebugLines();
	virtual class UTexture* GetLightBillboardTexture() = 0;

	TArray<FName> DebugLineLabels;

private:
	void CreateIconChild();

	class UBillBoardComponent* IconBillboard = nullptr;

	/*-----------------------------------------------------------------------------
		Shadow Features
	 -----------------------------------------------------------------------------*/
public:
	virtual const TArray<FMatrix>& GetLightViewProjectionMatrices() const { return TArray<FMatrix>(); }

	bool DoesCastShadows() const { return bCastShadows; }
	void SetCastShadows(bool bInCastShadows) { bCastShadows = bInCastShadows; }

	int32 GetShadowMapIdx() const { return ShadowMapIdx; }
	void SetShadowMapIdx(int32 InShadowIdx) { ShadowMapIdx = InShadowIdx; }

protected:
	mutable TArray<FMatrix> CachedLightViewProjection;
	mutable bool bIsLightVPDirty = true;

	bool bCastShadows = false; // 일단 SpotLight만 true로 함
	int32 ShadowMapIdx = -1;
};
