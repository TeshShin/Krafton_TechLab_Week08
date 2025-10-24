#ifndef COMMON_CONSTANTS_INCLUDE
#define COMMON_CONSTANTS_INCLUDE

/// Common Constants ///
cbuffer Camera : register(b11)
{
	row_major float4x4 View;
	row_major float4x4 Projection;
	float3 ViewWorldLocation;
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
