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
	float4 TotalColor;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
	float4 Tangent : TANGENT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;	// Transformed position to pass to the pixel shader
    float4 Color : COLOR;			// Color to pass to the pixel shader
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);

    Output.Color = Input.Color;

    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	float4 FinalColor = lerp(Input.Color, TotalColor, TotalColor.a);
	return FinalColor;
}
