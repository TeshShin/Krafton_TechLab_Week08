#pragma once

/**
 * @brief 섀도우 맵 매니저의 VRAM 및 할당 상태를 추적하는 스탯 구조체
 */
struct FShadowStatData
{
	// --- VRAM (Bytes) ---
	// 정적(Static) VRAM - 초기화 시 한 번만 계산
	uint64 VRAM_Directional_Depth = 0;
	uint64 VRAM_Directional_Moments = 0;
	uint64 VRAM_Spot_Depth = 0;
	uint64 VRAM_Spot_Moments = 0;
	uint64 VRAM_Point_Moments = 0;     // 포인트 라이트는 Moment RTV를 사용
	uint64 VRAM_Point_Pass_DSV = 0;  // 포인트 라이트 렌더링 패스용 DSV
	uint64 VRAM_Total = 0;

	// --- Config (초기화 설정값) ---
	uint32 Config_DirResolution = 0;
	uint32 Config_SpotResolution = 0;
	uint32 Config_MaxSpotShadows = 0;
	uint32 Config_PointResolution = 0;
	uint32 Config_MaxPointShadowCubes = 0;

	// --- Dynamic Stats (매 프레임 업데이트) ---
	uint32 Allocated_Directional = 0;
	uint32 Allocated_Spot = 0;
	uint32 Allocated_PointCubes = 0;
};

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
	ID3D11SamplerState* GetMomentSampler() const { return MomentSamplerState; }

	uint32 GetResolution(class ULightComponentBase* Light) const;

    // --- Spot Light Getters ---
    ID3D11ShaderResourceView* GetSpotLightSRV() const { return SpotShadowMapArraySRV; }
    ID3D11DepthStencilView* GetSpotLightDSV(uint32 SpotShadowIdx) const;
    uint32 GetSpotResolution() const { return SpotResolution; }
	uint32 GetMaxSpotShadows() const { return MaxSpotShadows; }

	ID3D11ShaderResourceView* GetSpotMomentsSRV() const { return SpotShadowMomentsSRV; }
	ID3D11RenderTargetView* GetSpotMomentsRTV(uint32 SpotShadowIdx) const { return SpotShadowMomentsSliceRTVs[SpotShadowIdx]; }

    // --- Point Light Getters ---
	void GetPointShadowRTVs(class ULightComponentBase* Light, TArray<ID3D11RenderTargetView*>& OutRTVs) const;
	ID3D11DepthStencilView* GetPointShadowDepthDSV() const { return PointShadowDepthDSV; }
    ID3D11ShaderResourceView* GetPointLightSRV() const { return PointShadowCubeArraySRV; }
    ID3D11ShaderResourceView* GetPointLightSRV_PCF() const { return PointShadowCubeArraySRV_PCF; }
    uint32 GetPointResolution() const { return PointResolution; }
	uint32 GetMaxPointShadowCubes() const { return MaxPointShadowCubes; }

	// --- Directional Light Getters ---
	ID3D11ShaderResourceView* GetDirectionalLightSRV() const { return DirShadowSRV; }
	ID3D11DepthStencilView* GetDirectionalLightDSV() const { return DirShadowDSV; }
	ID3D11DepthStencilView* GetDirectionalLightDSV(uint32 CascadeIdx) const;

	uint32 GetDirectionalResolution() const { return DirResolution; }

	ID3D11ShaderResourceView* GetDirectionalMomentSRV() const { return DirShadowMomentSRV; }
	ID3D11RenderTargetView* GetDirectionalMomentRTV() const { return DirShadowMomentRTV; }
	ID3D11RenderTargetView* GetDirectionalMomentRTV(uint32 CascadeIdx) const;
	uint32 GetDirectionalMaxNumCascades() const { return DirLightMaxNumCascades; }
