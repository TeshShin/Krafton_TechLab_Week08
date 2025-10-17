#pragma once
#include "RenderPass.h"

struct FNormalMapConstants
{
    FVector2 RenderTarget; // RenderTargetSize (width, height)
};

class FNormalMapPass : public FRenderPass
{
public:
    FNormalMapPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11DepthStencilState* InDS);

    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;

    ID3D11DepthStencilState* DS = nullptr;
    ID3D11SamplerState* SamplerState = nullptr;
    ID3D11Buffer* ConstantBufferPerFrame = nullptr;
};
