#pragma once
#include "ILineSource.h"

class FGridLineSource : public ILineSource
{
public:
    FGridLineSource(float InCellSize = 1.0f, int InNumLines = 250);

    virtual const TArray<FVector>& GetVertices() const override { return Vertices; }
    virtual const TArray<uint32>& GetIndices() const override { return Indices; }
    virtual bool IsDirty() const override { return bIsDirty; }
    virtual void ClearDirtyFlag() const override { bIsDirty = false; }

    void SetCellSize(float InCellSize);
    float GetCellSize() const { return CellSize; }

private:
    void GenerateGrid();

    TArray<FVector> Vertices;
    TArray<uint32> Indices;
    mutable bool bIsDirty = true;

    float CellSize;
    int NumLines;
};
