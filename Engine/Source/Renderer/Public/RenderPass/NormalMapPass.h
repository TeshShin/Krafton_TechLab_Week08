#pragma once
#include "RenderPass.h"

class FNormalMapPass : public FRenderPass
{
public:
    FNormalMapPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS);

	bool CanRender(const FRenderingContext& Context) override;
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;

    ID3D11DepthStencilState* DS = nullptr;
	ID3D11ShaderResourceView* NormalSRV = nullptr;
    ID3D11SamplerState* SamplerState = nullptr;
};
