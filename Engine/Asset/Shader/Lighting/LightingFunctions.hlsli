//--------------------------------------------------------------------------------------
// [LIGHTING FUNCTIONS] Common Lighting Calculations
// This file contains reusable lighting functions for both vertex and pixel shaders
//--------------------------------------------------------------------------------------

#ifndef LIGHTING_FUNCTIONS_HLSL
#define LIGHTING_FUNCTIONS_HLSL

float linstep(float minV, float maxV, float v)
{
    return saturate((v - minV) / (maxV - minV));
}

float ReduceLightBleeding(float p_max, float Amount)
{
	// [0, Amount] 구간을 제거하고 (Amount, 1] 구간을 선형 rescale
    return linstep(Amount, 1.0f, p_max);
}

//--------------------------------------------------------------------------------------
// [MODULAR LIGHTING SYSTEM] Lighting Result Structure
//--------------------------------------------------------------------------------------

/**
 * @brief Separated lighting components for proper material application
 * @note Allows independent Kd and Ks multiplication
 */
struct FLightingResult
{
    float3 Diffuse;   // Diffuse contribution (to be multiplied by Kd)
    float3 Specular;  // Specular contribution (to be multiplied by Ks)
	float3 Ambient;	  // Ambient contribution (to be multiplied by Ka)
};

struct FLightViewProj
{
	row_major float4x4 ViewMatrix;
	row_major float4x4 ProjectionMatrix;
};

//--------------------------------------------------------------------------------------
// [LIGHTING MODELS] Modular lighting calculation functions
//--------------------------------------------------------------------------------------

/**
 * @brief Calculate Blinn-Phong lighting (Diffuse + Specular)
 * @param LightDir Direction from surface to light (normalized)
 * @param Normal Surface normal (normalized)
 * @param ViewDir Direction from surface to camera (normalized)
 * @param LightColor Light's color and intensity
 * @param SpecularPower Shininess exponent (Ns)
 * @return Separated diffuse and specular contributions
 */
FLightingResult CalculateBlinnPhongLighting(float3 LightDir, float3 Normal, float3 ViewDir,
                                             float3 LightColor, float SpecularPower)
{
    FLightingResult Result;
	Result.Ambient = float3(0, 0, 0);

    // Diffuse (Lambertian)
    float NdotL = saturate(dot(Normal, LightDir));
    Result.Diffuse = LightColor * NdotL;

    // Specular (Blinn-Phong)
    float3 HalfVec = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(Normal, HalfVec));
    float SpecularFactor = pow(NdotH, SpecularPower);
	Result.Specular = LightColor * SpecularFactor;

    return Result;
}

/**
 * @brief Calculate Lambert lighting (Diffuse only, no specular)
 * @param LightDir Direction from surface to light (normalized)
 * @param Normal Surface normal (normalized)
 * @param LightColor Light's color and intensity
 * @return Diffuse contribution only
 */
FLightingResult CalculateLambertLighting(float3 LightDir, float3 Normal, float3 LightColor)
{
    FLightingResult Result;
    Result.Ambient = float3(0, 0, 0);

    float NdotL = saturate(dot(Normal, LightDir));
    Result.Diffuse = LightColor * NdotL;
    Result.Specular = float3(0, 0, 0);

    return Result;
}

//--------------------------------------------------------------------------------------
// [UNIFIED FORWARD RENDERING] Dynamic Light Calculation with Attenuation
//--------------------------------------------------------------------------------------

// [IMPORTANT] Light Type Enumeration - Must match C++ EDynamicLightType
#define LIGHT_TYPE_AMBIENT		0
#define LIGHT_TYPE_DIRECTIONAL	1
#define LIGHT_TYPE_POINT		2
#define LIGHT_TYPE_SPOT			3

#define SHADOW_FILTER_NONE			0
#define SHADOW_FILTER_PCF			1
#define SHADOW_FILTER_VSM			2
#define SHADOW_FILTER_VSM_BOX		3
#define SHADOW_FILTER_VSM_GAUSSIAN	4

struct FShadowConstants
{
	uint FilterType;
	float VSM_LightBleedReduction;
	float2 Pad;
};

struct FDirectionalCSMLightConstants
{
	uint DirNumCascades;
	float3 CSMPadding;

	float4 DirCascadeSplits[12];
	FLightViewProj DirCascadeMatrices[12];
};

// [IMPORTANT] Must match C++ FUnifiedDynamicLight exactly (field names and order)
struct FUnifiedDynamicLight
{
    float3 Position;            // 12 bytes - World space light position
    float Intensity;            // 4 bytes  - Light intensity (0.0 - 20.0)
    float3 Color;               // 12 bytes - RGB color filter (0.0 - 1.0 per channel)
    float SourceRadius;         // 4 bytes  - Light influence radius / Physical size
    float3 Direction;           // 12 bytes - Light direction (Spot/Rect only, unused for Point)
    float FalloffExponent;      // 4 bytes  - Radial falloff exponent (2.0 - 16.0)
    float Param0;               // 4 bytes  - Spot: InnerConeAngle (radians), Rect: Width
    float Param1;               // 4 bytes  - Spot: OuterConeAngle (radians), Rect: Height
    uint LightType;             // 4 bytes  - Light type identifier
	float ShadowBias;            // 4 bytes - Shadow Bias
	uint bCastShadows;         // 4 bytes - Light Does Cast Shadows
	int ShadowMapIndex;        // 4 bytes - Shadow Texture2D Array Index
	int ShadowFilterSize;      // 4 bytes  - PCF/Box Size, Gauss Radius
	float ShadowGaussSigma;      // 4 bytes - VSM Gaussian Sigma
	float ShadowResolutionScale; // 4 bytes - Shadow Map Resolution
	float Pad[3];				 // 12 bytes - Padding
};

