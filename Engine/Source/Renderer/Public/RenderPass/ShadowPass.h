#pragma once
#include "RenderPass.h"

class FShadowPass : public FRenderPass
{
public:
	FShadowPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS);

	bool CanRender(const FRenderingContext& Context) override;
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
	void Execute(FRenderingContext& Context) override;
	void Release() override;

private:
	void RenderAllStaticMeshes(const FRenderingContext& Context) const;

	ID3D11VertexShader* VS = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
	ID3D11Buffer* CBLightViewProj = nullptr;

	ID3D11DepthStencilState* DS = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;

};
