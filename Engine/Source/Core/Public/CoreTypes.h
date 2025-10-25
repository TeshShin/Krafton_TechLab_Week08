#pragma once
#include <array>
#include "Core/Public/Math/Vector.h"
#include "Core/Public/Math/Matrix.h"
#include "Core/Public/Types.h"

struct FModelConstants
{
	FMatrix World;
	FMatrix WorldInverseTranspose;
};

struct FCameraConstants
{
	FMatrix View;
	FMatrix Projection;
	FVector ViewWorldLocation;
	float NearClip;
	float FarClip;
};

struct FViewportConstants
{
	FVector2 RenderTargetSize;
	int IsOrthographic;
};

#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_SHININESS_MAP	 (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

struct FMaterialConstants
{
	FVector4 Ka;
	FVector4 Kd;
	FVector4 Ks;
	float Ns;
	float Ni;
	float D;
	uint32 MaterialFlags;
	float Time; // Time in seconds
};

struct FLineVertex
{
	FVector Position;
	FVector4 Color;
};

struct FNormalVertex
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 TexCoord;
	FVector4 Tangent;

	static FNormalVertex FromLineVertex(const FLineVertex& Line)
	{
		FNormalVertex NewVertex;
		NewVertex.Position = Line.Position;
		NewVertex.Normal = FVector::ZeroVector();
		NewVertex.Color = Line.Color;
		NewVertex.TexCoord = FVector2(0, 0);
		NewVertex.Tangent = FVector4::ZeroVector();
		return NewVertex;
	}
};

struct FRay
{
	FVector4 Origin;
	FVector4 Direction;
};

/**
 * @brief Render State Settings for Actor's Component
 */
struct FRenderState
{
	ECullMode CullMode = ECullMode::None;
	EFillMode FillMode = EFillMode::Solid;
	// Shadow rasterizer bias
	int32_t DepthBias = 0;
	float   DepthBiasClamp = 0.0f;
	float   SlopeScaledDepthBias = 0.0f;
};

/**
 * @brief 변환 정보를 담는 구조체
 */
struct FTransform
{
	FVector Location = FVector(0.0f, 0.0f, 0.0f);
	FVector Rotation = FVector(0.0f, 0.0f, 0.0f);
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

	FTransform() = default;

	FTransform(const FVector& InLocation, const FVector& InRotation = FVector::ZeroVector(),
		const FVector& InScale = FVector::OneVector())
		: Location(InLocation), Rotation(InRotation), Scale(InScale)
	{
	}
};

/**
 * @brief 2차원 좌표의 정보를 담는 구조체
 */
struct FPoint
{
	float X = 0.0f;
	float Y = 0.0f;
};

/**
 * @brief 윈도우를 비롯한 2D 화면의 정보를 담는 구조체
 */
struct FRect
{
	float Left = 0.0f;
	float Top = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;

	float GetRight() const { return Left + Width; }
	float GetBottom() const { return Top + Height; }
};

/// Light Constants
// HLSL의 cbuffer는 16바이트 단위로 정렬되므로, C++ 구조체도 이에 맞춰야 합니다.

/**
 * @brief Ambient Light structure for global illumination
 * @note 16-byte aligned (total 16 bytes)
 */
struct FAmbientLight
{
	FVector Color;      // 12 bytes
	float Intensity;    // 4 bytes
};

/**
 * @brief Light constants for GPU ConstantBuffer
 * @note Register b10, 32 bytes total
 * @details Only Ambient light uses ConstantBuffer
 *          All dynamic lights (Directional, Point, Spot) use StructuredBuffer (t6)
 */
struct FLightConstants
{
	uint32 UnifiedLightCount;       // 4 bytes  - Number of lights in unified StructuredBuffer
	float Padding[3];               // 12 bytes - Padding for 16-byte alignment
};

struct FForwardPlusCameraConstants
{
	FMatrix View;
	FMatrix Proj;
	FMatrix InvProj;
	std::array<uint32, 2>  ScreenSize;     // pixels (width, height)
	std::array<uint32, 2>  ViewportOrigin; // pixels (top-left x,y)
	uint32  NumTilesX;      // dispatch dim X
	uint32  NumTilesY;      // dispatch dim Y
	uint32  NumZSlices;     // dispatch dim Z
	float   NearZ;          // view-space near (>= 0)
	float   FarZ;           // view-space far  (>  NearZ)
};

struct FForwardPlusConstants
{
	uint32 NumLights;                // number of entries in DynamicLights
	uint32 MaxLightsPerCluster;      // capacity per cluster
	uint32 TotalClusters;            // NumTilesX*NumTilesY*NumZSlices
	uint32 FP_Pad0;
};
