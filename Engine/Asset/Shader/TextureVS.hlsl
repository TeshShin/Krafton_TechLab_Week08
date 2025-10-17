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

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPosition: TEXCOORD0;
	float3 WorldNormal : TEXCOORD1;
	float2 Tex : TEXCOORD2;
	float3 WorldTangent : TEXCOORD3;
	float  TangentSign  : TEXCOORD4;
};


PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
	Output.WorldNormal = normalize(mul(Input.Normal, (float3x3)WorldInverseTranspose));
	Output.Tex = Input.Tex;

    float3 worldT = mul(Input.Tangent.xyz, (float3x3)World);
    worldT = normalize(worldT - Output.WorldNormal * dot(Output.WorldNormal, worldT));
    Output.WorldTangent = worldT;
    Output.TangentSign = Input.Tangent.w;

    return Output;
}
