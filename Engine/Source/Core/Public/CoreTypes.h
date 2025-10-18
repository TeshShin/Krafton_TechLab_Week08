#pragma once
#include "Core/Public/Math/Vector.h"
#include "Core/Public/Math/Matrix.h"
#include "Core/Public/Types.h"

//struct BatchLineContants
//{
//	float CellSize;
//	//FMatrix BoundingBoxModel;
//	uint32 ZGridStartIndex; // 인덱스 버퍼에서, z방향쪽 그리드가 시작되는 인덱스
//	uint32 BoundingBoxStartIndex; // 인덱스 버퍼에서, 바운딩박스가 시작되는 인덱스
//};

struct FModelConstants
{
	FMatrix World;
	FMatrix WorldInverseTranspose;
};

struct FCameraConstants
{
	FCameraConstants() : NearClip(0), FarClip(0)
	{
		View = FMatrix::Identity();
		Projection = FMatrix::Identity();
	}

	FMatrix View;
	FMatrix Projection;
	FVector ViewWorldLocation;
	float NearClip;
	float FarClip;
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

struct FVertex
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
	FAmbientLight GlobalAmbient;    // 16 bytes - Scene-wide ambient illumination
	uint32 UnifiedLightCount;       // 4 bytes  - Number of lights in unified StructuredBuffer
	float Padding[3];               // 12 bytes - Padding for 16-byte alignment
};
