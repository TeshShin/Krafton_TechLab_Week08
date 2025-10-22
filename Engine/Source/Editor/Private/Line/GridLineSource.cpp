#include "pch.h"
#include "Editor/Public/Line/GridLineSource.h"

FGridLineSource::FGridLineSource(float InCellSize, int InNumLines)
    : CellSize(InCellSize)
    , NumLines(InNumLines)
{
    GenerateGrid();
}

void FGridLineSource::SetCellSize(float InCellSize)
{
    if (CellSize != InCellSize)
    {
        CellSize = InCellSize;
        GenerateGrid();
        bIsDirty = true;
    }
}

void FGridLineSource::GenerateGrid()
{
    Vertices.clear();
    Indices.clear();

    const float LineLength = CellSize * static_cast<float>(NumLines) / 2.f;
    const FVector4 Color(0.5f, 0.5f, 0.5f, 1.0f);

    // Z-axis lines (x=0 라인 제외)
    for (int32 LineCount = -NumLines / 2; LineCount < NumLines / 2; ++LineCount)
    {
        if (LineCount != 0)
        {
            FVector Start = { static_cast<float>(LineCount) * CellSize, -LineLength, 0.0f };
            FVector End = { static_cast<float>(LineCount) * CellSize, LineLength, 0.0f };
            Vertices.push_back({Start, Color});
            Vertices.push_back({End, Color});
        }
    }

    // X-axis lines (y=0 라인 제외)
    for (int32 LineCount = -NumLines / 2; LineCount < NumLines / 2; ++LineCount)
    {
        if (LineCount != 0)
        {
            FVector Start = { -LineLength, static_cast<float>(LineCount) * CellSize, 0.0f };
            FVector End = { LineLength, static_cast<float>(LineCount) * CellSize, 0.0f };
            Vertices.push_back({Start, Color});
            Vertices.push_back({End, Color});
        }
    }

    for (uint32 i = 0; i < Vertices.size(); ++i)
    {
        Indices.push_back(i);
    }
}