FLightingResult CalculateDynamicLight(FUnifiedDynamicLight Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower)
{
    FLightingResult Result;
    Result.Diffuse = float3(0, 0, 0);
	Result.Specular = float3(0, 0, 0);
	Result.Ambient = float3(0, 0, 0);

    // Early exit for disabled/dummy lights
    if (Light.Intensity <= 0.0f)
        return Result;

    float3 LightDir;
    float Attenuation = Light.Intensity;

	// Ambient Light: no direction or attenuation
	if (Light.LightType == LIGHT_TYPE_AMBIENT)
	{
		Result.Ambient = Light.Color * Light.Intensity;
		return Result;
	}
    // Directional Light: parallel rays, no distance attenuation
    if (Light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        LightDir = normalize(-Light.Direction);
    }
    // Point/Spot Lights: radial light with distance attenuation
    else
    {
        float3 LightVec = Light.Position - WorldPos;
        float Distance = length(LightVec);

        // Early exit if outside light influence radius
        if (Distance > Light.SourceRadius)
            return Result;

        LightDir = normalize(LightVec);

        // Distance Attenuation (radial falloff)
        float NormalizedDist = Distance / Light.SourceRadius;
		Attenuation = pow(saturate(1.0f - NormalizedDist), Light.FalloffExponent);
        Attenuation *= Light.Intensity;

        // Spot-specific attenuation (if applicable)
        if (Light.LightType == LIGHT_TYPE_SPOT)
        {
            float Theta = dot(LightDir, -Light.Direction);
			float InnerCos = cos(radians(Light.Param0)); // InnerConeAngle
            float OuterCos = cos(radians(Light.Param1));  // OuterConeAngle

            if (Theta < OuterCos)
                return Result; // Outside cone

            // Smooth cone falloff
            float SpotAttenuation = saturate((Theta - OuterCos) / (InnerCos - OuterCos));
            Attenuation *= SpotAttenuation;
        }
    }

    // Apply attenuated light color
    float3 AttenuatedLightColor = Light.Color * Attenuation;

#if defined (LIGHTING_MODEL_PHONG) || defined (LIGHTING_MODEL_GOURAUD)
    // Calculate Blinn-Phong lighting (separated Diffuse and Specular)
    Result = CalculateBlinnPhongLighting(LightDir, Normal, ViewDir, AttenuatedLightColor, SpecularPower);
#elif defined (LIGHTING_MODEL_LAMBERT)
    // Calculate Lambert lighting (Diffuse only)
    Result = CalculateLambertLighting(LightDir, Normal, AttenuatedLightColor);
#else
    // unknown lighting model - return zero contribution
    Result.Diffuse = float3(0, 0, 0);
    Result.Specular = float3(0, 0, 0);
#endif

    return Result;
}


//--------------------------------------------------------------------------------------
// [Shadow Functions]
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithShadows(FUnifiedDynamicLight Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
	Texture2DArray SpotShadowAtlas, StructuredBuffer<FLightViewProj> SpotLightShadowMatrices, TextureCubeArray PointShadowAtlas,
	Texture2D DirectionalTexture, FLightViewProj DirectionalShadowMatrix, SamplerComparisonState ShadowSampler)
{
	FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);

    float ShadowFactor = 1.0f;
    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
       if (Light.LightType == LIGHT_TYPE_SPOT)
       {
       	  FLightViewProj ViewProj = SpotLightShadowMatrices[Light.ShadowMapIndex];
          float4 LightSpacePos = mul(mul(float4(WorldPos, 1.0f), ViewProj.ViewMatrix), ViewProj.ProjectionMatrix);
          float3 ShadowCoords = LightSpacePos.xyz / LightSpacePos.w;

          ShadowCoords.x = ShadowCoords.x * 0.5f + 0.5f;
          ShadowCoords.y = ShadowCoords.y * -0.5f + 0.5f;

          float3 SampleCoords = float3(ShadowCoords.xy, Light.ShadowMapIndex);
          ShadowFactor = SpotShadowAtlas.SampleCmpLevelZero(ShadowSampler, SampleCoords, ShadowCoords.z - Light.ShadowBias);
       }
       else if (Light.LightType == LIGHT_TYPE_POINT)
       {
          // 픽셀에서 라이트까지의 선형 깊이 계산
          float3 LightToPixelDir = WorldPos - Light.Position;
          float PixelDepth = length(LightToPixelDir) / Light.SourceRadius;
       	  float3 SwizzledDir = float3(LightToPixelDir.y, LightToPixelDir.z, LightToPixelDir.x);

          float4 ShadowCoord = float4(SwizzledDir, Light.ShadowMapIndex);
          // (선형 깊이) vs (섀도우맵에 저장된 선형 깊이) 비교
          ShadowFactor = PointShadowAtlas.SampleCmpLevelZero(ShadowSampler, ShadowCoord, PixelDepth - Light.ShadowBias);
       }
       else if (Light.LightType == LIGHT_TYPE_DIRECTIONAL)
       {
       	  FLightViewProj ViewProj = DirectionalShadowMatrix;
       	  float4 LightSpacePos = mul(mul(float4(WorldPos, 1.0f), ViewProj.ViewMatrix), ViewProj.ProjectionMatrix);
          float3 ShadowCoords = LightSpacePos.xyz / LightSpacePos.w;

          ShadowCoords.x = ShadowCoords.x * 0.5f + 0.5f;
          ShadowCoords.y = ShadowCoords.y * -0.5f + 0.5f;

          float2 SampleCoords = ShadowCoords.xy;

          ShadowFactor = DirectionalTexture.SampleCmpLevelZero(ShadowSampler, SampleCoords, ShadowCoords.z - Light.ShadowBias);
       }
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;

    return Result;
}

