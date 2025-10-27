#pragma once
#include "Core/Public/Object/Object.h"
#include "Renderer/Public/Optimization/ViewVolumeCuller.h"

class UConfigManager;

enum class ECameraType
{
	ECT_Orthographic,
	ECT_Perspective,
};

class UCamera : public UObject
{
public:
	// Camera Speed Constants
	static constexpr float MIN_SPEED = 1.0f;
	static constexpr float MAX_SPEED = 70.0f;
	static constexpr float DEFAULT_SPEED = 20.0f;
	static constexpr float SPEED_ADJUST_STEP = 1.0f;

	UCamera();
	~UCamera() override;

	/* *
	* @brief Update 관련 함수
	* UpdateInput 함수는 사용자 입력으로 비롯된 변화의 갱신를 담당합니다.
	* Update, UpdateMatrix 함수들은 카메라의 변환 행렬의 갱신을 담당합니다.
	*/
	FVector UpdateInput();
	void Update(const D3D11_VIEWPORT& InViewport);
	void UpdateMatrixByPers();
	void UpdateMatrixByOrth();

	/**
	 * @brief Setter
	 */
	void SetLocation(const FVector& InOtherPosition);
	void SetRotation(const FVector& InOtherRotation);
	void SetFovY(const float InOtherFovY) { FovY = InOtherFovY; }
	void SetAspect(const float InOtherAspect) { Aspect = InOtherAspect; }
	void SetNearZ(const float InOtherNearZ) { NearZ = InOtherNearZ; }
	void SetFarZ(const float InOtherFarZ) { FarZ = InOtherFarZ; }
	void SetOrthoWidth(const float InOrthoWidth) { OrthoWidth = InOrthoWidth; }
	void SetCameraType(const ECameraType InCameraType) { CameraType = InCameraType; }

	/**
	 * @brief Getter
	 */
	const FCameraConstants& GetCameraConstants() const { return CameraConstants; }
	const FCameraConstants& GetCameraConstantsInverse() const { return InverseCameraConstants; }

	FRay ConvertToWorldRay(float NdcX, float NdcY) const;

	FVector CalculatePlaneNormal(const FVector& Axis);
	const FVector& GetLocation();
	const FVector& GetRotation();
	const FVector& GetForward() const { return ForwardVector; }
	const FVector& GetUp() const { return UpVector; }
	const FVector& GetRight() const { return RightVector; }
	float GetFovY() const { return FovY; }
	float GetAspect() const { return Aspect; }
	float GetNearZ() const { return NearZ; }
	float GetFarZ() const { return FarZ; }
	float GetOrthoWidth() const { return OrthoWidth; }
	ECameraType GetCameraType() const { return CameraType; }
	ViewVolumeCuller& GetViewVolumeCuller() { return ViewVolumeCuller; }

	// Camera Movement Speed Control
	float GetMoveSpeed() const { return CurrentMoveSpeed; }
	void SetMoveSpeed(float InSpeed) { CurrentMoveSpeed = clamp(InSpeed, MIN_SPEED, MAX_SPEED); }

	/* *
	 * @brief 행렬 형태로 저장된 좌표와 변환 행렬과의 연산한 결과를 반환합니다.
	 */
	inline FVector4 MultiplyPointWithMatrix(const FVector4& Point, const FMatrix& Matrix) const
	{
		FVector4 Result = Point * Matrix;
		/* *
		 * @brief 좌표가 왜곡된 공간에 남는 것을 방지합니다.
		 */
		if (Result.W != 0.f) { Result *= (1.f / Result.W); }

		return Result;
	}

private:
	FCameraConstants CameraConstants = {};
	FCameraConstants InverseCameraConstants = {};
	FVector RelativeLocation = {};
	FVector RelativeRotation = {};
	FVector ForwardVector = { 1,0,0 };
	FVector UpVector = {0,0,1};
	FVector RightVector = {0,1,0};
	float FovY = {};
	float Aspect = {};
	float NearZ = {};
	float FarZ = {};
	float OrthoWidth = {};
	ECameraType CameraType = {};

	// 절두체 컬링을 이용한 최적화
	ViewVolumeCuller ViewVolumeCuller;

	// Dynamic Movement Speed
	float CurrentMoveSpeed = DEFAULT_SPEED;

// Camera Override Section
public:
	/**
	 * @brief 이 카메라를 특정 컴포넌트에 파일럿
	 * @param InTargetComponent 파일럿할 대상 컴포넌트
	 */
	void AttachToComponent(USceneComponent* InTargetComponent);
	/**
	 * @brief 컴포넌트로부터 파일럿 해제
	 */
	void DetachFromComponent();
	/**
	 * @brief 현재 파일럿(Override) 중인지 확인
	 */
	bool IsOverridingWithComponent(const USceneComponent* Component) const { return bOverrideComponent && OverrideTargetComponent == Component; }

private:
	/**
	 * @brief true일 경우, 카메라가 외부 컴포넌트의 트랜스폼 따라감
	 */
	bool bOverrideComponent = false;

	/**
	 * @brief 카메라가 따라갈(파일럿할) 대상 컴포넌트
	 * TODO - 대상이 파괴되었을 때를 대비해야함
	*/
	USceneComponent* OverrideTargetComponent = nullptr;
};
