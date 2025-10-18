#pragma once
#include "AABB.h"
#include "Physics/Public/BoundingVolume.h"

struct FBoundingSphere : public IBoundingVolume
{
	FVector Center;
	float Radius;

	FBoundingSphere(const FVector& InCenter, float InRadius) : Center(InCenter), Radius(InRadius) {}

	bool RaycastHit() const override;
	bool IsIntersected(const FAABB& Other);

	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::Sphere; }
};
