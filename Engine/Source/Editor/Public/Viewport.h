#pragma once
#include "Editor/Public/ViewportClient.h"

class URenderer;

class FViewport
{
public:
	FViewport() = default;
	~FViewport();

	/**
	 * @brief 현재 모든 뷰포트의 카메라 상태를 UConfigManager에 동기화하는 새로운 함수
	 */
	void UpdateCameraSettingsToConfig();

	/**
	* @brief ViewportClient가 보유한 Viewport들의 정보를 갱신합니다.
	* 이 함수는 현재 2x2 기준으로 작성되어 있습니다.
	*/
	void InitializeLayout(const D3D11_VIEWPORT& InViewport);

	/**
	* @brief 모든 뷰포트의 카메라 데이터를 ConfigManager로부터 가져와 적용합니다.
	*/
	void ApplyAllCameraDataToViewportClients();

	/**
	* @brief 현재 활성화된 FViewport를 갱신합니다.
	*/
	void UpdateActiveViewportClient(const FVector& InMousePosition);

	/**
	* @brief 모든 뷰포트의 카메라를 업데이트합니다.
	*/
	void UpdateAllViewportClientCameras();

	/**
	 * @brief 직교 카메라의 이동량을 받아 포커스 포인트를 갱신하고,
	 * 활성 뷰포트를 제외한 나머지 직교 카메라들을 업데이트합니다.
	 */
	void UpdateOrthoFocusPointByDelta(const FVector& InDelta);

	bool IsSingleViewportMode() const;

	/**
	* @brief 현재 활성화된 뷰포트를 반환합니다.
	* @return 활성 뷰포트의 포인터. 없으면 nullptr.
	*/
	FViewportClient* GetActiveViewportClient() const { return ActiveViewportClient; }

	/**
	* @brief 활성 뷰포트의 카메라를 반환하는 헬퍼 함수.
	* @return 활성 카메라의 포인터. 없으면 nullptr.
	*/
	UCamera* GetActiveCamera() const { return ActiveViewportClient ? &ActiveViewportClient->Camera : nullptr; }
	UCamera* GetLastActiveCamera() const { return LastActiveCamera; }

	TArray<FViewportClient>& GetViewports() { return ViewportClients; }

	void SetFocusPoint(const FVector& NewFocusPoint);

private:
	TArray<FViewportClient> ViewportClients = {};
	TArray<ViewVolumeCuller> ViewVolumeCullers{4, ViewVolumeCuller()};
	FViewportClient* ActiveViewportClient = nullptr;
	UCamera* LastActiveCamera = nullptr;
	FVector FocusPoint = { 0.0f, 0.0f, 0.0f }; 	// 직교 투영 카메라가 공유하는 좌표

// Camera Override Section
public:
	/**
	 * @brief 특정 컴포넌트를 활성 뷰포트 카메라로 파일럿하기 시작
	 */
	void StartPiloting(USceneComponent* InComponentToPilot);

	/**
	 * @brief 현재 진행 중인 모든 파일럿 중지
	 */
	void StopPiloting();

	/**
	 * @brief 현재 이 컴포넌트가 파일럿 대상인지 확인
	 */
	bool IsPilotingComponent(USceneComponent* InComponent) const;

private:
	// "어떤 카메라가"
	UCamera* PilotingCamera = nullptr;

	// "어떤 컴포넌트를"
	USceneComponent* PilotedComponent = nullptr;
};

