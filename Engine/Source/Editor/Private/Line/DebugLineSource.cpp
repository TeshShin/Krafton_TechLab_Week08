#include "pch.h"
#include "Editor/Public/Line/DebugLineSource.h"

FDebugLineSource::FDebugLineSource()
{
}

const TArray<FLineVertex>& FDebugLineSource::GetVertices() const
{
	if (IsDirty())
	{
		RebuildRenderData();
	}
	return Vertices;
}

const TArray<uint32>& FDebugLineSource::GetIndices() const
{
	if (IsDirty())
	{
		RebuildRenderData();
	}
	return Indices;
}

void FDebugLineSource::AddLine(const FName& InLabel, const FVector& InStart, const FVector& InEnd, const FVector4& InColor)
{
	LabeledLines[InLabel] = FDebugLine{ InStart, InEnd, InColor };
	bIsDirty = true;
}

void FDebugLineSource::RemoveLine(const FName& InLabel)
{
	if (LabeledLines.erase(InLabel) > 0)
	{
		bIsDirty = true;
	}
}

void FDebugLineSource::ClearLines()
{
	LabeledLines.clear();
    Vertices.clear();
    Indices.clear();
    bIsDirty = true;
}

void FDebugLineSource::RebuildRenderData() const
{
	Vertices.clear();
	Indices.clear();

	uint32 CurrentIndex = 0;
	for (const auto& Pair : LabeledLines)
	{
		const FDebugLine& Line = Pair.second;

		Vertices.emplace_back(FLineVertex{ Line.Start, Line.Color });
		Vertices.emplace_back(FLineVertex{ Line.End, Line.Color });

		Indices.emplace_back(CurrentIndex++);
		Indices.emplace_back(CurrentIndex++);
	}
	ClearDirtyFlag();
}
