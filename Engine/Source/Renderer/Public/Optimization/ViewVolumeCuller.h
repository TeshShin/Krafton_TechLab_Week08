#pragma once

#include "Scene/Public/Component/PrimitiveComponent.h"
#include "Physics/Public/AABB.h"

class FOctree;

enum class EBoundCheckResult
{
	Outside,
	Intersect,
	Inside
};

struct FFrustum
{
    FVector4 Planes[6];

    EBoundCheckResult CheckIntersection(const FAABB& BBox) const
    {
        EBoundCheckResult Result = EBoundCheckResult::Inside;

        for (int i = 0; i < 6; ++i)
        {
            const FVector4& P = Planes[i];

			// positive vertex: 법선 방향으로 가장 먼 꼭짓점
			FVector Positive(
				P.X >= 0.0f ? BBox.Max.X : BBox.Min.X,
				P.Y >= 0.0f ? BBox.Max.Y : BBox.Min.Y,
				P.Z >= 0.0f ? BBox.Max.Z : BBox.Min.Z
			);

			// 양의 꼭짓점이 평면 바깥(음수)면 AABB는 절두체 밖
			if (P.Dot3(Positive) + P.W < 0.0f)
			{
				return EBoundCheckResult::Outside;
			}

			// Negative vertex: 법선 반대 방향의 꼭짓점
			FVector Negative(
				P.X >= 0.0f ? BBox.Min.X : BBox.Max.X,
				P.Y >= 0.0f ? BBox.Min.Y : BBox.Max.Y,
				P.Z >= 0.0f ? BBox.Min.Z : BBox.Max.Z
			);

			// 음의 꼭짓점이 바깥이면(절두체와 일부 교차) 결과를 Intersect로 표기
			if (P.Dot3(Negative) + P.W < 0.0f)
			{
				Result = EBoundCheckResult::Intersect;
			}
        }

        return Result;
    }

    void Clear() { for (int i = 0; i < 6; ++i) { Planes[i] = FVector4::Zero(); }; }
};

class ViewVolumeCuller
{
public:
	ViewVolumeCuller() = default;
	~ViewVolumeCuller() = default;
	ViewVolumeCuller(const ViewVolumeCuller& Other) = default;
	ViewVolumeCuller& operator=(const ViewVolumeCuller& Other) = default;

	void Cull(
        FOctree* StaticOctree,
        TArray<UPrimitiveComponent*>& DynamicPrimitives,
		const FCameraConstants& ViewProjConstants
	);

	const TArray<UPrimitiveComponent*>& GetRenderableObjects();
private:
    void CullOctree(FOctree* Octree);

    FFrustum CurrentFrustum{};
    TArray<UPrimitiveComponent*> RenderableObjects{};
};
