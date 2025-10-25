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
	void CreateConstantBuffers();
	void CreateRenderPasses();

	// Release
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseConstantBuffers();
	void ReleaseRenderPasses();

	// Render
	void Render();

	void OnResize(uint32 InWidth = 0, uint32 InHeight = 0) const;

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

	// Blend state accessors for passes that need explicit blending
	ID3D11BlendState* GetAlphaBlendState() const { return AlphaBlendState; }
	ID3D11BlendState* GetAdditiveBlendState() const { return AdditiveBlendState; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

	// Shadow rasterizer bias (global)
	void SetShadowSlopeScaledDepthBias(float InValue) { ShadowSlopeScaledDepthBias = InValue; }
	void SetShadowDepthBiasClamp(float InValue) { ShadowDepthBiasClamp = InValue; }
	void SetShadowDepthBias(int32 InValue) { ShadowDepthBias = InValue; }

	float GetShadowSlopeScaledDepthBias() const { return ShadowSlopeScaledDepthBias; }
	float GetShadowDepthBiasClamp() const { return ShadowDepthBiasClamp; }
	int32 GetShadowDepthBias() const { return ShadowDepthBias; }
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
	ID3D11Buffer* CameraCB = nullptr;
	ID3D11Buffer* ModelCB = nullptr;
	ID3D11Buffer* ViewportCB = nullptr;

	FViewport* ViewportClient = nullptr;

	bool bIsResizing = false;

	TArray<class FRenderPass*> LevelPasses;
	TArray<class FRenderPass*> PostProcessPasses;
	TArray<class FRenderPass*> ViewModePasses;
	TMap<EViewModeIndex, class FRenderPass*> ViewModePassesMap;
	class FRenderPass* EditorDepthPass;
	class FRenderPass* EditorOverlayPass;
	TArray<class FRenderPass*> EditorOverlayPasses;

// Shader Hot Reload Section
private:
	/**
	 * @brief Check for modified shader files and auto-reload them
	 *
	 * Called periodically (every ShaderCheckInterval seconds) to detect shader changes
	 * and automatically recompile modified shaders.
	 */
	void CheckShaderHotReload();

	// Shader Hot-Reload
	float ShaderCheckAccumulator = 0.0f;           ///< Time accumulator for periodic shader checks
	const float ShaderCheckInterval = 0.5f;        ///< Check shader files every 0.5 seconds (configurable)
// Shadow rasterizer bias parameters
private:
	float ShadowSlopeScaledDepthBias = 1.5f;
	float ShadowDepthBiasClamp = 0.0f;
	int32 ShadowDepthBias = 0;
};
