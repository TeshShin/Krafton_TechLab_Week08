#pragma once
#include "Core/Public/Object/Object.h"

class ILineSource;

class UBatchLineManager : public UObject
{
    GENERATED_BODY()
    DECLARE_SINGLETON_CLASS(UBatchLineManager, UObject)

public:
    void Init();
    void Release();

    void AddDebugLine(const FName& InLabel, const FVector& InStart, const FVector& InEnd, const FVector4& InColor);
	void RemoveDebugLine(const FName& InLabel);
	void AddDebugCircle(const FName& BaseLabel, const FVector& Center, float Radius, const FVector4& Color, TArray<FName>& OutLabels);
	void AddDebugArrow(const FName& InLabel, const FVector& InStart, const FVector& InEnd, const FVector4& InColor, float InHeadSize, TArray<FName>& OutLabels);
	void AddDebugCone(const FName& BaseLabel, const FVector& TipLocation, const FVector& Direction, float Radius,
		float ConeAngleDegrees, const FVector4& Color, TArray<FName>& OutLabels);
    void UpdateGrid(float InCellSize);
    void UpdateBoundingBox(const class IBoundingVolume* InBoundingVolume);
    float GetGridCellSize() const;

    void Update();
    void Render();

private:
    struct FBatchLineManagerData* Data = nullptr;
};
