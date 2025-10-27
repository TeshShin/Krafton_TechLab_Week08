#include "pch.h"
#include "Renderer/Public/Shadow/PSMBuilder.h"
#include "Editor/Public/Viewport.h"
static float Clamp01(float V) { return V < 0.0f ? 0.0f : (V > 1.0f ? 1.0f : V); }

void FPSMBuilder::BuildCameraNDCCorners(FVector4 OutCorners[8])
{
	OutCorners[0] = FVector4(-1.0f, -1.0f, 0.0f, 1.0f);
	OutCorners[1] = FVector4(1.0f, -1.0f, 0.0f, 1.0f);
	OutCorners[2] = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
	OutCorners[3] = FVector4(-1.0f, 1.0f, 0.0f, 1.0f);
	OutCorners[4] = FVector4(-1.0f, -1.0f, 1.0f, 1.0f);
	OutCorners[5] = FVector4(1.0f, -1.0f, 1.0f, 1.0f);
	OutCorners[6] = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	OutCorners[7] = FVector4(-1.0f, 1.0f, 1.0f, 1.0f);
}

FVector4 FPSMBuilder::MultiplyAndDoPerspectiveDivide(const FVector4& Point, const FMatrix& Matrix)
{
	FVector4 Result = FMatrix::VectorMultiply(Point, Matrix);
	if (Result.W != 0.0f)
	{
		Result.X /= Result.W;
		Result.Y /= Result.W;
		Result.Z /= Result.W;
		Result.W = 1.0f;
	}
	return Result;
}

void FPSMBuilder::BuildCameraFrustumCornersWorld(const UCamera* Camera, FVector4 OutWorld[8])
{
	FCameraConstants Inv = Camera->GetFViewProjConstantsInverse();
	FVector4 Ndc[8];
	BuildCameraNDCCorners(Ndc);

	for (int32 i = 0; i < 8; ++i)
	{
		FVector4 View = MultiplyAndDoPerspectiveDivide(Ndc[i], Inv.Projection);
		OutWorld[i] = MultiplyAndDoPerspectiveDivide(View, Inv.View);
	}

}

FMatrix FPSMBuilder::BuildOrthographicFromBounds(float Left, float Right, float Bottom, float Top, float NearZ, float
	FarZ)
{
	FMatrix P = FMatrix::Identity();
	P.Data[0][0] = 2.0f / (Right - Left);
	P.Data[1][1] = 2.0f / (Top - Bottom);
	P.Data[2][2] = 1.0f / (FarZ - NearZ);
	P.Data[3][0] = -(Right + Left) / (Right - Left);
	P.Data[3][1] = -(Top + Bottom) / (Top - Bottom);
	P.Data[3][2] = -NearZ / (FarZ - NearZ);
	P.Data[3][3] = 1.0f;
	return P;
}

FMatrix FPSMBuilder::BuildPSMWarp(float NearZ, float FarZ, float Lambda)
{
	NearZ = std::max(0.01f, NearZ);
	FarZ = std::max(FarZ, NearZ + 0.01f);

	const float Den = (FarZ - NearZ);
	const float A = FarZ / Den;
	const float B = -NearZ * FarZ / Den;

	FMatrix Full = FMatrix::Identity();
	Full.Data[2][2] = A;
	Full.Data[2][3] = 1.0f;
	Full.Data[3][2] = B;
	Full.Data[3][3] = 0.0f;

	if (Lambda <= 0.0f) { return FMatrix::Identity(); }
	if (Lambda >= 1.0f) { return Full; }

	FMatrix Lerp = FMatrix::Identity();
	Lerp.Data[2][2] = 1.0f + Lambda * (Full.Data[2][2] - 1.0f);
	Lerp.Data[2][3] = 0.0f + Lambda * (Full.Data[2][3] - 0.0f);
	Lerp.Data[3][2] = 0.0f + Lambda * (Full.Data[3][2] - 0.0f);
	Lerp.Data[3][3] = 1.0f + Lambda * (Full.Data[3][3] - 1.0f);
	return Lerp;

}

float FPSMBuilder::LerpFloat(float A, float B, float T)
{
	T = Clamp01(T);
	return A + (B - A) * T;
}

UCamera* FPSMBuilder::ResolveActiveOrFallbackCamera()
{
	UCamera* Active = nullptr;
	FViewport* Vp = URenderer::GetInstance().GetViewportClient();
	if (!Vp) { return nullptr; }

	Active = Vp->GetActiveCamera();
	if (Active) { return Active; }

	TArray<FViewportClient>& Clients = Vp->GetViewports();
	for (FViewportClient& Client : Clients)
	{
		if (Client.GetCameraType() == EViewportCameraType::Perspective)
		{
			return &Client.Camera;
		}
	}
	if (Clients.size() > 0) { return &Clients[0].Camera; }
	return nullptr;

}

