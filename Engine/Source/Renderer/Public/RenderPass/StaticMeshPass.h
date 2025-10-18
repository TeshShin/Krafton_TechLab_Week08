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

	TArray<FUnifiedDynamicLight> UnifiedLights;

	ID3D11ComputeShader* LightTilesCS = nullptr;

	ID3D11Buffer* ClusterCountBuffer = nullptr;
	ID3D11UnorderedAccessView* ClusterCountUAV = nullptr;
	ID3D11ShaderResourceView* ClusterCountSRV = nullptr;

	ID3D11Buffer* ClusterIndexBuffer = nullptr;
	ID3D11UnorderedAccessView* ClusterIndexUAV = nullptr;
	ID3D11ShaderResourceView* ClusterIndexSRV = nullptr;

	ID3D11Buffer* FP_CameraCB = nullptr;
	ID3D11Buffer* FP_ParamsCB = nullptr;

    void CreateClusterBuffers(FRenderingContext& Context, uint32 NumLights);

    // Debug heat overlay resources
    ID3D11VertexShader* HeatVS = nullptr;
    ID3D11PixelShader* HeatPS = nullptr;
    ID3D11DepthStencilState* DS_Disabled = nullptr;
    bool bShowClusterHeat = true; // simple always-on toggle for now
};
