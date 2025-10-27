#include "pch.h"
#include "Editor/Public/Camera.h"
#include "Manager/Public/InputManager.h"
#include "Manager/Public/TimeManager.h"
#include "Manager/Public/ConfigManager.h"
#include "Scene/Public/Level/Level.h"

UCamera::UCamera() :
	CameraConstants(FCameraConstants()),
	RelativeLocation(FVector(-15.0f, 0.f, 10.0f)), RelativeRotation(FVector(0, 0, 0)),
	FovY(90.f), Aspect(float(Render::INIT_SCREEN_WIDTH) / Render::INIT_SCREEN_HEIGHT),
	NearZ(0.1f), FarZ(1000.0f), OrthoWidth(90.0f), CameraType(ECameraType::ECT_Perspective)
{
	CurrentMoveSpeed = UConfigManager::GetInstance().GetCameraSensitivity();
}

UCamera::~UCamera()
{
	UConfigManager::GetInstance().SetCameraSensitivity(CurrentMoveSpeed);
}

FVector UCamera::UpdateInput()
{
	if (bOverrideComponent)
	{
		if (!IsValid(OverrideTargetComponent))
		{
			bOverrideComponent = false;
			OverrideTargetComponent = nullptr;
		}
	}

	FVector MovementDelta = FVector::Zero(); // 마우스의 변화량을 반환할 객체
	const UInputManager& Input = UInputManager::GetInstance();

	/**
	 * @brief 마우스 우클릭을 하고 있는 동안 카메라 제어가 가능합니다.
	 */
	if (Input.IsKeyDown(EKeyInput::MouseRight))
	{
		/**
		 * @brief W, A, S, D 는 각각 카메라의 상, 하, 좌, 우 이동을 담당합니다.
		 */
		FVector Direction = FVector::Zero();

		if (Input.IsKeyDown(EKeyInput::A)) { Direction += -RightVector; }
		if (Input.IsKeyDown(EKeyInput::D)) { Direction += RightVector; }
		if (Input.IsKeyDown(EKeyInput::W)) { Direction += ForwardVector; }
		if (Input.IsKeyDown(EKeyInput::S)) { Direction += -ForwardVector; }
		if (Input.IsKeyDown(EKeyInput::Q)) { Direction += -UpVector; }
		if (Input.IsKeyDown(EKeyInput::E)) { Direction += UpVector; }
		if (Direction.LengthSquared() > MATH_EPSILON)
		{
			Direction.Normalize();
		}
		SetLocation(GetLocation() + Direction * CurrentMoveSpeed * DT);
		MovementDelta = Direction * CurrentMoveSpeed * DT;

		SetMoveSpeed(CurrentMoveSpeed + Input.GetMouseWheelDelta() * SPEED_ADJUST_STEP);

		/**
		* @brief 마우스 위치 변화량을 감지하여 카메라의 회전을 담당합니다.
		* 원근 투영 모드를 적용한 카메라만 회전이 가능합니다.
		*/
		if (CameraType == ECameraType::ECT_Perspective)
		{
			const FVector MouseDelta = UInputManager::GetInstance().GetMouseDelta();
			const FVector DeltaRotation = FVector(0, MouseDelta.Y, MouseDelta.X) * KeySensitivityDegPerPixel;
			SetRotation(GetRotation() + DeltaRotation);
			MovementDelta = FVector::Zero(); // 원근 투영 모드는 반환할 필요가 없음
		}

		FVector Rotation = GetRotation();

		// Yaw 래핑(값이 무한히 커지지 않도록)
		if (Rotation.Z > 180.0f) Rotation.Z -= 360.0f;
		if (Rotation.Z < -180.0f) Rotation.Z += 360.0f;

		// Pitch 클램프(짐벌 플립 방지)
		Rotation.Y = std::min(Rotation.Y, 89.0f);
		Rotation.Y = std::max(Rotation.Y, -89.0f);
		SetRotation(Rotation);
	}

	return MovementDelta;
}

