#pragma once
#include "RenderPass.h"

class FBillboardPass : public FRenderPass
{
public:
    FBillboardPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS);

	bool CanRender(const FRenderingContext& Context) override;
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;
    ID3D11BlendState* BS = nullptr;
	ID3D11Buffer* ConstantBufferColor = nullptr;
};
