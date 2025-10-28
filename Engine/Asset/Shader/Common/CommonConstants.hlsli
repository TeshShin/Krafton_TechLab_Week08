#ifndef COMMON_CONSTANTS_INCLUDE
#define COMMON_CONSTANTS_INCLUDE


/// Common Define ///
#define SF_Billboard    (1u << 0)
#define SF_Bounds       (1u << 1)
#define SF_StaticMesh   (1u << 2)
#define SF_Text         (1u << 3)
#define SF_Decal        (1u << 4)
#define SF_FXAA         (1u << 5)
#define SF_Fog          (1u << 6)
#define SF_Shadow       (1u << 7)
#define SF_Octree       (1u << 8)
#define SF_ClusterHeat  (1u << 9)
/// Common Define ///

/// Common Constants ///
cbuffer Camera : register(b11)
{
	row_major float4x4 View;
	row_major float4x4 Projection;
	float3 ViewWorldLocation;
	uint ShowFlags;
	float NearClip;
	float FarClip;
}

cbuffer Model : register(b12)
{
	row_major float4x4 ModelWorld;
	row_major float4x4 ModelWorldInverseTranspose;
}

cbuffer Viewport : register(b13)
{
	float2 RenderTargetSize;
	int IsOrthographic;
}

/// Common Constants ///

#endif
