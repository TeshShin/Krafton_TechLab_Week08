#include "Asset/Shader/Common/CommonConstants.hlsli"

cbuffer LightViewConstants : register(b0)
{
	row_major float4x4 LightViewProjection;
};

// VS Input
struct VS_INPUT
{
	float3 Position : POSITION;
};

// VS Output
struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
};

// ---------------------------------
// Vertex Shader
// ---------------------------------
VS_OUTPUT mainVS(VS_INPUT Input)
{
	VS_OUTPUT Output;

	// 1. 월드 좌표로 변환
	float4 WorldPos = mul(float4(Input.Position, 1.0f), ModelWorld);

	// 2. 빛의 시점으로 변환
	Output.Position = mul(WorldPos, LightViewProjection);

	return Output;
}
