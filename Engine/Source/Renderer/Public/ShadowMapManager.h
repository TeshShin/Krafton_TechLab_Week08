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
    /**
     * @param InMaxSpotShadows - 최대 스포트라이트 섀도우 개수 (슬라이스 수)
     * @param InSpotResolution - 스포트라이트 섀도우 해상도
     * @param InMaxPointShadowCubes - 최대 포인트라이트 섀도우 개수 (큐브 수)
     * @param InPointResolution - 포인트라이트 섀도우 해상도
     * @param InDirLightResolution - Directional Light 섀도우 해상도
     */
    void Initialize(uint32 InMaxSpotShadows, uint32 InSpotResolution, uint32 InMaxPointShadowCubes, uint32 InPointResolution,
    	uint32 InDirLightResolution);
	void InitializeSpotShadows(uint32 InMaxSpotShadows, uint32 InSpotResolution);
	void InitializePointShadows(uint32 InMaxPointShadowCubes, uint32 InPointResolution);
	void InitializeDirectionalShadow(uint32 InResolution);

    void ReleaseSpotShadows();
    void ReleasePointShadows();
	void ReleaseDirectionalShadow();
    void Release();

    /**
     * 다음 프레임을 위해 현재 할당 인덱스를 초기화
     * (리소스 해제는 아님)
     */
    void ClearShadowMaps();

    /**
     * 라이트 컴포넌트의 타입에 맞춰 적절한 섀도우 맵 인덱스를 할당
     */
    void AllocateShadowMap(class ULightComponentBase* Light);

    /**
     * 공용 섀도우 샘플러 반환
     */
    ID3D11SamplerState* GetSamplerState() const { return ShadowMapSamplerState; }

	uint32 GetResolution(class ULightComponentBase* Light) const;

    // --- Spot Light Getters ---
    ID3D11ShaderResourceView* GetSpotLightSRV() const { return SpotShadowMapArraySRV; }
    ID3D11DepthStencilView* GetSpotLightDSV(uint32 SpotShadowIdx) const;
    uint32 GetSpotResolution() const { return SpotResolution; }
	uint32 GetMaxSpotShadows() const { return MaxSpotShadows; }

    // --- Point Light Getters ---
	void GetPointShadowRTVs(class ULightComponentBase* Light, TArray<ID3D11RenderTargetView*>& OutRTVs) const;
	ID3D11DepthStencilView* GetPointShadowDepthDSV() const { return PointShadowDepthDSV; }
    ID3D11ShaderResourceView* GetPointLightSRV() const { return PointShadowCubeArraySRV; }
    uint32 GetPointResolution() const { return PointResolution; }
	uint32 GetMaxPointShadowCubes() const { return MaxPointShadowCubes; }

	// --- Directional Light Getters ---
	ID3D11ShaderResourceView* GetDirectionalLightSRV() const { return DirLightShadowSRV; }
	ID3D11DepthStencilView* GetDirectionalLightDSV() const { return DirLightShadowDSV; }
	uint32 GetDirectionalResolution() const { return DirLightResolution; }

private:
    // D3D11 핵심 오브젝트
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;

    // 공용 샘플러
    ID3D11SamplerState* ShadowMapSamplerState = nullptr;

    // --- SpotLight 리소스 풀 ---
    uint32 MaxSpotShadows = 0;
    uint32 SpotResolution = 0;
    uint32 CurrentSpotShadowIdx = 0;

    ID3D11Texture2D* SpotShadowMapArrayTexture = nullptr;
    ID3D11ShaderResourceView* SpotShadowMapArraySRV = nullptr; // D3D11_SRV_DIMENSION_TEXTURE2DARRAY
    TArray<ID3D11DepthStencilView*> SpotShadowMapSliceDSVs;

    // --- PointLight 리소스 풀 ---
    uint32 MaxPointShadowCubes = 0;
    uint32 PointResolution = 0;
    uint32 CurrentPointCubeIdx = 0; // 큐브 기준 인덱스

    ID3D11Texture2D* PointShadowCubeArrayTexture = nullptr; // D3D11_RESOURCE_MISC_TEXTURECUBE 플래그 포함
    ID3D11ShaderResourceView* PointShadowCubeArraySRV = nullptr; // D3D11_SRV_DIMENSION_TEXTURECUBEARRAY
    TArray<ID3D11RenderTargetView*> PointShadowCubeSliceRTVs; // (크기: MaxPointShadowCubes * 6)
	ID3D11Texture2D* PointShadowDepthTexture = nullptr;
	ID3D11DepthStencilView* PointShadowDepthDSV = nullptr;

	// --- Directional Light 리소스 ---
	bool bIsDirShadowAllocated = false;
	uint32 DirLightResolution = 0;
	ID3D11Texture2D* DirLightShadowTexture = nullptr;
	ID3D11ShaderResourceView* DirLightShadowSRV = nullptr;
	ID3D11DepthStencilView* DirLightShadowDSV = nullptr;

	// Debug Section
public:
	void InitializeForDebug();
	ID3D11ShaderResourceView* GetSpotSRVForImGuiDebug(uint32 SpotIndex);
	ID3D11ShaderResourceView* GetPointSRVForImGuiDebug(uint32 CubeIndex, uint32 FaceIndex); // FaceIndex: 0~5
	ID3D11ShaderResourceView* GetDirectionalSRVForImGuiDebug();

private:
    // --- 디버그용 리소스 ---
    ID3D11Texture2D* ImGuiDebugTexture_Spot = nullptr;
    ID3D11ShaderResourceView* ImGuiDebugSRV_Spot = nullptr;

    ID3D11Texture2D* ImGuiDebugTexture_Point = nullptr;
    ID3D11ShaderResourceView* ImGuiDebugSRV_Point = nullptr;

	ID3D11Texture2D* ImGuiDebugTexture_Dir = nullptr;
	ID3D11ShaderResourceView* ImGuiDebugSRV_Dir = nullptr;
};