//--------------------------------------------------------------------------------------
// CSM Helper Functions
//--------------------------------------------------------------------------------------
uint SelectCascadeIndex(FDirectionalCSMLightConstants Constants, float viewSpaceZ, uint numCascades)
{
	for (uint i = 0; i < numCascades; ++i)
	{
		if (viewSpaceZ <= Constants.DirCascadeSplits[i + 1].x)
		{
			return i;
		}
	}

	return numCascades - 1;
}

//TODO: 경계선에서 blend 처리해주는 함수
float ComputeCascadeBlendFactor()
{
	return 1;
}


//==============================================================================
// Shadow Helper Functions
//==============================================================================

// NDC 좌표계로 변환해서 UV 좌표계를 얻어주는 helper function
float3 GetSampleCoords(float3 WorldPos, float4x4 ViewMatrix, float4x4 ProjectionMatrix)
{
    float4 LightSpacePos = mul(mul(float4(WorldPos, 1.0f), ViewMatrix), ProjectionMatrix);
    float3 uvw = LightSpacePos.xyz / LightSpacePos.w;

    uvw.x = uvw.x * 0.5f + 0.5f;
    uvw.y = uvw.y * -0.5f + 0.5f;

    return uvw;
}

void BuildOrthonormalBasis(float3 N, out float3 T, out float3 B)
{
	float3 a = (abs(N.y) < 0.999f) ? float3(0, 1, 0) : float3(0, 0, 1);
	T = normalize(cross(a, N));
	B = cross(N, T);
}


//==============================================================================
// PCF (Percentage Closer Filtering) - 비선형 필터
//==============================================================================

float PCF_Texture2DArray(Texture2DArray<float> ShadowMap, SamplerComparisonState Sampler,
                         float3 uvw, float RefZ, int FilterSize, float TexSize)
{
    int R = FilterSize / 2;
    float ShadowSamples = 0.0;

    float2 st = floor(uvw.xy * TexSize);

    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            float2 uv = (st + float2(x, y) + 0.5) / TexSize;
            ShadowSamples += ShadowMap.SampleCmpLevelZero(Sampler, float3(uv, uvw.z), RefZ);
        }
    }

    return ShadowSamples / ((2 * R + 1) * (2 * R + 1));
}

float PCF_Texture2D(Texture2D<float> ShadowMap, SamplerComparisonState Sampler,
                    float2 uv, float RefZ, int FilterSize, float TexSize)
{
    int R = FilterSize / 2;
    float ShadowSamples = 0.0;

    float2 st = floor(uv * TexSize);

    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            float2 sampleUV = (st + float2(x, y) + 0.5) / TexSize;
            ShadowSamples += ShadowMap.SampleCmpLevelZero(Sampler, sampleUV, RefZ);
        }
    }

    return ShadowSamples / ((2 * R + 1) * (2 * R + 1));
}

float PCF_TextureCubeArray(TextureCubeArray<float> ShadowMap, SamplerComparisonState Sampler,
                           float4 uvw, float RefZ, int FilterSize, float Resolution)
{
	// 1. 커널 반경(R) 및 총 샘플 수 계산
	int R = FilterSize / 2;
	float totalSamples = (float)FilterSize * FilterSize;

	// 2. "텍셀 크기"를 기반으로 방향 벡터를 얼마나 변경할지 "섭동 크기"를 정의합니다.
	// 이 값은 씬의 스케일이나 원하는 부드러움에 따라 조절이 필요할 수 있습니다.
	// (1.0 / width)는 텍스처 중앙에서 약 1픽셀에 해당하는 각도(라디안)와 비슷합니다.
	float perturbationScale = (1.0 / Resolution) * 2.0; // 2.0은 임의의 스케일 값

	// 3. 현재 샘플링 방향(V)의 접선 공간(T1, T2) 계산
	float3 V = uvw.xyz;
	// V와 거의 평행하지 않은 "안전한" up 벡터를 찾습니다.
	float3 up = abs(V.y) > 0.99 ? float3(1, 0, 0) : float3(0, 1, 0);
	// T1 (Tangent)
	float3 T1 = normalize(cross(up, V));
	// T2 (Bitangent)
	float3 T2 = normalize(cross(V, T1));

	// 4. 2D 커널 루프를 돌며 샘플 누적
	float ShadowSamples = 0.0;

	for (int y = -R; y <= R; ++y)
	{
		for (int x = -R; x <= R; ++x)
		{
			// 5. (x, y) 오프셋을 접선 공간에 적용하여 새로운 방향 벡터 계산
			float2 offset = float2(x, y) * perturbationScale;
			float3 perturbed_V = normalize(V + T1 * offset.x + T2 * offset.y);

			// 6. 새 방향 벡터와 원래 배열 인덱스(uvw.w)로 샘플링 좌표 생성
			float4 sample_uvw = float4(perturbed_V, uvw.w);

			// 7. 샘플링 및 누적
			ShadowSamples += ShadowMap.SampleCmpLevelZero(Sampler, sample_uvw, RefZ);
		}
	}

	// 8. 평균값 반환
	return ShadowSamples / totalSamples;
}

