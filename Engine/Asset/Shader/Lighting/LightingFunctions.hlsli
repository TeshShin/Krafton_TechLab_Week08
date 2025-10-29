//--------------------------------------------------------------------------------------
// [LIGHTING FUNCTIONS] Common Lighting Calculations
// This file contains reusable lighting functions for both vertex and pixel shaders
//--------------------------------------------------------------------------------------

#ifndef LIGHTING_FUNCTIONS_HLSL
#define LIGHTING_FUNCTIONS_HLSL

//--------------------------------------------------------------------------------------
// [VSM FILTER]
//--------------------------------------------------------------------------------------
// Filter selection (mutually exclusive):
// - Set VSM_USE_GAUSSIAN_FILTER to 1 to use Gaussian; otherwise falls back to box or linear.
// - If both are 0, uses unfiltered linear sampling of moments.
#ifndef VSM_USE_BOX_FILTER
#define VSM_USE_BOX_FILTER 1
#endif

#ifndef VSM_USE_GAUSSIAN_FILTER
#define VSM_USE_GAUSSIAN_FILTER 0
#endif

#ifndef BOX_FILTER_SIZE
#define BOX_FILTER_SIZE 11
#endif

// Gaussian settings (radius in texels; kernel size = 2*R+1)
#ifndef GAUSSIAN_KERNEL_RADIUS
#define GAUSSIAN_KERNEL_RADIUS 10
#endif

#ifndef GAUSSIAN_SIGMA
#define GAUSSIAN_SIGMA 5.0f
#endif

// Light bleeding reduction amount for VSM (0 = none, 1 = full cutoff)
#ifndef VSM_LIGHT_BLEED_REDUCTION
#define VSM_LIGHT_BLEED_REDUCTION 0.5f
#endif



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

cbuffer DirectionalCSMLightConstants : register(b4)
{
	uint DirNumCascades;
	float3 CSMPadding;

	float DirCascadeSplits[12];
	FLightViewProj DirCascadeMatrices[12];
}

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
    float Param2;               // 4 bytes  - Reserved for future use
    uint LightType;             // 4 bytes  - Light type identifier
	float ShadowBias;            // 4 bytes - Shadow Bias
	uint bCastShadows;         // 4 bytes - Light Does Cast Shadows
	int ShadowMapIndex;        // 4 bytes - Shadow Texture2D Array Index
	float Padding;				// 4 bytes - four Byte
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
          ShadowFactor = SpotShadowAtlas.SampleCmp(ShadowSampler, SampleCoords, ShadowCoords.z - Light.ShadowBias);
       }
       else if (Light.LightType == LIGHT_TYPE_POINT)
       {
          // 픽셀에서 라이트까지의 선형 깊이 계산
          float3 LightToPixelDir = WorldPos - Light.Position;
          float PixelDepth = length(LightToPixelDir) / Light.SourceRadius;
       	  float3 SwizzledDir = float3(LightToPixelDir.y, LightToPixelDir.z, LightToPixelDir.x);

          float4 ShadowCoord = float4(SwizzledDir, Light.ShadowMapIndex);
          // (선형 깊이) vs (섀도우맵에 저장된 선형 깊이) 비교
          ShadowFactor = PointShadowAtlas.SampleCmp(ShadowSampler, ShadowCoord, PixelDepth - Light.ShadowBias);
       }
       else if (Light.LightType == LIGHT_TYPE_DIRECTIONAL)
       {
       	  FLightViewProj ViewProj = DirectionalShadowMatrix;
       	  float4 LightSpacePos = mul(mul(float4(WorldPos, 1.0f), ViewProj.ViewMatrix), ViewProj.ProjectionMatrix);
          float3 ShadowCoords = LightSpacePos.xyz / LightSpacePos.w;

          ShadowCoords.x = ShadowCoords.x * 0.5f + 0.5f;
          ShadowCoords.y = ShadowCoords.y * -0.5f + 0.5f;

          float2 SampleCoords = ShadowCoords.xy;

          ShadowFactor = DirectionalTexture.SampleCmp(ShadowSampler, SampleCoords, ShadowCoords.z - Light.ShadowBias);
       }
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;

    return Result;
}
 
