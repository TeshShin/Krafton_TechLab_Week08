// ================================================================
// NormalMapShader.hlsl
// - Visualizes the world-space normals stored in the normal buffer.
// - Input : NormalBuffer (encoded in [0,1])
// - Output: RGB visualization of normals
// ================================================================
#include "Asset/Shader/Common/BlitVS.hlsl"
// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D SceneTexture : register(t0);
SamplerState PointSampler : register(s0);

Texture2D DepthTexture  : register(t1);

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float2 uv = Input.Position.xy / RenderTargetSize;

    // Stored normal is encoded to [0,1]. For visualization, we can show it directly.
    float3 encodedNormal = SceneTexture.Sample(PointSampler, uv).xyz;

    // Optionally decode/encode to ensure proper normalization
    float3 normalWS = normalize(encodedNormal * 2.0f - 1.0f);
    float3 vis = normalWS * 0.5f + 0.5f;

    return float4(vis, 1.0f);
}