void UCamera::Update(const D3D11_VIEWPORT& InViewport)
{
	//UpdateInput();

	const FMatrix RotationMatrix = FMatrix::RotationMatrix(FVector::GetDegreeToRadian(GetRotation()));
	const FVector4 Forward4 = FVector4::ForwardVector() * RotationMatrix;
	const FVector4 WorldUp4 = FVector4::UpVector() * RotationMatrix;
	const FVector WorldUp = { WorldUp4.X, WorldUp4.Y, WorldUp4.Z };

	ForwardVector = FVector(Forward4.X, Forward4.Y, Forward4.Z);
	ForwardVector.Normalize();
	RightVector = WorldUp.Cross(ForwardVector);
	RightVector.Normalize();
	UpVector = ForwardVector.Cross(RightVector);
	UpVector.Normalize();

	// 종횡비 갱신
	if (InViewport.Width > 0.f && InViewport.Height > 0.f)
	{
		SetAspect(InViewport.Width / InViewport.Height);
	}

	switch (CameraType)
	{
	case ECameraType::ECT_Perspective:
		UpdateMatrixByPers();
		break;
	case ECameraType::ECT_Orthographic:
		UpdateMatrixByOrth();
		break;
	}

	if (ULevel* Level = GWorld->GetLevel())
    {
        ViewVolumeCuller.Cull(Level->GetStaticOctree(), Level->GetDynamicPrimitives(), CameraConstants);
    }
}

void UCamera::UpdateMatrixByPers()
{
	/**
	 * @brief 표준 View/Projection 행렬 연산
	 */
	const FVector Location = GetLocation();

	CameraConstants.View = FMatrix::CreateViewFromAxes(Location, RightVector, UpVector, ForwardVector);
	const float RadianFovY = FVector::GetDegreeToRadian(FovY);
	CameraConstants.Projection = FMatrix::CreatePerspectiveFOV(RadianFovY, Aspect, NearZ, FarZ);
	CameraConstants.ViewWorldLocation = Location;
	CameraConstants.NearClip = NearZ;
	CameraConstants.FarClip = FarZ;

	/**
	 * @brief 역행렬(Inverse)을 미리 계산하여 캐시
	 */
	FMatrix R_Inv = FMatrix(RightVector, UpVector, ForwardVector);
	FMatrix T_Inv = FMatrix::TranslationMatrix(Location);
	InverseCameraConstants.View = R_Inv * T_Inv;

	const float F = 1.0f / std::tanf(RadianFovY * 0.5f);
	FMatrix P_Inv = FMatrix::Identity();
	P_Inv.Data[0][0] = Aspect / F;
	P_Inv.Data[1][1] = 1.0f / F;
	P_Inv.Data[2][2] = 0.0f;
	P_Inv.Data[2][3] = -(FarZ - NearZ) / (NearZ * FarZ);
	P_Inv.Data[3][2] = 1.0f;
	P_Inv.Data[3][3] = FarZ / (NearZ * FarZ);
	InverseCameraConstants.Projection = P_Inv;
	InverseCameraConstants.ViewWorldLocation = Location;
	InverseCameraConstants.NearClip = NearZ;
	InverseCameraConstants.FarClip = FarZ;
}

void UCamera::UpdateMatrixByOrth()
{
	/**
	 * @brief 표준 View/Projection 행렬 연산
	 */
	CameraConstants.View = FMatrix::CreateViewFromAxes(RelativeLocation, RightVector, UpVector, ForwardVector);

	const float OrthoHeight = OrthoWidth / Aspect;
	const float Left = -OrthoWidth * 0.5f;
	const float Right = OrthoWidth * 0.5f;
	const float Bottom = -OrthoHeight * 0.5f;
	const float Top = OrthoHeight * 0.5f;

	CameraConstants.Projection = FMatrix::CreateOrthographicOffCenter(Left, Right, Bottom, Top, NearZ, FarZ);
	CameraConstants.ViewWorldLocation = RelativeLocation;
	CameraConstants.NearClip = NearZ;
	CameraConstants.FarClip = FarZ;

	/**
	 * @brief 역행렬(Inverse)을 미리 계산하여 캐시
	 */
	// View Inverse = R * T
	FMatrix R_Inv = FMatrix(RightVector, UpVector, ForwardVector);
	FMatrix T_Inv = FMatrix::TranslationMatrix(RelativeLocation);
	InverseCameraConstants.View = R_Inv * T_Inv;

	FMatrix P_Inv = FMatrix::Identity();
	P_Inv.Data[0][0] = (Right - Left) * 0.5f;  // (r-l)/2
	P_Inv.Data[1][1] = (Top - Bottom) * 0.5f; // (t-b)/2
	P_Inv.Data[2][2] = (FarZ - NearZ);        // (zf-zn)
	P_Inv.Data[3][0] = (Right + Left) * 0.5f;  // (r+l)/2
	P_Inv.Data[3][1] = (Top + Bottom) * 0.5f; // (t+b)/2
	P_Inv.Data[3][2] = NearZ;                 // zn
	P_Inv.Data[3][3] = 1.0f;
	InverseCameraConstants.Projection = P_Inv;
	InverseCameraConstants.ViewWorldLocation = RelativeLocation;
	InverseCameraConstants.NearClip = NearZ;
	InverseCameraConstants.FarClip = FarZ;
}

