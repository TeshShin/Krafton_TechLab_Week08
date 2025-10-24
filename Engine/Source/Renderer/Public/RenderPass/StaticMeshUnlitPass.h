#pragma once
#include "Renderer/Public/RenderPass/RenderPass.h"

class FStaticMeshUnlitPass : public FRenderPass
{
public:
    FStaticMeshUnlitPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS);

	virtual bool CanRender(const FRenderingContext& Context) override;
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
	void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VSUnlit = nullptr;
    ID3D11PixelShader* PSUnlit = nullptr;

    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;

    ID3D11Buffer* ConstantBufferMaterial = nullptr;
};
