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

UENUM()
enum class EShadowProjectionMode
{
	LVP = 0,
	PSM = 1,
	LiSPSM = 2, // Directional 예정
	CSM = 3,
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
	virtual void DrawDebugArrow(TArray<FName>& InOutLabels) { }

protected:
	virtual void DrawDebugLines() {}
	virtual void ClearDebugLines();
	virtual class UTexture* GetLightBillboardTexture() = 0;

	TArray<FName> DebugLineLabels;

private:
	void CreateIconChild();

	class UBillBoardComponent* IconBillboard = nullptr;

public:
	// ... other public methods
	virtual float GetShadowNearClip() const { return 0.1f; }
	virtual float GetShadowFarClip() const { return 1000.0f; }

	/*-----------------------------------------------------------------------------
		Shadow Features
	 -----------------------------------------------------------------------------*/
public:
	const TArray<FMatrix>& GetLightViewMatrices(const FCameraConstants& InCameraInvConstants) const;
    const FMatrix& GetLightProjectionMatrix(const FCameraConstants& InCameraInvConstants) const;
    const TArray<FMatrix>& GetLightViewProjectionMatrices(const FCameraConstants& InCameraInvConstants) const;
    // Returns per-cascade DSV projection matrices in CSM mode (size == num cascades)
    const TArray<FMatrix>& GetCSMDsvProjections(const FCameraConstants& InCameraInvConstants) const { UpdateLightMatricesInternal(InCameraInvConstants); return CachedCSMDSVProjection; }

	bool DoesCastShadows() const { return bCastShadows; }
	void SetCastShadows(bool bInCastShadows) { bCastShadows = bInCastShadows; }

	int32 GetShadowMapIdx() const { return ShadowMapIdx; }
	void SetShadowMapIdx(int32 InShadowIdx) { ShadowMapIdx = InShadowIdx; }

	float GetShadowResolutionScale() const { return ShadowResolutionScale; }
	void SetShadowResolutionScale(float InResolutionScale) { ShadowResolutionScale = clamp(InResolutionScale, 0.0f, 1.0f); }

	float GetShadowSharpen() const { return ShadowSharpen; }
	void SetShadowSharpen(float InShadowSharpen) { ShadowSharpen = clamp(InShadowSharpen, 0.0f, 1.0f); }

	float GetShadowBias() const { return ShadowBias; }
	void SetShadowBias(float InShadowBias) { ShadowBias = InShadowBias; }

	float GetShadowSlopeBias() const { return ShadowSlopeBias; }
	void SetShadowSlopeBias(float InShadowSlopeBias) { ShadowSlopeBias = InShadowSlopeBias; }

	EShadowProjectionMode GetShadowProjectionMode() const { return ShadowProjectionMode; }
	void SetShadowProjectionMode(EShadowProjectionMode InMode)
	{
		ShadowProjectionMode = InMode;
		bIsLightVPDirty = true;
	}

	uint32 GetNumOfCascade() { return NumOfCascade; }
	float GetCascadeSplitLambda() {	return CascadeSpitLambda; }

protected:
	virtual void UpdateLightMatricesInternal(const FCameraConstants& InCameraInvConstants) const {}

	mutable TArray<FMatrix> CachedLightViewProjection;
	mutable TArray<FMatrix> CachedLightViewMatrices;
	mutable TArray<FMatrix> CachedCSMDSVProjection;

	mutable FMatrix CachedLightProjectionMatrix;
	mutable bool bIsLightVPDirty = true;

	bool bCastShadows = false;
	int32 ShadowMapIdx = -1;
	float ShadowResolutionScale = 1.0f;
	float ShadowBias = 0;
	float ShadowSlopeBias = 0;
	float ShadowSharpen = 1.0f;

	EShadowProjectionMode ShadowProjectionMode = EShadowProjectionMode::LVP;

protected:
	uint32 NumOfCascade = 12;
	float CascadeSpitLambda = 0.5;
};
