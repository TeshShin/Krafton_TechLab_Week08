#pragma once
#include "Renderer/Public/RenderPass/RenderPass.h"
#include "Renderer/Public/LightData.h"

class FStaticMeshPass : public FRenderPass
{
public:
    FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferModel, ID3D11DepthStencilState* InDS);

	virtual bool CanRender(const FRenderingContext& Context);
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
	void Execute(FRenderingContext& Context) override;
    void Release() override;
	
	TArray<FUnifiedDynamicLight> CollectLightsFromContext(FRenderingContext& Context);

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

	TArray<FUnifiedDynamicLight> UnifiedLights;
};
