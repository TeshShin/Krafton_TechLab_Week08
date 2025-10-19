// ================================================================
// ClusterHeatShader.hlsl
// - Debug fullscreen overlay visualizing Forward+ cluster light counts
// - Reads FP_ClusterCount (t7) and FP cbuffers (b11,b12)
// ================================================================

// Camera and tiling parameters (must match TexturePS / LightTilesComputeShader)
cbuffer FP_CameraCB : register(b11)
{
    row_major float4x4 FP_View;
    row_major float4x4 FP_Projection;
    row_major float4x4 FP_InvProj;
    uint2   FP_ScreenSize;     // pixels (width, height)
    uint2   FP_ViewportOrigin; // pixels (top-left x,y)
    uint    FP_NumTilesX;      // dispatch dim X
    uint    FP_NumTilesY;      // dispatch dim Y
    uint    FP_NumZSlices;     // dispatch dim Z
    float   FP_NearZ;          // view-space near (>= 0)
    float   FP_FarZ;           // view-space far  (>  NearZ)
};

cbuffer FP_ForwardPlusCB : register(b12)
{
    uint FP_NumLights;                // number of entries in DynamicLights
    uint FP_MaxLightsPerCluster;      // capacity per cluster
    uint FP_TotalClusters;            // NumTilesX*NumTilesY*NumZSlices
    uint FP_Pad0;
};

// Cluster counts per cluster (size: FP_TotalClusters)
StructuredBuffer<uint> FP_ClusterCount : register(t7);

struct VS_OUT
{
    float4 Position : SV_POSITION;
};

// Fullscreen triangle via SV_VertexID
VS_OUT mainVS(uint vertexID : SV_VertexID)
{
    VS_OUT o;
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    o.Position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    o.Position.y *= -1.0f; // Flip Y for DX screen
    return o;
}

float3 HeatColor(float t)
{
    // Simple blue->cyan->green->yellow->red gradient
    t = saturate(t);
    float3 c0 = float3(0.0, 0.0, 1.0); // blue
    float3 c1 = float3(0.0, 1.0, 1.0); // cyan
    float3 c2 = float3(0.0, 1.0, 0.0); // green
    float3 c3 = float3(1.0, 1.0, 0.0); // yellow
    float3 c4 = float3(1.0, 0.0, 0.0); // red

    if (t < 0.25) { float u = t / 0.25; return lerp(c0, c1, u); }
    if (t < 0.50) { float u = (t - 0.25) / 0.25; return lerp(c1, c2, u); }
    if (t < 0.75) { float u = (t - 0.50) / 0.25; return lerp(c2, c3, u); }
    float u = (t - 0.75) / 0.25; return lerp(c3, c4, u);
}

float4 mainPS(VS_OUT input) : SV_TARGET
{
    // Full-viewport overlay
    float2 pix2 = input.Position.xy - float2(FP_ViewportOrigin);
    // Compute tile index from pixel (fractional map to tiles)
    int tileX = clamp(int(floor(pix2.x * (float)FP_NumTilesX / max(FP_ScreenSize.x, 1))), 0, int(FP_NumTilesX - 1));
    int tileY = clamp(int(floor(pix2.y * (float)FP_NumTilesY / max(FP_ScreenSize.y, 1))), 0, int(FP_NumTilesY - 1));

    // Sum counts across all z-slices for this tile
    uint sum = 0;
    [loop]
    for (uint z = 0; z < FP_NumZSlices; ++z)
    {
        uint cid = (z * FP_NumTilesY + tileY) * FP_NumTilesX + tileX;
        sum += FP_ClusterCount[cid];
    }

    // Normalize against a dynamic scale favoring visibility.
    // Use a fraction of total lights so colors warm up with plausible counts.
    float heatNorm = max(1.0, (float)FP_NumLights * 0.5);
    float t = saturate(log(1.0 + (float)sum) / log(1.0 + heatNorm));
    float3 rgb = HeatColor(t);

    // If no lights at all, show deep blue
    if (FP_NumLights == 0)
        rgb = float3(0.0, 0.0, 0.25);

    // Slight transparency for overlay blending
    return float4(rgb, 0.65);
}
