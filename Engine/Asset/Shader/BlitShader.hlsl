cbuffer PerFrameConstants : register(b0)
{
    float2 RenderTargetSize;
};

cbuffer Camera : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 ViewWorldLocation;
    float NearClip;
    float FarClip;
};

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D SceneTexture : register(t0);
SamplerState PointSampler : register(s0);

// ------------------------------------------------
// Vertex and Pixel Shader I/O
// ------------------------------------------------
struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

// ================================================================
// Vertex Shader
// - Generates a full-screen triangle without needing a vertex buffer.
// ================================================================
PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;

    // SV_VertexID를 사용하여 화면을 덮는 큰 삼각형의 클립 공간 좌표를 생성
    // ID 0 -> (-1, 1), ID 1 -> (3, 1), ID 2 -> (-1, -3) -- 수정된 좌표계
    // 이 좌표계는 UV가 (0,0)부터 시작하도록 조정합니다.
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    output.Position.y *= -1.0f;

    return output;
}

// ================================================================
// Pixel Shader
// ================================================================
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	float2 uv = Input.Position.xy / RenderTargetSize;
	float3 Color = SceneTexture.Sample(PointSampler, uv).xyz;

	return float4(Color, 1.0f);
}
