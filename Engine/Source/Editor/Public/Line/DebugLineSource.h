#pragma once
#include "ILineSource.h"
#include "LineVertex.h"

class FDebugLineSource : public ILineSource
{
private:
	struct FDebugLine
	{
		FVector Start;
		FVector End;
		FVector4 Color;
	};

public:
	FDebugLineSource();

	// ILineSource Interface
	virtual const TArray<FLineVertex>& GetVertices() const override;
	virtual const TArray<uint32>& GetIndices() const override;
	virtual bool IsDirty() const override { return bIsDirty; }
	virtual void ClearDirtyFlag() const override { bIsDirty = false; }

	void AddLine(const FName& InLabel, const FVector& InStart, const FVector& InEnd, const FVector4& InColor);
	void RemoveLine(const FName& InLabel);
	void ClearLines();

private:
	// FName을 키로 라인 정보를 저장
	TMap<FName, FDebugLine> LabeledLines;

	mutable TArray<FLineVertex> Vertices;
	mutable TArray<uint32> Indices;
	mutable bool bIsDirty = true;

	void RebuildRenderData() const;
};
