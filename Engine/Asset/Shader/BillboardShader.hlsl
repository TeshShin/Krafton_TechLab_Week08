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

cbuffer Color : register(b2)
{
	float4 Color;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float4 Color : TEXCOORD0;
	float2 Tex : TEXCOORD1;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
	Output.Color = Color;
	Output.Tex = Input.Tex;

	return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	float4 FinalColor = Input.Color;
	FinalColor *= Texture.Sample(Sampler, Input.Tex);

	if(FinalColor.a < 0.1f) { discard; }
	return FinalColor;
}
