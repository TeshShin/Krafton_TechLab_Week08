#include "Asset/Shader/Common/CommonConstants.hlsli"

cbuffer LightViewConstants : register(b0)
{
	row_major float4x4 LightViewProjection;
};

cbuffer LightConstants : register(b1)
{
	float3 LightPosition;
	float LightRadius;
};

struct VS_INPUT
{
	float3 Position : POSITION;
};

struct PS_INPUT
{
	float4 ClipPosition   : SV_POSITION;
	float3 WorldPosition  : TEXCOORD0;
};

// ---------------------------------
// Vertex Shader
// ---------------------------------
PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;

	// 1. 월드 좌표로 변환
	float4 WorldPos = mul(float4(Input.Position, 1.0f), ModelWorld);

	// 2. 빛의 시점으로 변환 (SV_POSITION)
	Output.ClipPosition = mul(WorldPos, LightViewProjection);

	// 3. 픽셀 셰이더로 월드 좌표 전달
	Output.WorldPosition = WorldPos.xyz;

	return Output;
}

// ---------------------------------
// Pixel Shader (mainPS)
// ---------------------------------
float4 mainPS(PS_INPUT Input) : SV_Target
{
	float linearDepth = length(Input.WorldPosition - LightPosition) / LightRadius;
	return float4(linearDepth, 0.0f, 0.0f, 1.0f);
}
