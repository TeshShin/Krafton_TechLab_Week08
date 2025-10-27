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
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1
#define LIGHT_TYPE_SPOT        2
#define LIGHT_TYPE_AMBIENT     3

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
	row_major float4x4 LightView; // 64 bytes - Light View Matrix
	row_major float4x4 LightProjection; // 64 bytes - Light Projection Matrix
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
 

// ndc좌표계로 변환해서 UV좌표계를 얻어주는 helper function 
float3 GetSampleCoords(float3 WorldPos, float4x4 VPMatrix)
{
	float4 LightClipSpace = mul(float4(WorldPos, 1.0f), VPMatrix);
	float3 uvw = LightClipSpace.xyz / LightClipSpace.w;
	
	uvw.x = uvw.x * 0.5f + 0.5f;
	uvw.y = uvw.y * -0.5f + 0.5f;
	
	return float3(uvw.xyz);
}

// Percentage Closer Filter
float PCF(Texture2DArray<float> ShadowMap, SamplerComparisonState Sampler, float3 uvw, float RefZ, int FilterSize)
{ 
	float2 texSize;;
	int index;
	ShadowMap.GetDimensions(texSize.x, texSize.y, index);
	 
	int R = FilterSize / 2;
	float ShadowSamples= 0.0;
	 
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

	// Chebyshev(Cantelli ) upper bound term
	float d = t - Moments.x; 
    float pMax = variance / (d * d + variance);

    // Reduce light bleeding
    pMax = ReduceLightBleeding(pMax, VSM_LIGHT_BLEED_REDUCTION);

    return saturate(pMax);
}

//--------------------------------------------------------------------------------------
// [VSM] Box-filtered
//--------------------------------------------------------------------------------------
float2 SampleVSMBox(Texture2DArray<float2> ShadowMomentsArray,
                    SamplerState Sampler,
                    float3 uvw,
                    int KernelSize)
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

//--------------------------------------------------------------------------------------
// VSM: 선형필터와 결합하기 위해, 선형으로 depth 비교
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// [VSM] Gaussian-filtered (2D via separable 1D weights)
// - Computes 1D Gaussian weights on-the-fly; uses product to form 2D kernel
// - radius: kernel radius in texels (kernel size = 2*radius+1)
// - sigma: standard deviation in texels
//--------------------------------------------------------------------------------------
float2 SampleVSMGaussian(
    Texture2DArray<float2> ShadowMomentsArray,
    SamplerState Sampler,
    float3 uvw,
    int radius,
    float sigma)
{
    radius = max(0, radius);

    uint2 texSize;
    float element;
    ShadowMomentsArray.GetDimensions(texSize.x, texSize.y, element);

    float2 st = texSize * uvw.xy;

    // Precompute 1D normalization sum: S = sum_{i=-r..r} g(i)
    float oneDsum = 0.0f;
    [loop]
    for (int i = -radius; i <= radius; ++i)
    {
        float x = i / sigma;
        // Unnormalized Gaussian (normalization handled by dividing by (oneDsum^2))
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
            float w = wx * wy; // separable 2D weight

            float2 uv = (st + float2(x, y) + 0.5f) / texSize;
            acc += w * ShadowMomentsArray.Sample(Sampler, float3(uv, uvw.z)).xy;
        }
    }

    return acc / norm2D;
}

// VSM + Gaussian Filter
float ShadowVisibilityVSM_GaussianFiltered(
    Texture2DArray<float2> ShadowMomentsArray,
    SamplerState ShadowSampler,
    float3 sampleUVW,
    float t,
    int radius,
    float sigma)
{
    float2 moments = SampleVSMGaussian(ShadowMomentsArray, ShadowSampler, sampleUVW, radius, sigma);
    return VSM_Visibility(moments, t);
}

