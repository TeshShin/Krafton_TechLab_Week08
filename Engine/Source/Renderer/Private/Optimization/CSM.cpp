#include "pch.h"
#include "Renderer/Public/Optimization/CSM.h"
namespace CSM
{
	TArray<float> ComputeCascadeSplitDistances(float nearZ, float farZ, uint32 numCascades, float lambda)
	{
		TArray<float> splits;
		if (numCascades < 1 || nearZ <= 0.0f || farZ <= nearZ)
		{
			// Fallback: single split covering [nearZ, farZ]
			splits.push_back(nearZ);
			splits.push_back(farZ);
			return splits;
		}

		splits.resize(numCascades + 1);
		splits[0] = nearZ;
		splits[numCascades] = farZ;

		const float n = nearZ;
		const float f = farZ;
		const float invCount = 1.0f / static_cast<float>(numCascades);
		const float clampedLambda = std::clamp(lambda, 0.0f, 1.0f);

		for (uint32 i = 1; i < numCascades; ++i)
		{
			float si = static_cast<float>(i) * invCount;
			// Linear split: equally spaced in view depth
			float linear = n + (f - n) * si;
			// Logarithmic split: constant ratio in view depth
			float logv = n * std::pow(f / n, si);
			// Practical split: blend
			splits[i] = linear * (1.0f - clampedLambda) + logv * clampedLambda;
		}

		// Ensure strict monotonic increase (robustness)
		for (uint32 i = 1; i <= numCascades; ++i)
		{
			if (splits[i] <= splits[i - 1])
			{
				splits[i] = std::nextafter(splits[i - 1], std::numeric_limits<float>::infinity());
			}
		}

		return splits;
	}

	TArray<FVector2> ComputeCascadeNdcZRanges(float nearZ, float farZ, const TArray<float>& splitsView, bool isPerspective)
	{
		TArray<FVector2> ranges;
		if (splitsView.size() < 2)
			return ranges;

		const uint32 numCascades = static_cast<uint32>(splitsView.size() - 1);
		ranges.resize(numCascades);

		for (uint32 i = 0; i < numCascades; ++i)
		{
			float vNear = splitsView[i];
			float vFar = splitsView[i + 1];
			float ndcNear = ViewZToNDC(vNear, nearZ, farZ, isPerspective);
			float ndcFar = ViewZToNDC(vFar, nearZ, farZ, isPerspective);
			ranges[i] = FVector2(ndcNear, ndcFar);
		}
		return ranges;
	}

	TArray<TArray<FVector>> BuildCascadeCornersNDC(float nearZ, float farZ, const TArray<float>& splitsView, bool isPerspective)
	{
		TArray<TArray<FVector>> cascades;
		if (splitsView.size() < 2)
			return cascades;

		auto ranges = ComputeCascadeNdcZRanges(nearZ, farZ, splitsView, isPerspective);
		cascades.resize(ranges.size());

		for (size_t i = 0; i < ranges.size(); ++i)
		{
			const float zN = ranges[i].X; // near NDC
			const float zF = ranges[i].Y; // far NDC

			TArray<FVector> corners;
			corners.resize(8);

			// Near plane (BL, BR, TL, TR)
			corners[0] = FVector(-1.0f, -1.0f, zN);
			corners[1] = FVector(1.0f, -1.0f, zN);
			corners[2] = FVector(-1.0f, 1.0f, zN);
			corners[3] = FVector(1.0f, 1.0f, zN);

			// Far plane (BL, BR, TL, TR)
			corners[4] = FVector(-1.0f, -1.0f, zF);
			corners[5] = FVector(1.0f, -1.0f, zF);
			corners[6] = FVector(-1.0f, 1.0f, zF);
			corners[7] = FVector(1.0f, 1.0f, zF);

			cascades[i] = std::move(corners);
		}

		return cascades;
	}

	TArray<TArray<FVector>> BuildCascadeCornersWorldFromNDC(const FMatrix& invViewProj, const TArray<TArray<FVector>>& cornersNDC)
	{
		TArray<TArray<FVector>> cascadesWS;
		cascadesWS.resize(cornersNDC.size());

		for (size_t ci = 0; ci < cornersNDC.size(); ++ci)
		{
			const auto& cNDC = cornersNDC[ci];
			TArray<FVector> cWS;
			cWS.resize(cNDC.size());

			for (size_t i = 0; i < cNDC.size(); ++i)
			{
				const FVector& ndc = cNDC[i];
				const FVector4 worldH = invViewProj.TransformHomogeneous(ndc);
				if (std::abs(worldH.W) < 1e-6f)
				{
					cWS[i] = FVector(0.0f, 0.0f, 0.0f);
				}
				else
				{
					cWS[i] = FVector(worldH.X / worldH.W, worldH.Y / worldH.W, worldH.Z / worldH.W);
				}
			}

			cascadesWS[ci] = std::move(cWS);
		}

		return cascadesWS;
	}

