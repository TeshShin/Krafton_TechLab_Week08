#pragma once
#include "Core/Public/Types.h"
#include "Editor/Public/Camera.h"
#include "Renderer/Public/Renderer.h"

class FPSMBuilder
{
public:
	// 스팟라이트: 라이트 위치/기저 + FOV/범위 입력
	static bool BuildSpotLightPSM(
		const FVector& LightPosition,
		const FVector& LightRight,
		const FVector& LightUp,
		const FVector& LightForward,
		float OuterConeAngleDegrees,
		float AttenuationRadius,
		FMatrix& OutLightViewProjection);

	// 디렉셔널: 라이트 방향만으로 라이트 뷰 구성(오리엔테이션), 카메라 절두체 기반 크롭
	static bool BuildDirectionalLightPSM(
		const FVector& LightDirection,
		FMatrix& OutLightViewProjection);

	// 장면 위젯 포커스 이슈 대응: 활성 카메라 → 원근 카메라 → 첫 카메라 순 폴백
	static UCamera* ResolveActiveOrFallbackCamera();

	// 카메라 NDC 코너 (LH, z: 0..1)
	static void BuildCameraNDCCorners(FVector4 OutCorners[8]);

	// 카메라 역행렬로 NDC 코너를 월드 좌표로
	static void BuildCameraFrustumCornersWorld(const UCamera* Camera, FVector4 OutWorld[8]);

	// 행렬 곱 + 원근분할
	static FVector4 MultiplyAndDoPerspectiveDivide(const FVector4& Point, const FMatrix& Matrix);

	// 오쏘 그래픽스 프로젝션(워프 후 XY 바운드 크롭, D3D LH, z:0..1)
	static FMatrix BuildOrthographicFromBounds(float Left, float Right, float Bottom, float Top, float NearZ, float
		FarZ);

	// PSM 워프 행렬(w' = z 계열), Lambda 0..1
	static FMatrix BuildPSMWarp(float NearZ, float FarZ, float Lambda);

	// 보조(클리핑/중복 방지)
	static float LerpFloat(float A, float B, float T);

};
