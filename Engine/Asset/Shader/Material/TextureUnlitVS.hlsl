#include "Asset/Shader/Common/CommonConstants.hlsli"
#include "Asset/Shader/Lighting/LightingFunctions.hlsli"

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
	Output.WorldPosition = mul(float4(Input.Position, 1.0f), ModelWorld).xyz;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), ModelWorld), View), Projection);
	Output.WorldNormal = normalize(mul(Input.Normal, (float3x3) ModelWorldInverseTranspose));
	Output.Tex = Input.Tex;
	Output.TotalDiffuse = Input.Color;
	Output.WorldTangent = float3(0.0f, 0.0f, 0.0f);
	Output.TangentSign = 0.0f;
	return Output;
}
