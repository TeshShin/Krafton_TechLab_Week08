#include "Asset/Shader/Common/BlitVS.hlsl"

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D SceneTexture : register(t0);
SamplerState PointSampler : register(s0);

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	float2 uv = Input.Position.xy / RenderTargetSize;
	float3 Color = SceneTexture.Sample(PointSampler, uv).xyz;

	return float4(Color, 1.0f);
}
