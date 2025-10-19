#include "pch.h"
#include "Editor/Public/Line/BoundingBoxLineSource.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"

FBoundingBoxLineSource::FBoundingBoxLineSource()
{
}

void FBoundingBoxLineSource::SetBoundingVolume(const IBoundingVolume* InBoundingVolume)
{
    BoundingVolume = InBoundingVolume;
    GenerateLines();
    bIsDirty = true;
}

void FBoundingBoxLineSource::GenerateLines()
{
    Vertices.clear();
    Indices.clear();

    if (!BoundingVolume) { return; }

    switch (BoundingVolume->GetType())
    {
        case EBoundingVolumeType::AABB:
            GenerateAABB(static_cast<const FAABB*>(BoundingVolume));
            break;
        case EBoundingVolumeType::OBB:
            GenerateOBB(static_cast<const FOBB*>(BoundingVolume));
            break;
        case EBoundingVolumeType::SpotLight:
            GenerateSpotLight(static_cast<const FSpotLightOBB*>(BoundingVolume));
            break;
        default:
            break;
    }

    for (uint32 i = 0; i < Vertices.size(); ++i)
    {
        Indices.push_back(i);
    }
}

void FBoundingBoxLineSource::GenerateAABB(const FAABB* InAABB)
{
    if (!InAABB) { return; }

    FVector Min = InAABB->Min;
    FVector Max = InAABB->Max;

    FVector Corners[8];
    Corners[0] = FVector(Min.X, Min.Y, Min.Z);
    Corners[1] = FVector(Max.X, Min.Y, Min.Z);
    Corners[2] = FVector(Max.X, Max.Y, Min.Z);
    Corners[3] = FVector(Min.X, Max.Y, Min.Z);
    Corners[4] = FVector(Min.X, Min.Y, Max.Z);
    Corners[5] = FVector(Max.X, Min.Y, Max.Z);
    Corners[6] = FVector(Max.X, Max.Y, Max.Z);
    Corners[7] = FVector(Min.X, Max.Y, Max.Z);

    // Bottom
    Vertices.push_back(Corners[0]); Vertices.push_back(Corners[1]);
    Vertices.push_back(Corners[1]); Vertices.push_back(Corners[2]);
    Vertices.push_back(Corners[2]); Vertices.push_back(Corners[3]);
    Vertices.push_back(Corners[3]); Vertices.push_back(Corners[0]);

    // Top
    Vertices.push_back(Corners[4]); Vertices.push_back(Corners[5]);
    Vertices.push_back(Corners[5]); Vertices.push_back(Corners[6]);
    Vertices.push_back(Corners[6]); Vertices.push_back(Corners[7]);
    Vertices.push_back(Corners[7]); Vertices.push_back(Corners[4]);

    // Sides
    Vertices.push_back(Corners[0]); Vertices.push_back(Corners[4]);
    Vertices.push_back(Corners[1]); Vertices.push_back(Corners[5]);
    Vertices.push_back(Corners[2]); Vertices.push_back(Corners[6]);
    Vertices.push_back(Corners[3]); Vertices.push_back(Corners[7]);
}

void FBoundingBoxLineSource::GenerateOBB(const FOBB* InOBB)
{
    if (!InOBB) { return; }

    FVector Corners[8];
    const FVector& Extents = InOBB->Extents;

    // Define the 8 local corners
    FVector LocalCorners[8];
    LocalCorners[0] = FVector(-Extents.X, -Extents.Y, -Extents.Z);
    LocalCorners[1] = FVector( Extents.X, -Extents.Y, -Extents.Z);
    LocalCorners[2] = FVector( Extents.X,  Extents.Y, -Extents.Z);
    LocalCorners[3] = FVector(-Extents.X,  Extents.Y, -Extents.Z);
    LocalCorners[4] = FVector(-Extents.X, -Extents.Y,  Extents.Z);
    LocalCorners[5] = FVector( Extents.X, -Extents.Y,  Extents.Z);
    LocalCorners[6] = FVector( Extents.X,  Extents.Y,  Extents.Z);
    LocalCorners[7] = FVector(-Extents.X,  Extents.Y,  Extents.Z);

    // Transform local corners to world space
    for (int i = 0; i < 8; ++i)
    {
        Corners[i] = InOBB->ScaleRotation.TransformPosition(LocalCorners[i]) + InOBB->Center;
    }

    // Bottom
    Vertices.push_back(Corners[0]); Vertices.push_back(Corners[1]);
    Vertices.push_back(Corners[1]); Vertices.push_back(Corners[2]);
    Vertices.push_back(Corners[2]); Vertices.push_back(Corners[3]);
    Vertices.push_back(Corners[3]); Vertices.push_back(Corners[0]);

    // Top
    Vertices.push_back(Corners[4]); Vertices.push_back(Corners[5]);
    Vertices.push_back(Corners[5]); Vertices.push_back(Corners[6]);
    Vertices.push_back(Corners[6]); Vertices.push_back(Corners[7]);
    Vertices.push_back(Corners[7]); Vertices.push_back(Corners[4]);

    // Sides
    Vertices.push_back(Corners[0]); Vertices.push_back(Corners[4]);
    Vertices.push_back(Corners[1]); Vertices.push_back(Corners[5]);
    Vertices.push_back(Corners[2]); Vertices.push_back(Corners[6]);
    Vertices.push_back(Corners[3]); Vertices.push_back(Corners[7]);
}

void FBoundingBoxLineSource::GenerateSpotLight(const FSpotLightOBB* InSpotLight)
{
    if (!InSpotLight) { return; }

    GenerateOBB(InSpotLight);

    // You might want to add more lines here to visualize the cone of the spotlight
}