//==============================================================================
// VSM (Variance Shadow Mapping) - 선형 필터
//==============================================================================

// M1 = E(x), M2 = E(x^2)
float VSM_Visibility(float2 Moments, float t, float LightBleedReduction)
{
    if (t <= Moments.x)
    {
        return 1.0f;
    }

    // Variance = E(z^2) - (E(z))^2
    float variance = Moments.y - (Moments.x * Moments.x);
    const float MinVariance = 1e-5f;
    variance = max(variance, MinVariance);

    // Chebyshev upper bound term
    float d = t - Moments.x;
    float pMax = variance / (d * d + variance);

    // Reduce light bleeding
    pMax = ReduceLightBleeding(pMax, LightBleedReduction);

    return saturate(pMax);
}

//--------------------------------------------------------------------------------------
// [VSM] Box-filtered
//--------------------------------------------------------------------------------------
float2 SampleVSMBox(Texture2DArray<float2> ShadowMomentsArray, SamplerState Sampler,
                    float3 uvw, int KernelSize, float TexSize)
{
    int radius = max(0, (KernelSize & 1) ? KernelSize / 2 : (KernelSize - 1) / 2);

    float2 st = TexSize * uvw.xy;
    float2 sum = float2(0.0, 0.0);
    int count = (2 * radius + 1) * (2 * radius + 1);

    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            float2 uv = (st + float2(i, j) + 0.5f) / TexSize;
            sum += ShadowMomentsArray.SampleLevel(Sampler, float3(uv, uvw.z), 0.0).xy;
        }
    }
    return (count > 0) ? (sum / count) : ShadowMomentsArray.SampleLevel(Sampler, uvw, 0.0).xy;
}
float2 SampleVSMBox_Point(TextureCubeArray<float2> ShadowMomentsArray, SamplerState Sampler,
                    float4 dir, uint KernelSize, float Resolution)
{
    int radius = max(0, (KernelSize & 1) ? KernelSize / 2 : (KernelSize - 1) / 2);

	float3 T, B;
	BuildOrthonormalBasis(normalize(dir.xyz), T, B);

	float TexelAngle = 1 / Resolution;

    float2 sum = float2(0.0, 0.0);
    int count = (2 * radius + 1) * (2 * radius + 1);

    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
			float2 angle = TexelAngle * float2(x, y);

            float3 dOffset = (dir.xyz + angle.x * T + angle.y * B);
            sum += ShadowMomentsArray.SampleLevel(Sampler, float4(dOffset, dir.w), 0.0).xy;
        }
    }
    return (count > 0) ? (sum / count) : ShadowMomentsArray.SampleLevel(Sampler, dir, 0.0
    	).xy;
}

float2 SampleVSMBox_Texture2D(Texture2D<float2> ShadowMoments, SamplerState Sampler,
                              float2 uv, uint KernelSize, float TexSize)
{
    int radius = max(0, (KernelSize & 1) ? KernelSize / 2 : (KernelSize - 1) / 2);

    float2 st = TexSize * uv;
    float2 sum = float2(0.0, 0.0);
    int count = (2 * radius + 1) * (2 * radius + 1);

    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            float2 sampleUV = (st + float2(i, j) + 0.5f) / TexSize;
            sum += ShadowMoments.SampleLevel(Sampler, sampleUV, 0.0).xy;
        }
    }
    return (count > 0) ? (sum / count) : ShadowMoments.SampleLevel(Sampler, uv, 0.0).xy;
}

//--------------------------------------------------------------------------------------
// [VSM] Gaussian-filtered
//--------------------------------------------------------------------------------------
float2 SampleVSMGaussian(Texture2DArray<float2> ShadowMomentsArray, SamplerState Sampler,
                         float3 uvw, int radius, float sigma, float TexSize)
{
    radius = max(0, radius);

    float2 st = TexSize * uvw.xy;

    float oneDsum = 0.0f;
    [loop]
    for (int i = -radius; i <= radius; ++i)
    {
        float x = i / sigma;
        oneDsum += exp(-0.5f * x * x);
    }

    float norm2D = max(oneDsum * oneDsum, 1e-8f);

    float2 acc = float2(0.0f, 0.0f);
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        float jy = y / sigma;
        float wy = exp(-0.5f * jy * jy);

        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            float jx = x / sigma;
            float wx = exp(-0.5f * jx * jx);
            float w = wx * wy;

            float2 uv = (st + float2(x, y) + 0.5f) / TexSize;
            acc += w * ShadowMomentsArray.SampleLevel(Sampler, float3(uv, uvw.z), 0.0).xy;
        }
    }

    return acc / norm2D;
}