//--------------------------------------------------------------------------------------
// CSM Helper Functions
//--------------------------------------------------------------------------------------
uint SelectCascadeIndex(float viewSpaceZ, uint numCascades)
{
	for (uint i = 0; i < numCascades; ++i)
	{
		if (viewSpaceZ <= DirCascadeSplits[i + 1])
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
                         float3 uvw, float RefZ, int FilterSize)
{
    float2 texSize;
    int index;
    ShadowMap.GetDimensions(texSize.x, texSize.y, index);

    int R = FilterSize / 2;
    float ShadowSamples = 0.0;

    float2 st = floor(uvw.xy * texSize);

    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            float2 uv = (st + float2(x, y) + 0.5) / texSize;
            ShadowSamples += ShadowMap.SampleCmp(Sampler, float3(uv, uvw.z), RefZ);
        }
    }

    return ShadowSamples / ((2 * R + 1) * (2 * R + 1));
}

float PCF_Texture2D(Texture2D<float> ShadowMap, SamplerComparisonState Sampler,
                    float2 uv, float RefZ, int FilterSize)
{
    float2 texSize;
    ShadowMap.GetDimensions(texSize.x, texSize.y);

    int R = FilterSize / 2;
    float ShadowSamples = 0.0;

    float2 st = floor(uv * texSize);

    for (int y = -R; y <= R; ++y)
    {
        for (int x = -R; x <= R; ++x)
        {
            float2 sampleUV = (st + float2(x, y) + 0.5) / texSize;
            ShadowSamples += ShadowMap.SampleCmp(Sampler, sampleUV, RefZ);
        }
    }

    return ShadowSamples / ((2 * R + 1) * (2 * R + 1));
}

float PCF_TextureCubeArray(TextureCubeArray<float> ShadowMap, SamplerComparisonState Sampler,
                           float4 uvw, float RefZ, int FilterSize)
{
    // Cube map PCF는 direction 기반이므로 간단한 샘플링만 수행
    // 정확한 PCF를 위해서는 tangent space에서 offset을 계산해야 하지만
    // 여기서는 단순화된 버전 사용
    return ShadowMap.SampleCmp(Sampler, uvw, RefZ);
}

//==============================================================================
// VSM (Variance Shadow Mapping) - 선형 필터
//==============================================================================

// M1 = E(x), M2 = E(x^2)
float VSM_Visibility(float2 Moments, float t)
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
    #ifndef VSM_LIGHT_BLEED_REDUCTION
    #define VSM_LIGHT_BLEED_REDUCTION 0.3f
    #endif
    pMax = ReduceLightBleeding(pMax, VSM_LIGHT_BLEED_REDUCTION);

    return saturate(pMax);
}

//--------------------------------------------------------------------------------------
// [VSM] Box-filtered
//--------------------------------------------------------------------------------------
float2 SampleVSMBox(Texture2DArray<float2> ShadowMomentsArray, SamplerState Sampler,
                    float3 uvw, int KernelSize)
{
    int radius = max(0, (KernelSize & 1) ? KernelSize / 2 : (KernelSize - 1) / 2);

    uint2 texSize;
    float element;
    ShadowMomentsArray.GetDimensions(texSize.x, texSize.y, element);

    float2 st = texSize * uvw.xy;
    float2 sum = float2(0.0, 0.0);
    int count = (2 * radius + 1) * (2 * radius + 1);

    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            float2 uv = (st + float2(i, j) + 0.5f) / texSize;
            sum += ShadowMomentsArray.Sample(Sampler, float3(uv, uvw.z)).xy;
        }
    }
    return (count > 0) ? (sum / count) : ShadowMomentsArray.Sample(Sampler, uvw).xy;
}
float2 SampleVSMBox_Point(TextureCubeArray<float2> ShadowMomentsArray, SamplerState Sampler,
                    float4 dir, int KernelSize)
{
    int radius = max(0, (KernelSize & 1) ? KernelSize / 2 : (KernelSize - 1) / 2);

    float Width, Height;
    float element;
    ShadowMomentsArray.GetDimensions(Width, Height, element);
	
	float3 T, B;
	BuildOrthonormalBasis(normalize(dir.xyz), T, B); 

	float TexelAngle = 1 / Width;
	
    float2 sum = float2(0.0, 0.0);
    int count = (2 * radius + 1) * (2 * radius + 1);

    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
			float2 angle = TexelAngle * float2(x, y);
			
            float3 dOffset = (dir.xyz + angle.x * T + angle.y * B);
            sum += ShadowMomentsArray.Sample(Sampler, float4(dOffset, dir.w)).xy;
        }
    }
    return (count > 0) ? (sum / count) : ShadowMomentsArray.Sample(Sampler, dir).xy;
}

