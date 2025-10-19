#pragma once

#include "ILineSource.h"
#include "Physics/Public/BoundingVolume.h"
#include "LineVertex.h"

class FBoundingBoxLineSource : public ILineSource
{
public:
    FBoundingBoxLineSource();

    virtual const TArray<FLineVertex>& GetVertices() const override { return Vertices; }
    virtual const TArray<uint32>& GetIndices() const override { return Indices; }
    virtual bool IsDirty() const override { return bIsDirty; }
    virtual void ClearDirtyFlag() const override { bIsDirty = false; }

    void SetBoundingVolume(const IBoundingVolume* InBoundingVolume);

private:
    void GenerateLines();
    void GenerateAABB(const class FAABB* InAABB);
    void GenerateOBB(const class FOBB* InOBB);
    void GenerateSpotLight(const class FSpotLightOBB* InSpotLight);

    TArray<FLineVertex> Vertices;
    TArray<uint32> Indices;
    mutable bool bIsDirty = true;

    const IBoundingVolume* BoundingVolume = nullptr;
};
