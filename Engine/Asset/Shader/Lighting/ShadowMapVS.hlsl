#include "../Common/CommonConstants.hlsli"

cbuffer LightViewConstants : register(b0)
{
	row_major float4x4 LightView;
	row_major float4x4 LightProjection;
};

// VS Input
struct VS_INPUT
{
	float3 Position : POSITION;
};

// VS Output
struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float4 LightPos : TEXCOORD0;
};
    
// ---------------------------------
// Vertex Shader
// ---------------------------------
PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;

	// 1. 월드 좌표로 변환
	float4 WorldPos = mul(float4(Input.Position, 1.0f), ModelWorld);

	// 2. 빛의 시점으로 변환
	Output.LightPos = mul(WorldPos, LightView);
	Output.Position = mul(Output.LightPos, LightProjection); 
	
	return Output; 
} 
