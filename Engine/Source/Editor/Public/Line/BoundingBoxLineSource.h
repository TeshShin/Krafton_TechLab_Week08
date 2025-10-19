#pragma once

#include "ILineSource.h"
#include "Physics/Public/BoundingVolume.h"

class FBoundingBoxLineSource : public ILineSource
{
public:
    FBoundingBoxLineSource();

    virtual const TArray<FVector>& GetVertices() const override { return Vertices; }
    virtual const TArray<uint32>& GetIndices() const override { return Indices; }
    virtual bool IsDirty() const override { return bIsDirty; }
    virtual void ClearDirtyFlag() override { bIsDirty = false; }

    void SetBoundingVolume(const IBoundingVolume* InBoundingVolume);

private:
    void GenerateLines();
    void GenerateAABB(const class FAABB* InAABB);
    void GenerateOBB(const class FOBB* InOBB);
    void GenerateSpotLight(const class FSpotLightOBB* InSpotLight);

    TArray<FVector> Vertices;
    TArray<uint32> Indices;
    bool bIsDirty = true;

    const IBoundingVolume* BoundingVolume = nullptr;
};
