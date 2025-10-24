#include "Asset/Shader/Common/CommonConstants.hlsli"

cbuffer FontDataBuffer : register(b0)
{
    float2 AtlasSize;      // 512.0, 512.0
    float2 GlyphSize;      // 16.0, 16.0
    float2 GridSize;       // 32.0, 32.0
    float2 Padding;
};

// 입력 구조체
struct VSInput
{
	float3 Position : POSITION;     // FVector (3 floats)
	float2 TexCoord : TEXCOORD0;    // FVector2 (2 floats)
	uint CharIndex : TEXCOORD1;     // uint32 문자 인덱스
};

struct PSInput
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	uint CharIndex : TEXCOORD1;
};

// Texture and Sampler
Texture2D FontAtlas : register(t0);
SamplerState FontSampler : register(s0);

// Vertex shader
PSInput mainVS(VSInput Input)
{
	PSInput Output;

	// 월드 좌표계로 변환
	float4 WorldPos = mul(float4(Input.Position, 1.0f), ModelWorld);

	// 뷰-프로젝션 변환
	Output.Position = mul(WorldPos, View);
	Output.Position = mul(Output.Position, Projection);

	// ASCII 문자를 16x16 그리드로 매핑 (범용적 처리)
	// ASCII 코드를 기반으로 그리드 위치 계산
	uint col = Input.CharIndex % 16;  // 열 (0-15)
	uint row = Input.CharIndex / 16;  // 행 (0-15)
	float2 gridPos = float2(float(col), float(row));

	// 16x16 그리드 셀 크기 계산
	float2 cellSize = float2(1.0f / 16.0f, 1.0f / 16.0f);

	// 최종 UV 좌표 계산: 그리드 위치 + 셀 내부 오프셋
	float2 atlasUV = (gridPos * cellSize) + (Input.TexCoord * cellSize);
	Output.TexCoord = atlasUV;

	Output.CharIndex = Input.CharIndex;

	return Output;
}

// Pixel shader
float4 mainPS(PSInput Input) : SV_TARGET
{
	// 폰트 텍스처에서 색상 샘플링
	float4 AtlasColor = FontAtlas.Sample(FontSampler, Input.TexCoord);

	// 흰색 글자에 알파 블렌딩 적용
	float4 FinalColor = float4(1.0f, 1.0f, 1.0f, AtlasColor.r);

	// 투명한 픽셀은 폐기 (선택사항 - 성능 향상)
	if (FinalColor.a < 0.01f)
		discard;

	return FinalColor;
}
