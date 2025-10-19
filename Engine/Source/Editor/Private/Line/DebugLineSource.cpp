#include "pch.h"
#include "Editor/Public/Line/DebugLineSource.h"

FDebugLineSource::FDebugLineSource()
{
}

void FDebugLineSource::AddLine(const FVector& InStart, const FVector& InEnd, const FVector4& InColor)
{
    Vertices.push_back(InStart);
    Vertices.push_back(InEnd);
    Indices.push_back(static_cast<uint32>(Vertices.size()) - 2);
    Indices.push_back(static_cast<uint32>(Vertices.size()) - 1);
    Colors.push_back(InColor);
    bIsDirty = true;
}

void FDebugLineSource::ClearLines()
{
    Vertices.clear();
    Indices.clear();
    Colors.clear();
    bIsDirty = true;
}
