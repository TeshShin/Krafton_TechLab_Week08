#include "pch.h"
#include "Physics/Public/BoundingSphere.h"

bool FBoundingSphere::RaycastHit() const
{
	return false;
}

bool FBoundingSphere::IsIntersected(const FAABB& Other)
{
	float sqDist = Other.GetCenterDistanceSquared(Center);
	return sqDist <= (Radius * Radius);
}