	static void BuildLightBasis(const FVector& dirIn, FVector& outForward, FVector& outRight, FVector& outUp)
	{
		FVector d = dirIn.GetNormalized();
		if (d.LengthSquared() < 1e-6f)
		{
			d = FVector(1.0f, 0.0f, 0.0f);
		}

		// Choose an up candidate to avoid degeneracy
		FVector upCand = (std::abs(d.Z) > 0.99f) ? FVector(0.0f, 1.0f, 0.0f) : FVector(0.0f, 0.0f, 1.0f);

		FVector right = upCand.Cross(d).GetNormalized();
		FVector up = d.Cross(right).GetNormalized();

		outForward = d;
		outRight = right;
		outUp = up;
	}

	FMatrix BuildDirectionalLightView(const FVector& lightDirection, const FVector& targetPosition, float viewOffset)
	{
		FVector forward, right, up;
		BuildLightBasis(lightDirection, forward, right, up);

		// Place the eye behind the target along -forward
		FVector eye = targetPosition - forward * viewOffset;

		return FMatrix::CreateViewFromAxes(eye, right, up, forward);
	}

	TArray<TArray<FVector>> TransformCascadesToLightView(const FVector& lightDirection,
		const TArray<TArray<FVector>>& cascadesWorld,
		float viewOffset)
	{
		TArray<TArray<FVector>> cascadesLS;
		cascadesLS.resize(cascadesWorld.size());

		for (size_t ci = 0; ci < cascadesWorld.size(); ++ci)
		{
			const auto& cw = cascadesWorld[ci];

			// Compute cascade center
			FVector center(0.0f, 0.0f, 0.0f);
			for (const auto& p : cw)
			{
				center.X += p.X; center.Y += p.Y; center.Z += p.Z;
			}
			center.X /= static_cast<float>(cw.size());
			center.Y /= static_cast<float>(cw.size());
			center.Z /= static_cast<float>(cw.size());

			// Build view and transform
			FMatrix lightView = BuildDirectionalLightView(lightDirection, center, viewOffset);

			TArray<FVector> lsCorners;
			lsCorners.resize(cw.size());
			for (size_t i = 0; i < cw.size(); ++i)
			{
				lsCorners[i] = lightView.TransformPosition(cw[i]);
			}

			cascadesLS[ci] = std::move(lsCorners);
		}

		return cascadesLS;
	}

	TArray<FAABB> ComputeLightSpaceAABBs(const TArray<TArray<FVector>>& cascadesLightSpace)
	{
		TArray<FAABB> result;
		result.resize(cascadesLightSpace.size());

		for (size_t ci = 0; ci < cascadesLightSpace.size(); ++ci)
		{
			FAABB box;
			for (const auto& p : cascadesLightSpace[ci])
			{
				box.Min.X = std::min(box.Min.X, p.X);
				box.Min.Y = std::min(box.Min.Y, p.Y);
				box.Min.Z = std::min(box.Min.Z, p.Z);
				box.Max.X = std::max(box.Max.X, p.X);
				box.Max.Y = std::max(box.Max.Y, p.Y);
				box.Max.Z = std::max(box.Max.Z, p.Z);
			}
			result[ci] = box;
		}
		return result;
	}

	TArray<FMatrix> BuildOrthoFromAABBs(const TArray<FAABB>& aabbs)
	{
		TArray<FMatrix> proj;
		proj.resize(aabbs.size());
		for (size_t i = 0; i < aabbs.size(); ++i)
		{
			const auto& b = aabbs[i];
			// DirectX off-center orthographic: left, right, bottom, top, near, far
			proj[i] = FMatrix::CreateOrthographicOffCenter(b.Min.X, b.Max.X, b.Min.Y, b.Max.Y, b.Min.Z, b.Max.Z);
		}
		return proj;
	}

	TArray<FMatrix> BuildCascadeLightVP(const FVector& lightDirection,
		const TArray<TArray<FVector>>& cascadesWorld,
		float viewOffset)
	{
		// 1) World -> Light space
		auto cascadesLS = TransformCascadesToLightView(lightDirection, cascadesWorld, viewOffset);
		// 2) AABB fit
		auto aabbs = ComputeLightSpaceAABBs(cascadesLS);

		// 3) Ortho
		auto proj = BuildOrthoFromAABBs(aabbs);

		// 4) Rebuild view per cascade (same as in step 4) and compose VP
		TArray<FMatrix> lightVP;
		lightVP.resize(cascadesWorld.size());
		for (size_t ci = 0; ci < cascadesWorld.size(); ++ci)
		{
			// Compute center again for the view placement
			FVector center(0.0f, 0.0f, 0.0f);
			for (const auto& p : cascadesWorld[ci])
			{
				center.X += p.X; center.Y += p.Y; center.Z += p.Z;
			}
			center.X /= static_cast<float>(cascadesWorld[ci].size());
			center.Y /= static_cast<float>(cascadesWorld[ci].size());
			center.Z /= static_cast<float>(cascadesWorld[ci].size());

			FMatrix view = BuildDirectionalLightView(lightDirection, center, viewOffset);
			lightVP[ci] = view * proj[ci];
		}
		return lightVP;
	}
}
