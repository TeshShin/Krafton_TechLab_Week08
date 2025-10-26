#pragma once

class FShadowMapManager
{
public:
	FShadowMapManager();
	~FShadowMapManager();

	FShadowMapManager(FShadowMapManager const&) = delete;
	FShadowMapManager& operator=(FShadowMapManager const&) = delete;

	static FShadowMapManager& GetInstance();

public:
	void Initalize(uint32 InMaxShadows, uint32 InResolution);
	void Release();

	void ClearShadowMaps();
	void AllocateShadowMap(class ULightComponentBase* Light);

	ID3D11ShaderResourceView* GetSRV() const { return ShadowMapArraySRV; }
	ID3D11DepthStencilView* GetDSV(uint32 ShadowMapIdx) const;
	ID3D11SamplerState* GetSamplerState() const { return ShadowMapSamplerState; }

	// VSM moments resources
	ID3D11ShaderResourceView* GetMomentsSRV() const { return ShadowMomentsSRV; }
	ID3D11RenderTargetView* GetMomentsRTV(uint32 ShadowMapIdx) const { return ShadowMomentsSliceRTVs[ShadowMapIdx]; }
	ID3D11SamplerState* GetLinearSampler() const { return ShadowLinearSamplerState; }

	uint32 GetResolution() const { return Resolution; }

private:
	uint32 MaxShadows;
	uint32 Resolution;
	uint32 CurrentShadowIdx = 0;

	ID3D11Texture2D* ShadowMapArrayTexture;
	ID3D11ShaderResourceView* ShadowMapArraySRV;
	TArray<ID3D11DepthStencilView*> ShadowMapSliceDSVs;
	ID3D11SamplerState* ShadowMapSamplerState;

	// VSM Moments (RG32F) as color render targets
	ID3D11Texture2D* ShadowMomentsArrayTexture = nullptr;
	ID3D11ShaderResourceView* ShadowMomentsSRV = nullptr;
	TArray<ID3D11RenderTargetView*> ShadowMomentsSliceRTVs;
	ID3D11SamplerState* ShadowLinearSamplerState = nullptr;
};
