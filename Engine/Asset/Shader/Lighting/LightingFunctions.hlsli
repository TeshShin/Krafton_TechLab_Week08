//--------------------------------------------------------------------------------------
// [LIGHTING FUNCTIONS] Common Lighting Calculations
// This file contains reusable lighting functions for both vertex and pixel shaders
//--------------------------------------------------------------------------------------

#ifndef LIGHTING_FUNCTIONS_HLSL
#define LIGHTING_FUNCTIONS_HLSL

//--------------------------------------------------------------------------------------
// [VSM FILTER CONFIG]
//--------------------------------------------------------------------------------------
// Set to 1 to use a simple box filter over the VSM moments when sampling.
#ifndef VSM_USE_BOX_FILTER
#define VSM_USE_BOX_FILTER 1
#endif

// Box filter kernel size (must be odd). Typical values: 3, 5, 7, 9, 11
#ifndef VSM_FILTER_SIZE
#define VSM_FILTER_SIZE 35
#endif

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
	// m1(湲곕낯 depth)? 鍮꾧탳 癒쇱?, 洹몃┝??or 鍮??먮떒
	if (t <= Moments.x)
	{
		return 1.0f;
	}

	// Variance = E(z^2) - (E(z))^2
	float variance = Moments.y - (Moments.x * Moments.x);
	// Prevent negative/too small variance (light bleeding control)
	const float MinVariance = 1e-5f;

	// Chebyshev/Cantelli upper bound term
	float d = t - Moments.x; 
	float pMax = variance / (d * d + variance);

	// Light bleeding reduction (optional, tune Amount between 0-1)
	//const float LightBleedReduction = 0.2f;
	//pMax = smoothstep(LightBleedReduction, 1.0f, pMax);
	
	return saturate(pMax);
}

//--------------------------------------------------------------------------------------
// [VSM] Box-filtered moments sampling
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

// Sample VSM moments from Texture2DArray<float2> 
//  일반 SamplerState (non-comparison)를 사용하자
float ShadowVisibilityVSM(Texture2DArray<float2> ShadowMomentsArray,
						   SamplerState ShadowSampler,
						   float3 sampleUVW,
						   float t /* light-view linear/normalized depth */)
{
	float2 moments = ShadowMomentsArray.Sample(ShadowSampler, sampleUVW).xy;
	return VSM_Visibility(moments, t);
}  

// Box-filtered variant: averages moments over an NxN kernel, then evaluates VSM visibility
float ShadowVisibilityVSM_BoxFiltered(Texture2DArray<float2> ShadowMomentsArray,
                                      SamplerState ShadowSampler,
                                      float3 sampleUVW,
                                      float t,
                                      int KernelSize)
{
    float2 moments = SampleVSMBox(ShadowMomentsArray, ShadowSampler, sampleUVW, KernelSize);
    return VSM_Visibility(moments, t);
}

FLightingResult CalculateDynamicLightWithShadows(FUnifiedDynamicLight Light,
	float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower, Texture2DArray<float2> ShadowMapArray, SamplerState ShadowSampler, float3 SampleCoords, float t)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);

	float ShadowFactor = 1.0f;
	if (Light.bCastShadows > 0)
	{
		
		// Compute receiver depth normalized to match stored moments 
		// PCF path (hardware depth comparison)
		// Texture2D format + sampler State ?섎닃 ?꾩슂媛 ?덈떎 .
		//ShadowFactor = PCF(ShadowMapArray, ShadowSampler, SampleCoords, (ShadowCoords.z - Light.ShadowBias), 11);
		#if VSM_USE_BOX_FILTER
		ShadowFactor = ShadowVisibilityVSM_BoxFiltered(ShadowMapArray, ShadowSampler, SampleCoords, t, VSM_FILTER_SIZE);
		#else
		ShadowFactor = ShadowVisibilityVSM(ShadowMapArray, ShadowSampler, SampleCoords, t);
		#endif
	}

	Result.Diffuse *= ShadowFactor;
	Result.Specular *= ShadowFactor;

    return Result;
}

// Overload: VSM-based shadowing using RG moments texture array
// - ShadowMomentsArray: Texture2DArray storing (E[z], E[z^2]) per texel
// - ShadowSampler: regular sampler (non-comparison)
FLightingResult CalculateDynamicLightWithShadows2(
	FUnifiedDynamicLight Light,
	float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
	Texture2DArray<float> ShadowMomentsArray, SamplerComparisonState ShadowSampler)
{
	FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);

	float ShadowFactor = 1.0f;
	if (Light.bCastShadows > 0)
	{
		float4 lightPosH = mul(mul(float4(WorldPos, 1.0f), Light.LightView), Light.LightProjection);
		float3 uvw = lightPosH.xyz / lightPosH.w;

		// Project to texture space
		uvw.x = uvw.x * 0.5f + 0.5f;
		uvw.y = uvw.y * -0.5f + 0.5f;

		// Receiver depth t in same normalization as stored z
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
// [SHADOWING] Single-sample hard shadow from 2D array depth map
// - Samples one texel and does a binary compare (no filtering)
// - Expects ShadowMapArray to be a depth SRV (or R32F of clip-space depth)
// - Uses regular sampler and manual comparison to avoid requiring a comparison sampler
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
        // Project world position into the light's clip space
        float4 lightPosH = mul(mul(float4(WorldPos, 1.0f), Light.LightView), Light.LightProjection);
        float3 uvw = lightPosH.xyz / max(lightPosH.w, 1e-6f);

        // Convert to texture space
        float2 uv;
        uv.x = uvw.x * 0.5f + 0.5f;
        uv.y = uvw.y * -0.5f + 0.5f;

        // If outside the shadow map, treat as lit
        bool outside = (uv.x < 0.0f) || (uv.x > 1.0f) || (uv.y < 0.0f) || (uv.y > 1.0f) || (uvw.z < 0.0f) || (uvw.z > 1.0f);
        if (!outside)
        {
            // Receiver depth in clip-space [0,1] with bias
            float receiver = uvw.z - Light.ShadowBias;
            // Sample the stored depth from the shadow map array
            float stored = ShadowMapArray.SampleLevel(ShadowSampler, float3(uv, Light.ShadowMapIndex), 0).r;
            ShadowFactor = (receiver <= stored) ? 1.0f : 0.0f;
        }
        else
        {
            ShadowFactor = 1.0f;
        }
    }

    Result.Diffuse *= ShadowFactor;
    Result.Specular *= ShadowFactor;
    return Result;
}
#endif // LIGHTING_FUNCTIONS_HLSL



