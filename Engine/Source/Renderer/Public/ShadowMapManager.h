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

	uint32 GetResolution() const { return Resolution; }

private:
	uint32 MaxShadows;
	uint32 Resolution;
	uint32 CurrentShadowIdx = 0;

	ID3D11Texture2D* ShadowMapArrayTexture;
	ID3D11ShaderResourceView* ShadowMapArraySRV;
	TArray<ID3D11DepthStencilView*> ShadowMapSliceDSVs;
	ID3D11SamplerState* ShadowMapSamplerState;

// Debug Section
public:
	void InitializeForDebug(ID3D11Device* Device);
	ID3D11ShaderResourceView* GetSRVForImGuiDebug(uint32 ShadowMapIdx);

private:
	ID3D11Texture2D* ImGuiDebugTexture;
	ID3D11ShaderResourceView* ImGuiDebugSRV;
};
