//--------------------------------------------------------------------------------------
// [LIGHTING FUNCTIONS] Common Lighting Calculations
// This file contains reusable lighting functions for both vertex and pixel shaders
//--------------------------------------------------------------------------------------

#ifndef LIGHTING_FUNCTIONS_HLSL
#define LIGHTING_FUNCTIONS_HLSL

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

FLightingResult CalculateDynamicLightWithShadows(FUnifiedDynamicLight Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower,
	Texture2DArray SpotShadowAtlas, StructuredBuffer<float4x4> SpotLightShadowMatrices, TextureCubeArray PointShadowAtlas, SamplerComparisonState ShadowSampler)
{
    FLightingResult Result = CalculateDynamicLight(Light, WorldPos, Normal, ViewDir, SpecularPower);

	float ShadowFactor = 1.0f;
	if (Light.bCastShadows > 0)
	{
		if (Light.LightType == LIGHT_TYPE_SPOT)
		{
			// column_major로 전달되어 곱셈 순서 반대
			float4 LightSpacePos = mul(SpotLightShadowMatrices[Light.ShadowMapIndex], float4(WorldPos, 1.0f));
			float3 ShadowCoords = LightSpacePos.xyz / LightSpacePos.w;

			ShadowCoords.x = ShadowCoords.x * 0.5f + 0.5f;
			ShadowCoords.y = ShadowCoords.y * -0.5f + 0.5f;

			float3 SampleCoords = float3(ShadowCoords.xy, Light.ShadowMapIndex);
			ShadowFactor = SpotShadowAtlas.SampleCmp(ShadowSampler, SampleCoords, ShadowCoords.z - Light.ShadowBias);
		}
		else if (Light.LightType == LIGHT_TYPE_POINT)
		{
			// 1. 픽셀에서 라이트까지의 '실제 거리' (선형 깊이) 계산
			float3 LightToPixelDir = WorldPos - Light.Position;
			float PixelDepth = length(LightToPixelDir);
			float3 SwizzledDir = float3(LightToPixelDir.y, LightToPixelDir.z, LightToPixelDir.x);

			float4 ShadowCoord = float4(SwizzledDir, Light.ShadowMapIndex);
			// 3. (선형 깊이) vs (섀도우맵에 저장된 선형 깊이) 비교
			ShadowFactor = PointShadowAtlas.SampleCmpLevelZero(
			   ShadowSampler,
			   ShadowCoord,
			   PixelDepth - Light.ShadowBias
			);
		}
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

#endif // LIGHTING_FUNCTIONS_HLSL
