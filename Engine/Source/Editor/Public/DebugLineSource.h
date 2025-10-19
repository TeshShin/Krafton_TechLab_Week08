#pragma once
#include "Editor/Public/ILineSource.h"

class DebugLineSource : public ILineSource
{
public:
    DebugLineSource();

    virtual const TArray<FVector>& GetVertices() const override { return Vertices; }
    virtual const TArray<uint32>& GetIndices() const override { return Indices; }
    virtual bool IsDirty() const override { return bIsDirty; }
    virtual void ClearDirtyFlag() override { bIsDirty = false; }

    void AddLine(const FVector& InStart, const FVector& InEnd, const FVector4& InColor);
    void ClearLines();

private:
    TArray<FVector> Vertices;
    TArray<uint32> Indices;
    TArray<FVector4> Colors;
    bool bIsDirty = true;
};
