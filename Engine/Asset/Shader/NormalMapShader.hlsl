// ================================================================
// NormalMapShader.hlsl
// - Visualizes the world-space normals stored in the normal buffer.
// - Input : NormalBuffer (encoded in [0,1])
// - Output: RGB visualization of normals
// ================================================================

// ------------------------------------------------
// Constant Buffers
// ------------------------------------------------
cbuffer PerFrameConstants : register(b0)
{
    float2 RenderTargetSize; // (width, height)
};

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D NormalTexture : register(t0);
Texture2D DepthTexture  : register(t1);
SamplerState PointSampler : register(s0);

// ------------------------------------------------
// VS/PS I/O
// ------------------------------------------------
struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

// Fullscreen triangle via SV_VertexID
PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    output.Position.y *= -1.0f; // Flip Y to match UV
    return output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float2 uv = Input.Position.xy / RenderTargetSize;

    // Discard where no geometry was rendered (depth cleared to 1.0)
    float depth = DepthTexture.Sample(PointSampler, uv).r;
    if (depth >= 0.9999f)
    {
        discard;
    }

    // Stored normal is encoded to [0,1]. For visualization, we can show it directly.
    float3 encodedNormal = NormalTexture.Sample(PointSampler, uv).xyz;

    // Optionally decode/encode to ensure proper normalization
    float3 normalWS = normalize(encodedNormal * 2.0f - 1.0f);
    float3 vis = normalWS * 0.5f + 0.5f;

    return float4(vis, 1.0f);
}
