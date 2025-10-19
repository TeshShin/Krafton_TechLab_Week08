#include "pch.h"
#include "Editor/Public/GridLineSource.h"

GridLineSource::GridLineSource(float InCellSize, int InNumLines)
    : m_CellSize(InCellSize)
    , m_NumLines(InNumLines)
{
    GenerateGrid();
}

void GridLineSource::SetCellSize(float InCellSize)
{
    if (m_CellSize != InCellSize)
    {
        m_CellSize = InCellSize;
        GenerateGrid();
        m_bIsDirty = true;
    }
}

void GridLineSource::GenerateGrid()
{
    m_Vertices.clear();
    m_Indices.clear();

    const float LineLength = m_CellSize * static_cast<float>(m_NumLines) / 2.f;

    // Z-axis lines
    for (int32 LineCount = -m_NumLines / 2; LineCount < m_NumLines / 2; ++LineCount)
    {
        FVector Start = { static_cast<float>(LineCount) * m_CellSize, -LineLength, 0.0f };
        FVector End = { static_cast<float>(LineCount) * m_CellSize, LineLength, 0.0f };
        m_Vertices.push_back(Start);
        m_Vertices.push_back(End);
    }

    // X-axis lines
    for (int32 LineCount = -m_NumLines / 2; LineCount < m_NumLines / 2; ++LineCount)
    {
        FVector Start = { -LineLength, static_cast<float>(LineCount) * m_CellSize, 0.0f };
        FVector End = { LineLength, static_cast<float>(LineCount) * m_CellSize, 0.0f };
        m_Vertices.push_back(Start);
        m_Vertices.push_back(End);
    }

    for (uint32 i = 0; i < m_Vertices.size(); ++i)
    {
        m_Indices.push_back(i);
    }
}
