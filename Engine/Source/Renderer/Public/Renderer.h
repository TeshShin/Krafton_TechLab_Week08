#pragma once
#include "DeviceResources.h"
#include "Core/Public/Object/Object.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Renderer/Public/Pipeline.h"

class FViewport;
class UCamera;
class UPipeline;
class FViewportClient;
class FFXAAPass;

/**
 * @brief Rendering Pipeline 전반을 처리하는 클래스
 */
UCLASS()
class URenderer : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(URenderer, UObject)

public:
	void Init(HWND InWindowHandle);
	void Release();

	// Initialize
	void CreateDepthStencilState();
	void CreateBlendState();
	void CreateDefaultShader();
	void CreateConstantBuffers();

	// Release
	void ReleaseConstantBuffers();
	void ReleaseDefaultShader();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();

	// Render
	void Render();

	void RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride = 0, uint32 InIndexBufferStride = 0);

	void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0) const;

	// Getter & Setter
	ID3D11Device* GetDevice() const { return DeviceResources->GetDevice(); }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceResources->GetDeviceContext(); }
	IDXGISwapChain* GetSwapChain() const { return DeviceResources->GetSwapChain(); }

	ID3D11RenderTargetView* GetBackBufferRTV() const { return DeviceResources->GetBackBufferRTV(); }
	ID3D11ShaderResourceView* GetBackBufferSRV() const { return DeviceResources->GetBackBufferSRV(); }

	ID3D11RenderTargetView* GetDestinationRTV() const {return DeviceResources->GetDestinationRTV(); }
	ID3D11ShaderResourceView* GetSourceSRV() const{return DeviceResources->GetSourceSRV(); }
	ID3D11RenderTargetView* GetSourceRTV() const{return DeviceResources->GetSourceRTV(); }

	ID3D11RenderTargetView* GetNormalBufferRTV() const { return DeviceResources->GetNormalBufferRTV(); }
	ID3D11ShaderResourceView* GetNormalBufferSRV() const { return DeviceResources->GetNormalBufferSRV(); }

	ID3D11DepthStencilView* GetDepthBufferDSV() const { return DeviceResources->GetDepthBufferDSV(); }
	ID3D11ShaderResourceView* GetDepthBufferSRV() const { return DeviceResources->GetDepthBufferSRV(); }

	UDeviceResources* GetDeviceResources() const { return DeviceResources; }
	FViewport* GetViewportClient() const { return ViewportClient; }
	UPipeline* GetPipeline() const { return Pipeline; }
	bool GetIsResizing() const { return bIsResizing; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

private:
	void RenderBegin() const;

	void RenderLevel(struct FRenderingContext& RenderingContext);
	void RenderPostProcess(struct FRenderingContext& RenderingContext);
	void RenderByViewMode(struct FRenderingContext& RenderingContext);
	void RenderEditorDepth(struct FRenderingContext& RenderingContext);
	void RenderEditorOverlay(struct FRenderingContext& RenderingContext);

	void RenderEnd() const;

	UPipeline* Pipeline = nullptr;
	UDeviceResources* DeviceResources = nullptr;

	// DS
	ID3D11DepthStencilState* DefaultDS = nullptr;
	ID3D11DepthStencilState* ReadOnlyDS = nullptr;
	ID3D11DepthStencilState* DisabledDS = nullptr;

	// BS
	ID3D11BlendState* AlphaBlendState = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;

	// Constant Buffers
	ID3D11Buffer* ConstantBufferModels = nullptr;
	ID3D11Buffer* ConstantBufferViewProj = nullptr;
	ID3D11Buffer* ConstantBufferColor = nullptr;

	// Default Shaders
	ID3D11VertexShader* DefaultVertexShader = nullptr;
	ID3D11PixelShader* DefaultPixelShader = nullptr;
	ID3D11InputLayout* DefaultInputLayout = nullptr;

	FViewport* ViewportClient = nullptr;

	bool bIsResizing = false;

	TArray<class FRenderPass*> LevelPasses;
	TArray<class FRenderPass*> PostProcessPasses;
	TMap<EViewModeIndex, class FRenderPass*> ViewModePasses;
	TArray<class FRenderPass*> EditorDepthPasses;
	TArray<class FRenderPass*> EditorOverlayPasses;
};
