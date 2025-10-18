#pragma once
#include "Renderer/Public/RenderPass/RenderPass.h"

class FStaticMeshPass : public FRenderPass
{
public:
    FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
        ID3D11VertexShader* InVSPhong, ID3D11PixelShader* InPSPhong, ID3D11VertexShader* InVSLambert, ID3D11PixelShader* InPSLambert, ID3D11VertexShader* InVSGouraud, ID3D11PixelShader* InPSGouraud, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS);
    void Execute(FRenderingContext& Context) override;
    void Release() override;

    /**
     * @brief Update lights from rendering context and upload to GPU
     * @return Number of dynamic lights in the unified buffer
     */
    uint32 UpdateLightsFromContext(FRenderingContext& Context);

private:
    ID3D11VertexShader* VSPhong = nullptr;
    ID3D11PixelShader* PSPhong = nullptr;

	ID3D11VertexShader* VSLambert = nullptr;
	ID3D11PixelShader* PSLambert = nullptr;

	ID3D11VertexShader* VSGouraud = nullptr;
	ID3D11PixelShader* PSGouraud = nullptr;

    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;
    
    ID3D11Buffer* ConstantBufferMaterial = nullptr;
    ID3D11Buffer* ConstantBufferLight = nullptr;

    // Unified Dynamic Light Buffer (Point, Spot, Rect lights)
    ID3D11Buffer* UnifiedLightStructuredBuffer = nullptr;
    ID3D11ShaderResourceView* UnifiedLightSRV = nullptr;
    uint32 UnifiedLightCapacity;
};