bool FPSMBuilder::BuildSpotLightPSM(
	const FVector& LightPosition,
	const FVector& LightRight,
	const FVector& LightUp,
	const FVector& LightForward,
	float OuterConeAngleDegrees,
	float AttenuationRadius,
	FMatrix& OutLightViewProjection)
{
	FMatrix T = FMatrix::TranslationMatrixInverse(LightPosition);
	FMatrix R = FMatrix(LightRight, LightUp, LightForward).Transpose();
	FMatrix LightView = T * R;

	UCamera* Camera = ResolveActiveOrFallbackCamera();
	if (!Camera) { return false; }

	FVector4 FrustumWorld[8];
	BuildCameraFrustumCornersWorld(Camera, FrustumWorld);

	FVector4 InLightView[8];
	for (int32 i = 0; i < 8; ++i)
	{
		InLightView[i] = FMatrix::VectorMultiply(FrustumWorld[i], LightView);
	}

	float MinZ = FLT_MAX, MaxZ = -FLT_MAX;
	for (int32 i = 0; i < 8; ++i)
	{
		MinZ = std::min(MinZ, InLightView[i].Z);
		MaxZ = std::max(MaxZ, InLightView[i].Z);
	}
	const float Eps = 0.05f;
	float WarpNear = std::max(Eps, MinZ);
	float WarpFar = std::max(WarpNear + Eps, std::min(MaxZ + Eps, AttenuationRadius));

	const float Aspect = 1.0f;
	const float FovRad = OuterConeAngleDegrees * 2.0f * ToRad;
	const float DotAbs = std::abs(Camera->GetForward().Dot(LightForward));
	const float Lambda = LerpFloat(0.15f, 0.85f, DotAbs);

	FMatrix Warp = BuildPSMWarp(WarpNear, WarpFar, Lambda);
	FMatrix Persp = FMatrix::CreatePerspectiveFOV(FovRad, Aspect, WarpNear, WarpFar);

	float MinX = FLT_MAX, MinY = FLT_MAX, MinZW = FLT_MAX;
	float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZW = -FLT_MAX;

	for (int32 i = 0; i < 8; ++i)
	{
		FVector4 Clip = FMatrix::VectorMultiply(InLightView[i], Warp);
		Clip = FMatrix::VectorMultiply(Clip, Persp);
		if (Clip.W != 0.0f)
		{
			const float X = Clip.X / Clip.W;
			const float Y = Clip.Y / Clip.W;
			const float Z = Clip.Z / Clip.W;
			MinX = std::min(MinX, X);  MaxX = std::max(MaxX, X);
			MinY = std::min(MinY, Y);  MaxY = std::max(MaxY, Y);
			MinZW = std::min(MinZW, Z); MaxZW = std::max(MaxZW, Z);
		}
	}

	const float CropEps = 1e-3f;
	FMatrix Crop = BuildOrthographicFromBounds(MinX - CropEps, MaxX + CropEps, MinY - CropEps, MaxY + CropEps, MinZW,
		MaxZW + CropEps);

	OutLightViewProjection = LightView * Warp * Persp * Crop;
	return true;

}

bool FPSMBuilder::BuildDirectionalLightPSM(
	const FVector& LightDirection,
	FMatrix& OutLightViewProjection)
{
	// 라이트 뷰(오리엔테이션)
	FVector Forward = LightDirection;
	Forward.Normalize();

	FVector WorldUp(0.0f, 0.0f, 1.0f);
	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f);
	}
	FVector Right = WorldUp.Cross(Forward);
	Right.Normalize();
	FVector Up = Forward.Cross(Right);
	Up.Normalize();

	FMatrix R = FMatrix(Right, Up, Forward).Transpose();
	FMatrix LightView = R; // 방향광: 위치 개념 무시(오리엔테이션만)

	UCamera* Camera = ResolveActiveOrFallbackCamera();
	if (!Camera) { return false; }

	FVector4 FrustumWorld[8];
	BuildCameraFrustumCornersWorld(Camera, FrustumWorld);

	FVector4 InLightView[8];
	for (int32 i = 0; i < 8; ++i)
	{
		InLightView[i] = FMatrix::VectorMultiply(FrustumWorld[i], LightView);
	}

	float MinZ = FLT_MAX, MaxZ = -FLT_MAX;
	for (int32 i = 0; i < 8; ++i)
	{
		MinZ = std::min(MinZ, InLightView[i].Z);
		MaxZ = std::max(MaxZ, InLightView[i].Z);
	}
	const float Eps = 0.05f;
	const float WarpNear = std::max(Eps, MinZ);
	const float WarpFar = std::max(WarpNear + Eps, MaxZ + Eps);

	const float DotAbs = std::abs(Camera->GetForward().Dot(Forward));
	const float Lambda = LerpFloat(0.15f, 0.85f, DotAbs);
	FMatrix Warp = BuildPSMWarp(WarpNear, WarpFar, Lambda);

	float MinX = FLT_MAX, MinY = FLT_MAX, MinZW = FLT_MAX;
	float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZW = -FLT_MAX;

	for (int32 i = 0; i < 8; ++i)
	{
		FVector4 Clip = FMatrix::VectorMultiply(InLightView[i], Warp);
		if (Clip.W != 0.0f)
		{
			const float X = Clip.X / Clip.W;
			const float Y = Clip.Y / Clip.W;
			const float Z = Clip.Z / Clip.W;
			MinX = std::min(MinX, X);  MaxX = std::max(MaxX, X);
			MinY = std::min(MinY, Y);  MaxY = std::max(MaxY, Y);
			MinZW = std::min(MinZW, Z); MaxZW = std::max(MaxZW, Z);
		}
	}

	const float CropEps = 1e-3f;
	FMatrix OrthoCrop = BuildOrthographicFromBounds(MinX - CropEps, MaxX + CropEps, MinY - CropEps, MaxY + CropEps,
		MinZW, MaxZW + CropEps);

	OutLightViewProjection = LightView * Warp * OrthoCrop;
	return true;

}