float2 SampleVSMGaussian_Point(TextureCubeArray<float2>ShadowMomentsArray, SamplerState Sampler,
                         float4 dir, int radius, float sigma, float Resolution)
{
	radius = max(0, radius);

	float texelAngle = 1.0f / Resolution;

	float oneDsum = 0.0f;
    [loop]
	for (int i = -radius; i <= radius; ++i)
	{
		float x = i / sigma;
		oneDsum += exp(-0.5f * x * x);
	}

	float norm2D = max(oneDsum * oneDsum, 1e-8f);

	float3 T, B;
	BuildOrthonormalBasis(normalize(dir.xyz), T, B);

	float2 acc = float2(0.0f, 0.0f);
    [loop]
	for (int y = -radius; y <= radius; ++y)
	{
		float jy = y / sigma;
		float wy = exp(-0.5f * jy * jy);

        [loop]
		for (int x = -radius; x <= radius; ++x)
		{
			float jx = x / sigma;
			float wx = exp(-0.5f * jx * jx);
			float w = wx * wy;

			float2 ang = float2(x, y) * texelAngle;
			float3 dOff = normalize(dir.xyz + ang.x * T + ang.y * B);
			acc += w * ShadowMomentsArray.SampleLevel(Sampler, float4(dOff, dir.w), 0.0).xy;
		}
	}

	return acc / norm2D;
}

float2 SampleVSMGaussian_Texture2D(Texture2D<float2> ShadowMoments, SamplerState Sampler,
                                   float2 uv, int radius, float sigma, float TexSize)
{
    radius = max(0, radius);

    float2 st = TexSize * uv;

    float oneDsum = 0.0f;
    [loop]
    for (int i = -radius; i <= radius; ++i)
    {
        float x = i / sigma;
        oneDsum += exp(-0.5f * x * x);
    }

    float norm2D = max(oneDsum * oneDsum, 1e-8f);

    float2 acc = float2(0.0f, 0.0f);
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        float jy = y / sigma;
        float wy = exp(-0.5f * jy * jy);

        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            float jx = x / sigma;
            float wx = exp(-0.5f * jx * jx);
            float w = wx * wy;

            float2 sampleUV = (st + float2(x, y) + 0.5f) / TexSize;
            acc += w * ShadowMoments.SampleLevel(Sampler, sampleUV, 0.0).xy;
        }
    }

    return acc / norm2D;
}

//==============================================================================
// Unified Shadow Functions
//==============================================================================

//--------------------------------------------------------------------------------------
// PCF Shadow (Hard/Soft) - Directional Light
//--------------------------------------------------------------------------------------


float SampleCSMShadow_PCF(FDirectionalCSMLightConstants Constants,
   float3 worldPos,
    float viewSpaceZ,
    Texture2DArray<float> DirectionalShadowArray,
    SamplerComparisonState ShadowSampler,
    float shadowBias,
    int filterSize)
{
	uint numCascade = Constants.DirNumCascades;
	if (numCascade == 0)
		return 1.0f;

	// Select Cascade Idx
	uint cascadeIdx = SelectCascadeIndex(Constants, viewSpaceZ, numCascade);

	FLightViewProj viewProj = Constants.DirCascadeMatrices[cascadeIdx];

	float3 uvw = GetSampleCoords(worldPos, viewProj.ViewMatrix, viewProj.ProjectionMatrix);
	float3 SampleCoords = float3(uvw.xy, cascadeIdx);
	float refZ = uvw.z - shadowBias;

	uint GlobalAtlasWidth, GlobalAtlasHeight, NumSlices;
	DirectionalShadowArray.GetDimensions(GlobalAtlasWidth, GlobalAtlasHeight, NumSlices);
	float fGlobalAtlasWidth = (float)GlobalAtlasWidth;

	float shadow0 = (filterSize <= 1)
        ? DirectionalShadowArray.SampleCmp(ShadowSampler, SampleCoords, refZ)
        : PCF_Texture2DArray(DirectionalShadowArray, ShadowSampler, SampleCoords, refZ, filterSize, fGlobalAtlasWidth);

	return shadow0;

	// Compute Cascade Blend Factor
}


