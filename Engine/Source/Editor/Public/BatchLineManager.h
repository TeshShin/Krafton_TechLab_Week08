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

    void AddLine(const FVector& InStart, const FVector& InEnd, const FVector4& InColor);
    void UpdateGrid(float InCellSize);
    void UpdateBoundingBox(const class IBoundingVolume* InBoundingVolume);
    float GetGridCellSize() const;

    void Update();
    void Render();

private:
    struct FBatchLineManagerData* Data = nullptr;
};
