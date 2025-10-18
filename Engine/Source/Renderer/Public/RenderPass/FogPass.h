#pragma once
#include "Renderer/Public/RenderPass/RenderPass.h"

// Matches the layout in DecalShader.hlsl

struct FFogConstants
{
    FVector4 FogColor;
    float FogDensity;
    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;
    float FogZ;
    float Padding[2];
};

struct FCameraInverseConstants
{
    FMatrix ViewInverse;
    FMatrix ProjectionInverse;
};

struct FViewportConstants
{
    FVector2 RenderTargetSize;
    float Padding[2];
};

class FFogPass : public FRenderPass
{
public:
    FFogPass(UPipeline* InPipeline, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBlendState);

	bool CanRender(const FRenderingContext& Context);
	void SetRenderTargets(class UDeviceResources* DeviceResources) override;
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11DepthStencilState* DS = nullptr;
    ID3D11BlendState* BS = nullptr;
	ID3D11SamplerState* Sampler = nullptr;

    ID3D11Buffer* ConstantBufferFog = nullptr;
    ID3D11Buffer* ConstantBufferCameraInverse = nullptr;
    ID3D11Buffer* ConstantBufferViewportInfo = nullptr;

	ID3D11ShaderResourceView* DepthSRV = nullptr;
};
