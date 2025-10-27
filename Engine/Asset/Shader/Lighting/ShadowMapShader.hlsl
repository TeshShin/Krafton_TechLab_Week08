#include "../Common/CommonConstants.hlsli"

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1
#define LIGHT_TYPE_SPOT        2
#define LIGHT_TYPE_AMBIENT     3

cbuffer ShadowLightInfo : register(b0)
{
	row_major float4x4 LightView;
	row_major float4x4 LightProjection;
	float3 LightPosition;
	float LightRadius;
	uint LightType;
}

struct VS_INPUT
{
	float3 Position : POSITION;
};

struct PS_INPUT
{
	float4 Position   : SV_POSITION;
	float4 ViewPosition  : TEXCOORD0;
	float4 WorldPosition  : TEXCOORD1;
};

float2 ComputeMomentsVSM(float Depth01)
{
	float2 M;
	M.x = Depth01;

	// Add small variance based on depth derivatives to reduce light leaking
	float Dx = ddx(Depth01);
	float Dy = ddy(Depth01);
	float D2 = Dx * Dx + Dy * Dy;
	// Clamp derivative energy to avoid exploding variance
	D2 = min(D2, 0.25);
	M.y = Depth01 * Depth01 + 0.25 * D2;

	return M;
}

// ---------------------------------
// Vertex Shader
// ---------------------------------
PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;

	Output.WorldPosition = mul(float4(Input.Position, 1.0f), ModelWorld);
	Output.ViewPosition = mul(Output.WorldPosition, LightView);
	Output.Position = mul(Output.ViewPosition, LightProjection);

	return Output;
}

// ---------------------------------
// Pixel Shader (mainPS)
// ---------------------------------
float4 mainPS(PS_INPUT Input) : SV_Target
{
	float FinalLinearDepth;

	if (LightType == LIGHT_TYPE_POINT)
	{
		FinalLinearDepth = length(Input.WorldPosition - LightPosition.xyz) / LightRadius;
	}
	else
	{
		FinalLinearDepth = Input.ViewPosition.z;
	}

	float2 Moment = ComputeMomentsVSM(FinalLinearDepth);
	return float4(Moment, 0.0f, 0.0f);
}