float ShadowVisibilityVSM(Texture2DArray<float2> ShadowMomentsArray,
						   SamplerState ShadowSampler,
						   float3 sampleUVW,
						   float t /* light-view linear/normalized depth */)
{
	float2 moments = ShadowMomentsArray.Sample(ShadowSampler, sampleUVW).xy;
	return VSM_Visibility(moments, t);
}  

// VSM + Box Filter 
float ShadowVisibilityVSM_BoxFiltered(Texture2DArray<float2> ShadowMomentsArray,
                                      SamplerState ShadowSampler,
                                      float3 sampleUVW,
                                      float t,
                                      int KernelSize)
{
    float2 moments = SampleVSMBox(ShadowMomentsArray, ShadowSampler, sampleUVW, KernelSize);
    return VSM_Visibility(moments, t);
}


// Shadow + VSM(선형 필터) 
FLightingResult CalculateDynamicLightWithVSM(FUnifiedDynamicLight Light,
	float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, Texture2DArray<float2> ShadowMapArray, SamplerState ShadowSampler, float t)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);
	 
	float3 SampleCoords = GetSampleCoords(WorldPos, mul(Light.LightView, Light.LightProjection));
 
	float ShadowFactor = 1.0f;
	if (Light.bCastShadows > 0)
	{
		#if VSM_USE_GAUSSIAN_FILTER
		ShadowFactor = ShadowVisibilityVSM_GaussianFiltered(
			ShadowMapArray, ShadowSampler, float3(SampleCoords.xy, Light.ShadowMapIndex), t,
			GAUSSIAN_KERNEL_RADIUS, GAUSSIAN_SIGMA);
		#elif VSM_USE_BOX_FILTER
		ShadowFactor = ShadowVisibilityVSM_BoxFiltered(ShadowMapArray, ShadowSampler, float3(SampleCoords.xy, Light.ShadowMapIndex), t, BOX_FILTER_SIZE);
		#else
		ShadowFactor = ShadowVisibilityVSM(ShadowMapArray, ShadowSampler, SampleCoords, t);
		#endif
	}

	Result.Diffuse *= ShadowFactor;
	Result.Specular *= ShadowFactor;

    return Result;
}

// Shadow + PCF(비선형 필터) 
FLightingResult CalculateDynamicLightWithPCF(
	FUnifiedDynamicLight Light,
	float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
	Texture2DArray<float> ShadowMomentsArray, SamplerComparisonState ShadowSampler)
{
	FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);

	float ShadowFactor = 1.0f;
	if (Light.bCastShadows > 0)
	{

		float3 uvw= GetSampleCoords(WorldPos, mul(Light.LightView, Light.LightProjection));
		 
		float t = uvw.z - Light.ShadowBias;  

		ShadowFactor = PCF(ShadowMomentsArray, ShadowSampler, float3(uvw.xy, Light.ShadowMapIndex), t, 11);
	}

	Result.Diffuse *= ShadowFactor;
	Result.Specular *= ShadowFactor;
	return Result;
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

//--------------------------------------------------------------------------------------
// 어떤 Filter도 없는 Hard Shadow
//--------------------------------------------------------------------------------------
FLightingResult HardShadow(
    FUnifiedDynamicLight Light,
    float3 WorldPos,
    float3 Normal,
    float3 ViewDir,
    float SpecularPower,
    Texture2DArray<float> ShadowMapArray,
    SamplerState ShadowSampler)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);

    float ShadowFactor = 1.0f;
    if (Light.bCastShadows > 0)
    { 
		float3 SampleCoords = GetSampleCoords(WorldPos, mul(Light.LightView, Light.LightProjection));
		
		float Depth = SampleCoords.z - Light.ShadowBias;

		float SampleDepth = ShadowMapArray.SampleLevel(ShadowSampler, float3(SampleCoords.xy, Light.ShadowMapIndex), 0).r;

		ShadowFactor = (Depth <= SampleDepth) ? 1.0f : 0.0f; 
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}
#endif // LIGHTING_FUNCTIONS_HLSL



