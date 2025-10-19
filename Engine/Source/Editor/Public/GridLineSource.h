#pragma once

#include "Editor/Public/ILineSource.h"

class GridLineSource : public ILineSource
{
public:
    GridLineSource(float InCellSize = 1.0f, int InNumLines = 250);

    virtual const TArray<FVector>& GetVertices() const override { return m_Vertices; }
    virtual const TArray<uint32>& GetIndices() const override { return m_Indices; }
    virtual bool IsDirty() const override { return m_bIsDirty; }
    virtual void ClearDirtyFlag() override { m_bIsDirty = false; }

    void SetCellSize(float InCellSize);
    float GetCellSize() const { return m_CellSize; }

private:
    void GenerateGrid();

    TArray<FVector> m_Vertices;
    TArray<uint32> m_Indices;
    bool m_bIsDirty = true;

    float m_CellSize;
    int m_NumLines;
};
