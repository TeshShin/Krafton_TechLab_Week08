#pragma once
#include "Renderer/Public/RenderPass/RenderPass.h"

class FStaticMeshPass : public FRenderPass
{
public:
    FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS);
    void Execute(FRenderingContext& Context) override;
    void Release() override;

    void UpdateLightsFromContext(FRenderingContext& Context);

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;
    
    ID3D11Buffer* ConstantBufferMaterial = nullptr;
    ID3D11Buffer* ConstantBufferLight = nullptr;

    // Unified Dynamic Light Buffer (Point, Spot, Rect lights)
    ID3D11Buffer* UnifiedLightStructuredBuffer = nullptr;
    ID3D11ShaderResourceView* UnifiedLightSRV = nullptr;
    uint32 UnifiedLightCapacity;
};
