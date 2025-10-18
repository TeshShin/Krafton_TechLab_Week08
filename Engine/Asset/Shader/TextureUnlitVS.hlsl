#include "LightingFunctions.hlsl"

cbuffer Model : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 WorldInverseTranspose;
}

cbuffer Camera : register(b1)
{
	row_major float4x4 View;
	row_major float4x4 Projection;
	float3 ViewWorldLocation;
	float NearClip;
	float FarClip;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
	float4 Tangent : TANGENT; // xyz: tangent, w: handedness
};

PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
	Output.WorldNormal = normalize(mul(Input.Normal, (float3x3) WorldInverseTranspose));
	Output.Tex = Input.Tex;
	Output.TotalDiffuse = Input.Color;
	Output.WorldTangent = float3(0.0f, 0.0f, 0.0f);
	Output.TangentSign = 0.0f;	
	return Output;
}
