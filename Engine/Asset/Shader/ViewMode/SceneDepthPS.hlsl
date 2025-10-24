// ================================================================
// SceneDepthView.hlsl
// - Renders a linearized depth view for the editor.
// - Input: Depth Texture
// - Output: Grayscale Linear Depth
// ================================================================
#include "Asset/Shader/Common/BlitVS.hlsl"

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D SceneTexture : register(t0);
SamplerState PointSampler : register(s0);

// ================================================================
// Pixel Shader
// - Samples non-linear depth and converts it to linear depth.
// ================================================================
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float2 ScreenPosition = Input.Position.xy;
    float2 uv = ScreenPosition / RenderTargetSize;

    float nonLinearDepth = SceneTexture.Sample(PointSampler, uv).r;
    float linearDepth;

    // IsOrthographic 플래그 값에 따라 분기
    float BandSize = 0.001f;
    if (IsOrthographic > 0)
    {
        // 직교 투영일 경우: 깊이 값은 이미 선형적이므로 그대로 사용
        linearDepth = nonLinearDepth;
    }
    else
    {
        // 원근 투영일 경우: 기존의 선형화 공식을 사용
        float viewSpaceDepth = (FarClip * NearClip) / (nonLinearDepth * (NearClip - FarClip) + FarClip);
        linearDepth = saturate(viewSpaceDepth / FarClip);
        BandSize = 0.02f;
    }

    float scaledDepth = linearDepth / BandSize;
    float sawtooth = frac(scaledDepth);
    return float4(sawtooth, sawtooth, sawtooth, 1.0f);
}