private:
    // D3D11 핵심 오브젝트
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;

    // 공용 샘플러
    ID3D11SamplerState* ShadowMapSamplerState = nullptr;

    // --- SpotLight 리소스 ---
    uint32 MaxSpotShadows = 0;
    uint32 SpotResolution = 0;
    uint32 CurrentSpotShadowIdx = 0;

    ID3D11Texture2D* SpotShadowMapArrayTexture = nullptr;
    ID3D11ShaderResourceView* SpotShadowMapArraySRV = nullptr; // D3D11_SRV_DIMENSION_TEXTURE2DARRAY
    TArray<ID3D11DepthStencilView*> SpotShadowMapSliceDSVs;

	ID3D11Texture2D* SpotShadowMomentsArrayTexture = nullptr;
	ID3D11ShaderResourceView* SpotShadowMomentsSRV = nullptr;
	TArray<ID3D11RenderTargetView*> SpotShadowMomentsSliceRTVs;

    // --- PointLight 리소스 ---
    uint32 MaxPointShadowCubes = 0;
    uint32 PointResolution = 0;
    uint32 CurrentPointCubeIdx = 0; // 큐브 기준 인덱스

    ID3D11Texture2D* PointShadowCubeArrayTexture = nullptr; // D3D11_RESOURCE_MISC_TEXTURECUBE 플래그 포함
    ID3D11ShaderResourceView* PointShadowCubeArraySRV = nullptr; // D3D11_SRV_DIMENSION_TEXTURECUBEARRAY
    ID3D11ShaderResourceView* PointShadowCubeArraySRV_PCF = nullptr;
    TArray<ID3D11RenderTargetView*> PointShadowCubeSliceRTVs; // (크기: MaxPointShadowCubes * 6)
	ID3D11Texture2D* PointShadowDepthTexture = nullptr;
	ID3D11DepthStencilView* PointShadowDepthDSV = nullptr;

	// --- DirectionalLight 리소스 ---	
	bool bIsDirShadowAllocated = false;
	uint32 DirResolution = 0;
	ID3D11Texture2D* DirShadowTexture = nullptr;
	ID3D11ShaderResourceView* DirShadowSRV = nullptr;
	ID3D11DepthStencilView* DirShadowDSV = nullptr;

	ID3D11Texture2D* DirShadowMomentTexture = nullptr;
	ID3D11ShaderResourceView* DirShadowMomentSRV = nullptr;
	ID3D11RenderTargetView* DirShadowMomentRTV;
	TArray<ID3D11RenderTargetView*> DirShadowMomentRTVs;

	TArray<ID3D11Texture2D*> ImGuiDebugTextures_Dir; 
	TArray<ID3D11ShaderResourceView*> ImGuiDebugSRVs_Dir;

	uint32 DirLightMaxNumCascades = 12; 
	TArray<ID3D11DepthStencilView*> DirLightCascadeDSVs; 

	// --- VSM Moments (RG32F) 리소스 ---
	ID3D11SamplerState* MomentSamplerState = nullptr;

// Debug Section
public:
	void InitializeForDebug();
	ID3D11ShaderResourceView* GetSpotSRVForImGuiDebug(uint32 SpotIndex);
	void UpdatePointShadowDebugTextures(uint32 CubeIndex);
	ID3D11ShaderResourceView* GetPointSRVForImGuiDebug(uint32 CubeIndex, uint32 FaceIndex); // FaceIndex: 0~5
	ID3D11ShaderResourceView* GetDirectionalSRVForImGuiDebug();
	ID3D11ShaderResourceView* GetDirectionalSRVForImGuiDebug(uint32 CascadeIdx);

private:
    // --- 디버그용 리소스 ---
    ID3D11Texture2D* ImGuiDebugTexture_Spot = nullptr;
    ID3D11ShaderResourceView* ImGuiDebugSRV_Spot = nullptr;

    ID3D11Texture2D* ImGuiDebugTextures_Point[6] = { nullptr };
    ID3D11ShaderResourceView* ImGuiDebugSRVs_Point[6] = { nullptr };

	ID3D11Texture2D* ImGuiDebugTexture_Dir = nullptr;
	ID3D11ShaderResourceView* ImGuiDebugSRV_Dir =  nullptr;

// Stat Section
public:
	const FShadowStatData& GetShadowStats() const;

private:
	void UpdateTotalVRAMStats();

	mutable FShadowStatData StatData;
};