FLightingResult CalculateDynamicLightWithPCF_Directional(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
	float ViewSpaceZ,
    Texture2DArray<float> DirectionalShadowArray,
    FDirectionalCSMLightConstants DirectionalConstants,
    //FLightViewProj DirectionalShadowMatrix,
    SamplerComparisonState ShadowSampler,
    int FilterSize)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0)
    {
		ShadowFactor = SampleCSMShadow_PCF(DirectionalConstants, WorldPos, ViewSpaceZ,
			DirectionalShadowArray, ShadowSampler, Light.ShadowBias, FilterSize);
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//--------------------------------------------------------------------------------------
// PCF Shadow (Hard/Soft) - Spot Light
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithPCF_Spot(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
    Texture2DArray<float> SpotShadowAtlas,
    StructuredBuffer<FLightViewProj> SpotLightShadowMatrices,
    SamplerComparisonState ShadowSampler,
    int FilterSize)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
        FLightViewProj ViewProj = SpotLightShadowMatrices[Light.ShadowMapIndex];
        float3 uvw = GetSampleCoords(WorldPos, ViewProj.ViewMatrix, ViewProj.ProjectionMatrix);

    	uint GlobalAtlasWidth, GlobalAtlasHeight, NumSlices;
    	SpotShadowAtlas.GetDimensions(GlobalAtlasWidth, GlobalAtlasHeight, NumSlices);
    	float fGlobalAtlasWidth = (float)GlobalAtlasWidth;

    	// 2. UV 좌표에 라이트의 'ResolutionScale'을 곱하여 UV를 보정합니다.
    	uvw.xy = uvw.xy * Light.ShadowResolutionScale;

        float3 SampleCoords = float3(uvw.xy, Light.ShadowMapIndex);
        float RefZ = uvw.z - Light.ShadowBias;

        if (FilterSize <= 1)
        {
            // Hard shadow
            ShadowFactor = SpotShadowAtlas.SampleCmpLevelZero(ShadowSampler, SampleCoords, RefZ);
        }
        else
        {
            // Soft shadow (PCF)
            ShadowFactor = PCF_Texture2DArray(SpotShadowAtlas, ShadowSampler, SampleCoords, RefZ, FilterSize, fGlobalAtlasWidth);
        }
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//--------------------------------------------------------------------------------------
// PCF Shadow (Hard/Soft) - Point Light
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithPCF_Point(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
    TextureCubeArray<float> PointShadowAtlas,
    SamplerComparisonState ShadowSampler,
    int FilterSize)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
        float3 LightToPixelDir = WorldPos - Light.Position;
        float PixelDepth = length(LightToPixelDir) / Light.SourceRadius;
        float3 SwizzledDir = float3(LightToPixelDir.y, LightToPixelDir.z, LightToPixelDir.x);

        float4 ShadowCoord = float4(SwizzledDir, Light.ShadowMapIndex);
        float RefZ = PixelDepth - Light.ShadowBias;

        if (FilterSize <= 1)
        {
            // Hard shadow
            ShadowFactor = PointShadowAtlas.SampleCmpLevelZero(ShadowSampler, ShadowCoord, RefZ);
        }
        else
        {
            // Soft shadow (PCF - simplified for cube maps)
        	uint Width, Height, Slices;
        	PointShadowAtlas.GetDimensions(Width, Height, Slices);
        	float fResolution = (float)Width;

        	ShadowFactor = PCF_TextureCubeArray(PointShadowAtlas, ShadowSampler, ShadowCoord, RefZ, FilterSize, fResolution);        }
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//--------------------------------------------------------------------------------------
// VSM Shadow - Directional Light
//--------------------------------------------------------------------------------------

/*
 *
 *
FLightingResult CalculateDynamicLightWithVSM_Directional(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, float ViewSpaceZ,
    Texture2DArray<float2> DirectionalMomentsMap,
	Texture2DArray<float> DirectionalShadowArray,
    //FLightViewProj DirectionalShadowMatrix,
    SamplerState ShadowSampler,
    int FilterType, // 0=None, 1=Box, 2=Gaussian
    int FilterSize,
    float GaussianSigma)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0)
    {
		uint numCascade = DirNumCascades;

		int cascadeIdx = SelectCascadeIndex(ViewSpaceZ, numCascade);
		FLightViewProj viewProj = DirCascadeMatrices[cascadeIdx];

		float3 uvw = GetSampleCoords(WorldPos, viewProj.ViewMatrix, viewProj.ProjectionMatrix);


    // CRITICAL: Calculate view-space depth for VSM comparison
		float4 lightSpacePos = mul(float4(WorldPos, 1.0f), viewProj.ViewMatrix);
		float t = lightSpacePos.z; // View-space depth in light's coordinate system

    // Set cascade index for array sampling
		float3 sampleCoords = float3(uvw.xy, cascadeIdx);


		//uvw.z = cascadeIdx;
		//float t = mul(float4(WorldPos, 1.0f), viewProj.ViewMatrix).z; // uvw.z;

        float2 moments;
        // Ensure we sample the correct array slice (cascade)
        uvw.z = cascadeIdx;
        if (FilterType == 2) // Gaussian
        {
            moments = SampleVSMGaussian_Texture2D(DirectionalMomentsMap, ShadowSampler,
                                                  uvw, FilterSize / 2, GaussianSigma);
        }
        else if (FilterType == 1) // Box
        {
            moments = SampleVSMBox_Texture2D(DirectionalMomentsMap, ShadowSampler,
                                             uvw, FilterSize);
        }
        else // No filter
        {
            moments = DirectionalMomentsMap.Sample(ShadowSampler, uvw).xy;
        }

        ShadowFactor = VSM_Visibility(moments, t);
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

 */

FLightingResult CalculateDynamicLightWithVSM_Directional(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, float ViewSpaceZ,
    Texture2DArray<float2> DirectionalMomentsMap,
	Texture2DArray<float> DirectionalShadowArray, FDirectionalCSMLightConstants DirConstants,
    //FLightViewProj DirectionalShadowMatrix,
    SamplerState ShadowSampler,
    FShadowConstants ShadowSettings)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0)
    {
    	int cascadeIdx = SelectCascadeIndex(DirConstants, ViewSpaceZ, DirConstants.DirNumCascades);
    	FLightViewProj viewProj = DirConstants.DirCascadeMatrices[cascadeIdx];

    	float3 uvw = GetSampleCoords(WorldPos, viewProj.ViewMatrix, viewProj.ProjectionMatrix);
		 
    	float4 lightSpacePos = mul(float4(WorldPos, 1.0f), viewProj.ViewMatrix);
    	float t = lightSpacePos.z; 

    	float3 sampleCoords = float3(uvw.xy, cascadeIdx);
		
    	uint Width, Height, Idx;
    	DirectionalMomentsMap.GetDimensions(Width, Height, Idx);
    	float TexSize = (float)Width;

    	float2 moments;
    	if (ShadowSettings.FilterType == SHADOW_FILTER_VSM_GAUSSIAN) // Gaussian
    	{
			moments = SampleVSMGaussian(DirectionalMomentsMap, ShadowSampler, sampleCoords, Light.ShadowFilterSize,
    		Light.ShadowGaussSigma, TexSize);
    	}
    	else if (ShadowSettings.FilterType == SHADOW_FILTER_VSM_BOX)
    	{
			moments = SampleVSMBox(DirectionalMomentsMap, ShadowSampler, sampleCoords, Light.ShadowFilterSize, TexSize);
		}
	    else
	    {
			moments = DirectionalMomentsMap.Sample(ShadowSampler, sampleCoords).xy;
		}

        ShadowFactor = VSM_Visibility(moments, t, ShadowSettings.VSM_LightBleedReduction);
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//--------------------------------------------------------------------------------------
// VSM Shadow - Spot Light
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithVSM_Spot(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
    Texture2DArray<float2> SpotMomentsAtlas,
    StructuredBuffer<FLightViewProj> SpotLightShadowMatrices,
    SamplerState ShadowSampler,
    FShadowConstants ShadowSettings)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
        FLightViewProj ViewProj = SpotLightShadowMatrices[Light.ShadowMapIndex];
        float3 uvw = GetSampleCoords(WorldPos, ViewProj.ViewMatrix, ViewProj.ProjectionMatrix);

    	uint GlobalAtlasWidth, GlobalAtlasHeight, NumSlices;
    	SpotMomentsAtlas.GetDimensions(GlobalAtlasWidth, GlobalAtlasHeight, NumSlices);
    	float fGlobalAtlasWidth = (float)GlobalAtlasWidth;
        uvw.xy = uvw.xy * Light.ShadowResolutionScale;

        float3 SampleCoords = float3(uvw.xy, Light.ShadowMapIndex);
        float t = mul(float4(WorldPos, 1.0f), ViewProj.ViewMatrix).z; // uvw.z;

        float2 moments;
        if (ShadowSettings.FilterType == SHADOW_FILTER_VSM_GAUSSIAN) // Gaussian
        {
            moments = SampleVSMGaussian(SpotMomentsAtlas, ShadowSampler, SampleCoords,
            	Light.ShadowFilterSize, Light.ShadowGaussSigma, fGlobalAtlasWidth);
        }
        else if (ShadowSettings.FilterType == SHADOW_FILTER_VSM_BOX) // Box
        {
            moments = SampleVSMBox(SpotMomentsAtlas, ShadowSampler,
                                   SampleCoords, Light.ShadowFilterSize, fGlobalAtlasWidth);
        }
        else // No filter
        {
            moments = SpotMomentsAtlas.SampleLevel(ShadowSampler, SampleCoords, 0.0).xy;
        }

        ShadowFactor = VSM_Visibility(moments, t, ShadowSettings.VSM_LightBleedReduction);
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//--------------------------------------------------------------------------------------
// VSM Shadow - Point Light (LinearDepth moments)
//--------------------------------------------------------------------------------------

FLightingResult CalculateDynamicLightWithVSM_Point(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
    TextureCubeArray<float2> PointMomentsAtlas,
    SamplerState ShadowSampler,
	FShadowConstants ShadowSettings)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
        float3 LightToPixelDir = WorldPos - Light.Position;
        float LinearDepth = length(LightToPixelDir) / Light.SourceRadius;
        float3 SwizzledDir = float3(LightToPixelDir.y, LightToPixelDir.z, LightToPixelDir.x);

        float4 SampleCoords = float4(SwizzledDir, Light.ShadowMapIndex);

    	uint Width, Height, Slices;
    	PointMomentsAtlas.GetDimensions(Width, Height, Slices);
    	float Resolution = (float)Width;

        float2 moments = PointMomentsAtlas.SampleLevel(ShadowSampler, SampleCoords, 0.0).xy;

		if (ShadowSettings.FilterType == SHADOW_FILTER_VSM_GAUSSIAN) // Gaussian
		{
			moments = SampleVSMGaussian_Point(PointMomentsAtlas, ShadowSampler, SampleCoords,
				Light.ShadowFilterSize, Light.ShadowGaussSigma, Resolution);
		}
		else if (ShadowSettings.FilterType == SHADOW_FILTER_VSM_BOX) // Box
		{
			moments = SampleVSMBox_Point(PointMomentsAtlas, ShadowSampler,
								   SampleCoords, Light.ShadowFilterSize, Resolution);
		}
		else // No filter
		{
			moments = PointMomentsAtlas.SampleLevel(ShadowSampler, SampleCoords, 0.0).xy;
		}
        ShadowFactor = VSM_Visibility(moments, LinearDepth, ShadowSettings.VSM_LightBleedReduction);
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//==============================================================================
// Unified Entry Points - PCF/VSM 모두 지원
//==============================================================================

//--------------------------------------------------------------------------------------
// PCF Unified Entry Point
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithPCF(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, float ViewSpaceZ,
    // PCF Resources
    Texture2DArray<float> SpotShadowAtlas,
    StructuredBuffer<FLightViewProj> SpotLightShadowMatrices,
    TextureCubeArray<float> PointShadowAtlas,
    Texture2DArray<float> DirectionalShadowArray,
    FDirectionalCSMLightConstants DirectionalConstants,
    SamplerComparisonState ShadowSampler,
    int FilterSize)
{
    if (Light.LightType == LIGHT_TYPE_SPOT)
    {
        return CalculateDynamicLightWithPCF_Spot(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                 SpotShadowAtlas, SpotLightShadowMatrices,
                                                 ShadowSampler, FilterSize);
    }
    else if (Light.LightType == LIGHT_TYPE_POINT)
    {
        return CalculateDynamicLightWithPCF_Point(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                  PointShadowAtlas, ShadowSampler, FilterSize);
    }
    else if (Light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
       //return CalculateDynamicLightWithPCF_Directional(Light, WorldPos, Normal, ViewDir, SpecularPower,
       //                                                 DirectionalShadowMap, DirectionalShadowMatrix,
       //                                                 ShadowSampler, FilterSize);

		return CalculateDynamicLightWithPCF_Directional(Light, WorldPos, Normal, ViewDir, SpecularPower, ViewSpaceZ,
                                                        DirectionalShadowArray, DirectionalConstants,
                                                        ShadowSampler, FilterSize);
	}

    // No shadow
    return CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
}

//--------------------------------------------------------------------------------------
// Hard Shadow Entry Point (FilterSize = 1)
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithShadows(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, float ViewSpaceZ,
    Texture2DArray<float> SpotShadowAtlas,
    StructuredBuffer<FLightViewProj> SpotLightShadowMatrices,
    TextureCubeArray<float> PointShadowAtlas,
    Texture2DArray<float> DirectionalShadowArray,
    FDirectionalCSMLightConstants DirectionalConstants,
    SamplerComparisonState ShadowSampler)
{
	return CalculateDynamicLightWithPCF(Light, WorldPos, Normal, ViewDir, SpecularPower, ViewSpaceZ,
                                        SpotShadowAtlas, SpotLightShadowMatrices, PointShadowAtlas,
                                        DirectionalShadowArray, DirectionalConstants, ShadowSampler, 1);
}

//--------------------------------------------------------------------------------------
// VSM Unified Entry Point (ViewSpace Z depth 사용)
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithVSM(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, float ViewSpaceZ,
    // VSM Resources
    Texture2DArray<float2> SpotMomentsAtlas,
    StructuredBuffer<FLightViewProj> SpotLightShadowMatrices,
    TextureCubeArray<float2> PointMomentsAtlas,
    Texture2DArray<float2> DirectionalMomentsMap,
    Texture2DArray<float> DirectionalShadowArray,
    FDirectionalCSMLightConstants DirectionalConstants,
    SamplerState ShadowSampler,
    FShadowConstants ShadowSettings)
{
    if (Light.LightType == LIGHT_TYPE_SPOT)
    {
        return CalculateDynamicLightWithVSM_Spot(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                 SpotMomentsAtlas, SpotLightShadowMatrices, ShadowSampler, ShadowSettings);
    }
    if (Light.LightType == LIGHT_TYPE_POINT)
    {

		return CalculateDynamicLightWithVSM_Point(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                   PointMomentsAtlas, ShadowSampler, ShadowSettings);

    }
    if (Light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        return CalculateDynamicLightWithVSM_Directional(Light, WorldPos, Normal, ViewDir, SpecularPower, ViewSpaceZ,
                                                        DirectionalMomentsMap, DirectionalShadowArray, DirectionalConstants,
                                                        ShadowSampler, ShadowSettings);
    }

    // No shadow
    return CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
}

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : TEXCOORD0;
	float3 WorldNormal : NORMAL;
	float2 Tex : TEXCOORD2;
	float3 WorldTangent : TEXCOORD3;
	float TangentSign : TEXCOORD4;
	float3 TotalAmbient : COLOR0;
	float3 TotalDiffuse : COLOR1;
	float3 TotalSpecular : COLOR2;
};

#endif // LIGHTING_FUNCTIONS_HLSL
