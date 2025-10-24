#pragma once

/**
 * @class FShadowMap
 * @brief 단일 광원의 섀도우 맵 리소스를 관리하는 래퍼 클래스
 * 텍스처, DSV(쓰기용), SRV(읽기용), 뷰포트, 비교 샘플러를 포함
 */
class FShadowMap
{
public:
	FShadowMap();
	~FShadowMap();

	FShadowMap(const FShadowMap&) = delete;
	FShadowMap& operator=(const FShadowMap&) = delete;

	/**
	 * @brief 지정된 해상도로 섀도우 맵 리소스를 생성
	 * @param InWidth - 섀도우 맵 너비
	 * @param InHeight - 섀도우 맵 높이
	 * @return 성공 시 true, 실패 시 false
	 */
	bool Initialize(uint32 InWidth, uint32 InHeight);

	/**
	 * @brief 섀도우 맵의 해상도를 변경합니다.
	 * 기존 리소스를 해제하고 새 해상도로 다시 생성합니다.
	 * @return 성공 시 true, 실패 시 false
	 */
	bool Resize(uint32 NewWidth, uint32 NewHeight);

	/**
	 * @brief 모든 D3D11 리소스를 해제
	 */
	void Release();

	// --- Getters ---
	ID3D11DepthStencilView* GetDSV() const { return ShadowMapDSV; }
	ID3D11ShaderResourceView* GetSRV() const { return ShadowMapSRV; }
	ID3D11SamplerState* GetSampler() const { return ComparisonSampler; }
	const D3D11_VIEWPORT& GetViewport() const { return Viewport; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

private:
	uint32 Width = 0;
	uint32 Height = 0;

	// 섀도우 맵 렌더링에 사용될 뷰포트
	D3D11_VIEWPORT Viewport = {};
	// 깊이 정보를 저장할 실제 텍스처 리소스
	ID3D11Texture2D* ShadowMapTexture = nullptr;
	// 섀도우 패스(쓰기)용 뎁스 스텐실 뷰 (DSV)
	ID3D11DepthStencilView* ShadowMapDSV = nullptr;
	// 메인 패스(읽기)용 셰이더 리소스 뷰 (SRV)
	ID3D11ShaderResourceView* ShadowMapSRV = nullptr;
	// PCF(Percentage-Closer Filtering)를 위한 비교 샘플러
	ID3D11SamplerState* ComparisonSampler = nullptr;
};