float2 SampleVSMBox_Texture2D(Texture2D<float2> ShadowMoments, SamplerState Sampler,
                              float2 uv, int KernelSize)
{
    int radius = max(0, (KernelSize & 1) ? KernelSize / 2 : (KernelSize - 1) / 2);

    uint2 texSize;
    ShadowMoments.GetDimensions(texSize.x, texSize.y);

    float2 st = texSize * uv;
    float2 sum = float2(0.0, 0.0);
    int count = (2 * radius + 1) * (2 * radius + 1);

    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            float2 sampleUV = (st + float2(i, j) + 0.5f) / texSize;
            sum += ShadowMoments.Sample(Sampler, sampleUV).xy;
        }
    }
    return (count > 0) ? (sum / count) : ShadowMoments.Sample(Sampler, uv).xy;
}

//--------------------------------------------------------------------------------------
// [VSM] Gaussian-filtered
//--------------------------------------------------------------------------------------
float2 SampleVSMGaussian(Texture2DArray<float2> ShadowMomentsArray, SamplerState Sampler,
                         float3 uvw, int radius, float sigma)
{
    radius = max(0, radius);

    uint2 texSize;
    float element;
    ShadowMomentsArray.GetDimensions(texSize.x, texSize.y, element);

    float2 st = texSize * uvw.xy;

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

            float2 uv = (st + float2(x, y) + 0.5f) / texSize;
            acc += w * ShadowMomentsArray.Sample(Sampler, float3(uv, uvw.z)).xy;
        }
    }

    return acc / norm2D;
}

float2 SampleVSMGaussian_Point(TextureCubeArray<float2>ShadowMomentsArray, SamplerState Sampler,
                         float4 dir, int radius, float sigma)
{
	radius = max(0, radius);

	uint Width, Height, Elements; 
	ShadowMomentsArray.GetDimensions(Width, Height, Elements);

	float texelAngle = 1.0f / Width;
	
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
			acc += w * ShadowMomentsArray.Sample(Sampler, float4(dOff, dir.w)).xy;
		}
	}

	return acc / norm2D;
}

