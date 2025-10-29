#pragma once

#include "Core/Public/CoreTypes.h"

/**
 * @brief Cascaded Shadow Map utilities
 *
 * Step 1: Split the camera view frustum into sub-frusta (cascades).
 * This provides view-space split distances using the common practical split scheme
 * that blends linear and logarithmic partitions controlled by lambda.
 */
namespace CSM
{
	struct FCascadeConfig
	{
		uint32 NumCascades = 4;   // number of cascades (>=1)
		float  Lambda = 0.5f;     // blend factor [0,1]: 0=linear, 1=logarithmic
	};

	/**
	 * @brief Compute view-space split distances for cascades.
	 * @param nearZ       Camera near plane distance (view-space, > 0)
	 * @param farZ        Camera far plane distance  (view-space, > nearZ)
	 * @param numCascades Number of cascades (>=1)
	 * @param lambda      Blend factor [0,1] between linear and logarithmic partitions
	 * @return Vector of size (numCascades+1) with monotonically increasing distances:
	 *         [ nearZ, split1, split2, ..., farZ ]
	 */
	TArray<float> ComputeCascadeSplitDistances(float nearZ, float farZ, uint32 numCascades, float lambda);

	//------------------------------------------------------------------------------
	// Step 2 helpers: map view-space split Z to NDC and build NDC frustum corners
	//------------------------------------------------------------------------------

	/**
	 * @brief Convert a view-space Z to NDC Z for DirectX conventions.
	 * @param zView  View-space depth (>0)
	 * @param nearZ  Camera near plane (>0)
	 * @param farZ   Camera far plane  (>nearZ)
	 * @param isPerspective If true, use perspective mapping, otherwise orthographic
	 */
	inline float ViewZToNDC(float zView, float nearZ, float farZ, bool isPerspective)
	{
		if (!isPerspective)
		{
			// D3D orthographic projection maps z linearly to [0,1]
			return (zView - nearZ) / (farZ - nearZ);
		}

		// D3D11 perspective (left-handed) NDC z = A + B / z
		const float A = farZ / (farZ - nearZ);
		const float B = -nearZ * farZ / (farZ - nearZ);
		return A + (B / zView);
	}

	/**
	 * @brief Build per-cascade NDC z ranges from view-space split distances.
	 * @param nearZ       Camera near plane
	 * @param farZ        Camera far plane
	 * @param splitsView  Split distances in view space (size = numCascades+1)
	 * @param isPerspective Camera projection type
	 * @return Array of (nearNDC, farNDC) for each cascade
	 */
	TArray<FVector2> ComputeCascadeNdcZRanges(float nearZ, float farZ, const TArray<float>& splitsView, bool isPerspective);

	/**
	 * @brief Build NDC-space frustum corners for each cascade.
	 *        Corner ordering matches existing usage:
	 *        [0..3] near plane (BL, BR, TL, TR), [4..7] far plane (BL, BR, TL, TR)
	 * @param nearZ, farZ Camera near/far
	 * @param splitsView  View-space split distances (numCascades+1)
	 * @param isPerspective Projection type
	 * @return Vector of cascades, each containing 8 NDC corners (xyz)
	 */
	TArray<TArray<FVector>> BuildCascadeCornersNDC(float nearZ, float farZ, const TArray<float>& splitsView, bool isPerspective);

	/**
	 * @brief Transform NDC cascade corners to world space using inverse ViewProjection.
	 * @param invViewProj  Camera (View*Proj)^-1 matrix
	 * @param cornersNDC   Output from BuildCascadeCornersNDC
	 * @return Per-cascade 8 world-space corners (xyz)
	 */
	TArray<TArray<FVector>> BuildCascadeCornersWorldFromNDC(const FMatrix& invViewProj, const TArray<TArray<FVector>>& cornersNDC);

	/** Convenience: Step 2+3 in one call */
	inline TArray<TArray<FVector>> BuildCascadeCornersWorld(const FMatrix& invViewProj,
		float nearZ, float farZ,
		const TArray<float>& splitsView,
		bool isPerspective)
	{
		auto ndc = BuildCascadeCornersNDC(nearZ, farZ, splitsView, isPerspective);
		return BuildCascadeCornersWorldFromNDC(invViewProj, ndc);
	}

	//------------------------------------------------------------------------------
	// Step 4: Move sub-frusta into light view space
	//------------------------------------------------------------------------------

	/**
	 * @brief Compute a stable LH view matrix from light direction and a target position.
	 *        Right/Up are derived to form an orthonormal basis.
	 */
	FMatrix BuildDirectionalLightView(const FVector& lightDirection, const FVector& targetPosition, float viewOffset = 1000.0f);

	/**
	 * @brief Transform per-cascade world-space corners into the light's view space.
	 * @param lightDirection Directional light forward direction (world)
	 * @param cascadesWorld  Per-cascade 8 world corners
	 * @param viewOffset     Distance to place the light camera behind the cascade center along -dir
	 * @return Per-cascade 8 corners in light view space
	 */
	TArray<TArray<FVector>> TransformCascadesToLightView(const FVector& lightDirection,
		const TArray<TArray<FVector>>& cascadesWorld,
		float viewOffset = 1000.0f);

	//------------------------------------------------------------------------------
	// Step 5: Fit AABBs in light space and build per-cascade orthographic matrices
	//------------------------------------------------------------------------------

	struct FAABB
	{
		FVector Min = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		FVector Max = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	};

	/**
	 * @brief Fit axis-aligned bounding boxes in light view space for each cascade.
	 */
	TArray<FAABB> ComputeLightSpaceAABBs(const TArray<TArray<FVector>>& cascadesLightSpace);

	/**
	 * @brief Build orthographic projection matrices from light-space AABBs.
	 *        Uses DirectX LH convention (Z:[0,1]).
	 */
	TArray<FMatrix> BuildOrthoFromAABBs(const TArray<FAABB>& aabbs);

	/**
	 * @brief Convenience: from world corners and direction, return per-cascade LightVP matrices.
	 */
	TArray<FMatrix> BuildCascadeLightVP(const FVector& lightDirection,
		const TArray<TArray<FVector>>& cascadesWorld,
		float viewOffset = 1000.0f);
}
