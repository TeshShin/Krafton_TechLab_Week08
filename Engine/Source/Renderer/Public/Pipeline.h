#pragma once

struct FPipelineInfo
{
	ID3D11InputLayout* InputLayout;
	ID3D11VertexShader* VertexShader;
	ID3D11RasterizerState* RasterizerState;
	ID3D11DepthStencilState* DepthStencilState;
	ID3D11PixelShader* PixelShader;
	ID3D11BlendState* BlendState;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

class UPipeline
{
public:
	UPipeline(ID3D11DeviceContext* InDeviceContext);
	~UPipeline();

	void UpdatePipeline(FPipelineInfo Info);

	void SetIndexBuffer(ID3D11Buffer* indexBuffer, uint32 stride);

	void SetVertexBuffer(ID3D11Buffer* VertexBuffer, uint32 Stride);

	void SetConstantBuffer(uint32 Slot, EShaderType ShaderType, ID3D11Buffer* ConstantBuffer);

	void SetSRV(uint32 Slot, EShaderType ShaderType, ID3D11ShaderResourceView* Srv);

	void SetSamplerState(uint32 Slot, EShaderType ShaderType, ID3D11SamplerState* SamplerState);

	void SetRenderTargets(uint32 NumViews, ID3D11RenderTargetView* const *RenderTargetViews, ID3D11DepthStencilView* DepthStencilView);

	void Draw(uint32 VertexCount, uint32 StartLocation);

	void DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation);

private:
	FPipelineInfo LastPipelineInfo{};
	ID3D11DeviceContext* DeviceContext;

	// 현재 파이프라인에 바인딩된 RTV, DSV 상태 캐싱
	TArray<ID3D11RenderTargetView*> CurrentRTVs;
	ID3D11DepthStencilView* CurrentDSV = nullptr;
};
