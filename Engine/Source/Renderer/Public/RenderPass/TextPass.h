#pragma once
#include "RenderPass.h"

struct FFontVertex
{
    FVector Position;        // 3D 월드 좌표
    FVector2 TexCoord;      // 쿼드 내 UV 좌표 (0~1)
    uint32 CharIndex;       // ASCII 문자 코드
};

struct FFontConstantBuffer
{
    FVector2 AtlasSize = FVector2(512.0f, 512.0f);      // 아틀라스 전체 크기
    FVector2 GlyphSize = FVector2(16.0f, 16.0f);         // 한 글자 크기
    FVector2 GridSize = FVector2(32.0f, 32.0f);          // 아틀라스 그리드 (32x32 글자)
    FVector2 Padding = FVector2(0.0f, 0.0f);             // 여백
};

class FTextPass : public FRenderPass
{
public:
    FTextPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS);
	bool CanRender(const FRenderingContext& Context) override;
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    void RenderTextInternal(const FString& Text, const FMatrix& WorldMatrix, ID3D11Buffer* InModelCB);

	ID3D11DepthStencilState* DS;
	ID3D11BlendState* BS;
    // Font rendering resources
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11Buffer* DynamicVertexBuffer = nullptr;
    ID3D11Buffer* FontDataConstantBuffer = nullptr;
    class UTexture* FontTexture = nullptr;
    FFontConstantBuffer ConstantBufferData;

    static constexpr uint32 MAX_FONT_VERTICES = 4096;
};