float2 SampleVSMGaussian_Texture2D(Texture2D<float2> ShadowMoments, SamplerState Sampler,
                                   float2 uv, int radius, float sigma)
{
    radius = max(0, radius);

    uint2 texSize;
    ShadowMoments.GetDimensions(texSize.x, texSize.y);

    float2 st = texSize * uv;

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

            float2 sampleUV = (st + float2(x, y) + 0.5f) / texSize;
            acc += w * ShadowMoments.Sample(Sampler, sampleUV).xy;
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


float SampleCSMShadow_PCF(
   float3 worldPos,
    float viewSpaceZ,
    Texture2DArray<float> DirectionalShadowArray,
    SamplerComparisonState ShadowSampler,
    float shadowBias,
    int filterSize)
{
	uint numCascade = DirNumCascades;
	if (numCascade == 0)
		return 1.0f;

	// Select Cascade Idx 
	uint cascadeIdx = SelectCascadeIndex(viewSpaceZ, numCascade);

	FLightViewProj viewProj = DirCascadeMatrices[cascadeIdx];
	
	float3 uvw = GetSampleCoords(worldPos, viewProj.ViewMatrix, viewProj.ProjectionMatrix);
	float3 SampleCoords = float3(uvw.xy, cascadeIdx);
	float refZ = uvw.z - shadowBias;

	float shadow0 = (filterSize <= 1)
        ? DirectionalShadowArray.SampleCmp(ShadowSampler, SampleCoords, refZ)
        : PCF_Texture2DArray(DirectionalShadowArray, ShadowSampler, SampleCoords, refZ, filterSize);

	return shadow0;
	
	// Compute Cascade Blend Factor
}


FLightingResult CalculateDynamicLightWithPCF_Directional(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
	float ViewSpaceZ,
    Texture2DArray<float> DirectionalShadowArray,
    //FLightViewProj DirectionalShadowMatrix,
    SamplerComparisonState ShadowSampler,
    int FilterSize)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0)
    {
		// CSM도 껐다 켰는게 가능하도록 수정..?
		ShadowFactor = SampleCSMShadow_PCF(
            WorldPos, ViewSpaceZ,
            DirectionalShadowArray, ShadowSampler,
            Light.ShadowBias, FilterSize);
		 
		// CSM적용 전 코드
        //float3 uvw = GetSampleCoords(WorldPos, DirectionalShadowMatrix.ViewMatrix,
        //                              DirectionalShadowMatrix.ProjectionMatrix);
        //float RefZ = uvw.z - Light.ShadowBias;
		//
		//if (FilterSize <= 1)
        //{
        //    // Hard shadow
        //    ShadowFactor = DirectionalShadowMap.SampleCmp(ShadowSampler, uvw.xy, RefZ);
        //}
        //else
        //{
        //    // Soft shadow (PCF)
        //    ShadowFactor = PCF_Texture2D(DirectionalShadowMap, ShadowSampler, uvw.xy, RefZ, FilterSize);
        //}
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
        float3 SampleCoords = float3(uvw.xy, Light.ShadowMapIndex);
        float RefZ = uvw.z - Light.ShadowBias;

        if (FilterSize <= 1)
        {
            // Hard shadow
            ShadowFactor = SpotShadowAtlas.SampleCmp(ShadowSampler, SampleCoords, RefZ);
        }
        else
        {
            // Soft shadow (PCF)
            ShadowFactor = PCF_Texture2DArray(SpotShadowAtlas, ShadowSampler, SampleCoords, RefZ, FilterSize);
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

    	ShadowFactor = PointShadowAtlas.SampleCmp(ShadowSampler, ShadowCoord, RefZ);

        // if (FilterSize <= 1)
        // {
        //     // Hard shadow
        //     ShadowFactor = PointShadowAtlas.SampleCmp(ShadowSampler, ShadowCoord, RefZ);
        // }
        // else
        // {
        //     // Soft shadow (PCF - simplified for cube maps)
        //     ShadowFactor = PCF_TextureCubeArray(PointShadowAtlas, ShadowSampler, ShadowCoord, RefZ, FilterSize);
        // }
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}

//--------------------------------------------------------------------------------------
// VSM Shadow - Directional Light
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithVSM_Directional(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
    Texture2D<float2> DirectionalMomentsMap,
    FLightViewProj DirectionalShadowMatrix,
    SamplerState ShadowSampler,
    int FilterType, // 0=None, 1=Box, 2=Gaussian
    int FilterSize,
    float GaussianSigma)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0)
    {
        float3 uvw = GetSampleCoords(WorldPos, DirectionalShadowMatrix.ViewMatrix,
                                      DirectionalShadowMatrix.ProjectionMatrix);
    	float t = mul(float4(WorldPos, 1.0f), DirectionalShadowMatrix.ViewMatrix).z; // uvw.z;

        float2 moments;
        if (FilterType == 2) // Gaussian
        {
            moments = SampleVSMGaussian_Texture2D(DirectionalMomentsMap, ShadowSampler,
                                                  uvw.xy, FilterSize / 2, GaussianSigma);
        }
        else if (FilterType == 1) // Box
        {
            moments = SampleVSMBox_Texture2D(DirectionalMomentsMap, ShadowSampler,
                                             uvw.xy, FilterSize);
        }
        else // No filter
        {
            moments = DirectionalMomentsMap.Sample(ShadowSampler, uvw.xy).xy;
        }

        ShadowFactor = VSM_Visibility(moments, t);
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
    int FilterType, // 0=None, 1=Box, 2=Gaussian
    int FilterSize,
    float GaussianSigma)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
        FLightViewProj ViewProj = SpotLightShadowMatrices[Light.ShadowMapIndex];
        float3 uvw = GetSampleCoords(WorldPos, ViewProj.ViewMatrix, ViewProj.ProjectionMatrix);
        float3 SampleCoords = float3(uvw.xy, Light.ShadowMapIndex);
        float t = mul(float4(WorldPos, 1.0f), ViewProj.ViewMatrix).z; // uvw.z;

        float2 moments;
        if (FilterType == 2) // Gaussian
        {
            moments = SampleVSMGaussian(SpotMomentsAtlas, ShadowSampler,
                                        SampleCoords, FilterSize / 2, GaussianSigma);
        }
        else if (FilterType == 1) // Box
        {
            moments = SampleVSMBox(SpotMomentsAtlas, ShadowSampler,
                                   SampleCoords, FilterSize);
        }
        else // No filter
        {
            moments = SpotMomentsAtlas.Sample(ShadowSampler, SampleCoords).xy;
        }

        ShadowFactor = VSM_Visibility(moments, t);
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
	int FilterType,
	int FilterSize,
	float GaussianSigma)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
    float ShadowFactor = 1.0f;

    if (Light.bCastShadows > 0 && Light.ShadowMapIndex >= 0)
    {
        float3 LightToPixelDir = WorldPos - Light.Position;
        float LinearDepth = length(LightToPixelDir) / Light.SourceRadius;
        float3 SwizzledDir = float3(LightToPixelDir.y, LightToPixelDir.z, LightToPixelDir.x);

        float4 SampleCoord = float4(SwizzledDir, Light.ShadowMapIndex);
        float2 moments = PointMomentsAtlas.Sample(ShadowSampler, SampleCoord).xy;
		 
		if (FilterType == 2) // Gaussian
		{
			moments = SampleVSMGaussian_Point(PointMomentsAtlas, ShadowSampler,
                                       SampleCoord, FilterSize / 2, GaussianSigma);
		}
		else if (FilterType == 1) // Box
		{
			moments = SampleVSMBox_Point(PointMomentsAtlas, ShadowSampler,
                                   SampleCoord, FilterSize);
			}
		else // No filter
		{
			moments = PointMomentsAtlas.Sample(ShadowSampler, SampleCoord).xy;

		}
        ShadowFactor = VSM_Visibility(moments, LinearDepth);
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
    //FLightViewProj DirectionalShadowMatrix,
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
		 
		return CalculateDynamicLightWithPCF_Directional(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                        ViewSpaceZ, DirectionalShadowArray,
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
    Texture2DArray<float> DirectionalShadowMap,
    //FLightViewProj DirectionalShadowMatrix,
    SamplerComparisonState ShadowSampler)
{
	return CalculateDynamicLightWithPCF(Light, WorldPos, Normal, ViewDir, SpecularPower, ViewSpaceZ,
                                        SpotShadowAtlas, SpotLightShadowMatrices,
                                        PointShadowAtlas, DirectionalShadowMap,
                                        ShadowSampler, 1);
}

//--------------------------------------------------------------------------------------
// VSM Unified Entry Point (ViewSpace Z depth 사용)
//--------------------------------------------------------------------------------------
FLightingResult CalculateDynamicLightWithVSM(
    FUnifiedDynamicLight Light,
    float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
    // VSM Resources
    Texture2DArray<float2> SpotMomentsAtlas,
    StructuredBuffer<FLightViewProj> SpotLightShadowMatrices,
    TextureCubeArray<float2> PointMomentsAtlas,
    Texture2D<float2> DirectionalMomentsMap,
    FLightViewProj DirectionalShadowMatrix,
    SamplerState ShadowSampler,
    float ViewSpaceDepth) // ViewSpace Z (from shader)
{
    #ifndef VSM_FILTER_TYPE
    #define VSM_FILTER_TYPE 0 // 0=None, 1=Box, 2=Gaussian
    #endif

    #ifndef VSM_FILTER_SIZE
    #define VSM_FILTER_SIZE 5
    #endif

    #ifndef VSM_GAUSSIAN_SIGMA
    #define VSM_GAUSSIAN_SIGMA 2.0f
    #endif

    if (Light.LightType == LIGHT_TYPE_SPOT)
    {
        return CalculateDynamicLightWithVSM_Spot(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                 SpotMomentsAtlas, SpotLightShadowMatrices,
                                                 ShadowSampler, VSM_FILTER_TYPE, VSM_FILTER_SIZE, VSM_GAUSSIAN_SIGMA);
    }
    if (Light.LightType == LIGHT_TYPE_POINT)
    {
	
		return CalculateDynamicLightWithVSM_Point(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                   PointMomentsAtlas, ShadowSampler, VSM_FILTER_TYPE, VSM_FILTER_SIZE, VSM_GAUSSIAN_SIGMA);
		
    }
    if (Light.LightType == LIGHT_TYPE_DIRECTIONAL)
    {
        return CalculateDynamicLightWithVSM_Directional(Light, WorldPos, Normal, ViewDir, SpecularPower,
                                                        DirectionalMomentsMap, DirectionalShadowMatrix,
                                                        ShadowSampler, VSM_FILTER_TYPE, VSM_FILTER_SIZE, VSM_GAUSSIAN_SIGMA);
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