void UCamera::SetLocation(const FVector& InOtherPosition)
{
	if (bOverrideComponent && OverrideTargetComponent)
	{
		OverrideTargetComponent->SetWorldLocation(InOtherPosition);
	}
	else
	{
		RelativeLocation = InOtherPosition;
	}
}

void UCamera::SetRotation(const FVector& InOtherRotation)
{
	if (bOverrideComponent && OverrideTargetComponent)
	{
		OverrideTargetComponent->SetWorldRotation(InOtherRotation);
	}
	else
	{
		RelativeRotation = InOtherRotation;
	}
}

FRay UCamera::ConvertToWorldRay(float NdcX, float NdcY) const
{
	/* *
	 * @brief 반환할 타입의 객체 선언
	 */
	FRay Ray = {};

	const FCameraConstants& ViewProjMatrix = GetCameraConstantsInverse();

	/* *
	 * @brief NDC 좌표 정보를 행렬로 변환합니다.
	 */
	const FVector4 NdcNear(NdcX, NdcY, 0.0f, 1.0f);
	const FVector4 NdcFar(NdcX, NdcY, 1.0f, 1.0f);

	/* *
	 * @brief Projection 행렬을 View 행렬로 역투영합니다.
	 * Model -> View -> Projection -> NDC
	 */
	const FVector4 ViewNear = MultiplyPointWithMatrix(NdcNear, ViewProjMatrix.Projection);
	const FVector4 ViewFar = MultiplyPointWithMatrix(NdcFar, ViewProjMatrix.Projection);

	/* *
	 * @brief View 행렬을 World 행렬로 역투영합니다.
	 * Model -> View -> Projection -> NDC
	 */
	const FVector4 WorldNear = MultiplyPointWithMatrix(ViewNear, ViewProjMatrix.View);
	const FVector4 WorldFar = MultiplyPointWithMatrix(ViewFar, ViewProjMatrix.View);

	/* *
	 * @brief 카메라의 월드 좌표를 추출합니다.
	 * Row-major 기준, 마지막 행 벡터는 위치 정보를 가지고 있음
	 */
	const FVector4 CameraPosition(
		ViewProjMatrix.View.Data[3][0],
		ViewProjMatrix.View.Data[3][1],
		ViewProjMatrix.View.Data[3][2],
		ViewProjMatrix.View.Data[3][3]);

	if (CameraType == ECameraType::ECT_Perspective)
	{
		FVector4 DirectionVector = WorldFar - CameraPosition;
		DirectionVector.Normalize();

		Ray.Origin = CameraPosition;
		Ray.Direction = DirectionVector;
	}
	else if (CameraType == ECameraType::ECT_Orthographic)
	{
		FVector4 DirectionVector = WorldFar - WorldNear;
		DirectionVector.Normalize();

		Ray.Origin = WorldNear;
		Ray.Direction = DirectionVector;
	}

	return Ray;
}

FVector UCamera::CalculatePlaneNormal(const FVector& Axis)
{
	return FVector(Axis.X, Axis.Y, Axis.Z).Cross(ForwardVector);
}

const FVector& UCamera::GetLocation()
{
	if (bOverrideComponent && OverrideTargetComponent)
	{
		return OverrideTargetComponent->GetWorldLocation();
	}
	else
	{
		return RelativeLocation;
	}
}

const FVector& UCamera::GetRotation()
{
	if (bOverrideComponent && OverrideTargetComponent)
	{
		FVector Rotation = OverrideTargetComponent->GetWorldRotation();
		return FVector(Rotation.X, -Rotation.Y, -Rotation.Z);
	}
	else
	{
		return RelativeRotation;
	}
}

void UCamera::AttachToComponent(USceneComponent* InTargetComponent)
{
	if (InTargetComponent != nullptr)
	{
		OverrideTargetComponent = InTargetComponent;
		bOverrideComponent = true;
	}
}

void UCamera::DetachFromComponent()
{
	OverrideTargetComponent = nullptr;
	bOverrideComponent = false;
}
