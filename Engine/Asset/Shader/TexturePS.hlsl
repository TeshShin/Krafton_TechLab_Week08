#include "TextureVS.hlsl"

//--------------------------------------------------------------------------------------
// [UNIFIED FORWARD RENDERING] Light Data Structures
//--------------------------------------------------------------------------------------

// Ambient Light (Scene-wide global illumination)
struct FAmbientLight
{
    float3 Color;
    float Intensity;
};

// Light Constants (ConstantBuffer b10)
cbuffer LightConstants : register(b10)
{
    FAmbientLight GlobalAmbient;        // 16 bytes - Scene-wide ambient illumination
    uint UnifiedLightCount;             // 4 bytes  - Number of lights in StructuredBuffer
    float3 Padding;                     // 12 bytes - Alignment padding
};

//--------------------------------------------------------------------------------------
// Material Constants
//--------------------------------------------------------------------------------------

cbuffer MaterialConstants : register(b2)
{
    float4 Ka;		// Ambient color
    float4 Kd;		// Diffuse color
    float4 Ks;		// Specular color
    float Ns;		// Specular exponent
    float Ni;		// Index of refraction
    float D;		// Dissolve factor
    uint MaterialFlags;	// Which textures are available (bitfield)
    float Time;
};

Texture2D DiffuseTexture : register(t0);	// map_Kd
Texture2D AmbientTexture : register(t1);	// map_Ka
Texture2D SpecularTexture : register(t2);   // map_Ks
Texture2D ShininessTexture : register(t3);   // map_Ns
Texture2D AlphaTexture : register(t4);		// map_d
Texture2D BumpTexture : register(t5);		// map_bump

// [IMPORTANT] Light Type Enumeration - Must match C++ EDynamicLightType
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1
#define LIGHT_TYPE_SPOT        2
#define LIGHT_TYPE_RECT        3

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
    float4 Padding;             // 16 bytes - Alignment padding
};

StructuredBuffer<FUnifiedDynamicLight> DynamicLights : register(t6);

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_SHININESS_MAP (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

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

    float NdotL = saturate(dot(Normal, LightDir));
    Result.Diffuse = LightColor * NdotL;
    Result.Specular = float3(0, 0, 0);

    return Result;
}

//--------------------------------------------------------------------------------------
// [UNIFIED FORWARD RENDERING] Dynamic Light Calculation with Attenuation
//--------------------------------------------------------------------------------------

/**
 * @brief Calculate dynamic light contribution with attenuation using Blinn-Phong
 * @return Separated diffuse and specular contributions
 */
FLightingResult CalculateDynamicLight(FUnifiedDynamicLight Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower)
{
    FLightingResult Result;
    Result.Diffuse = float3(0, 0, 0);
    Result.Specular = float3(0, 0, 0);

    // Early exit for disabled/dummy lights
    if (Light.Intensity <= 0.0f)
        return Result;

    float3 LightDir;
    float Attenuation = Light.Intensity;

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
        Attenuation = saturate(1.0f - pow(NormalizedDist, Light.FalloffExponent));
        Attenuation *= Light.Intensity;

        // Spot-specific attenuation (if applicable)
        if (Light.LightType == LIGHT_TYPE_SPOT)
        {
            float Theta = dot(LightDir, -Light.Direction);
            float InnerCos = cos(Light.Param0);  // InnerConeAngle
            float OuterCos = cos(Light.Param1);  // OuterConeAngle

            if (Theta < OuterCos)
                return Result; // Outside cone

            // Smooth cone falloff
            float SpotAttenuation = saturate((Theta - OuterCos) / (InnerCos - OuterCos));
            Attenuation *= SpotAttenuation;
        }
    }

    // Apply attenuated light color
    float3 AttenuatedLightColor = Light.Color * Attenuation;

    // Calculate Blinn-Phong lighting (separated Diffuse and Specular)
    Result = CalculateBlinnPhongLighting(LightDir, Normal, ViewDir, AttenuatedLightColor, SpecularPower);

    return Result;
}

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;

    float2 UV = Input.Tex;

    // Base diffuse color
    float4 DiffuseColor = Kd;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        DiffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
    }

    // Ambient color for material
    float4 AmbientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        AmbientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }
	else if (MaterialFlags & HAS_DIFFUSE_MAP)
	{
		AmbientColor *= DiffuseTexture.Sample(SamplerWrap, UV);
	}

    // Specular color for material
    float4 SpecularColor = Ks;
    if (MaterialFlags & HAS_SPECULAR_MAP)
    {
        SpecularColor *= SpecularTexture.Sample(SamplerWrap, UV);
    }

    // [UNIFIED FORWARD RENDERING] Accumulate dynamic lights (single loop)
    float3 Normal = normalize(Input.WorldNormal);
    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero

    // Accumulate separated diffuse and specular contributions
    float3 TotalDiffuse = float3(0, 0, 0);
    float3 TotalSpecular = float3(0, 0, 0);

    for (uint i = 0; i < UnifiedLightCount; i++)
    {
        FLightingResult LightResult = CalculateDynamicLight(
            DynamicLights[i], Input.WorldPosition, Normal, ViewDir, SpecularPower);

        TotalDiffuse += LightResult.Diffuse;
        TotalSpecular += LightResult.Specular;
    }

	float4 FinalColor;

    // [PHYSICALLY CORRECT] Apply material properties separately
    // Ambient term: Ka * GlobalAmbient
    FinalColor.rgb = AmbientColor.rgb * GlobalAmbient.Color * GlobalAmbient.Intensity;

    // Diffuse term: Kd * Diffuse lighting
    FinalColor.rgb += DiffuseColor.rgb * TotalDiffuse;

    // Specular term: Ks * Specular lighting
    FinalColor.rgb += SpecularColor.rgb * TotalSpecular;

    // 3. 알파 값 처리 (기존 코드와 동일)
    FinalColor.a = D; // 기본 알파값
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D * alpha;
    }

    // Normal mapping
    // -----------------------
	float3 wsNormal = Input.WorldNormal;
	if (MaterialFlags & HAS_BUMP_MAP)
	{
        // Sample and unpack tangent-space normal (assumes XYZ in texture)
		float3 nTS = BumpTexture.Sample(SamplerWrap, UV).xyz * 2.0f - 1.0f;
		nTS = normalize(nTS);

        float3 N = normalize(Input.WorldNormal);
        float3 T = normalize(Input.WorldTangent);
        // Recompute B using handedness (stored in TangentSign)
        float3 B = normalize(cross(N, T)) * Input.TangentSign;

        float3x3 TBN = float3x3(T, B, N);
        wsNormal = normalize(mul(nTS, TBN));
    }
    else
    {
        wsNormal = normalize(Input.WorldNormal);
    }

    // 3. 알파 값 처리 (기존 코드와 동일)
    FinalColor.a = D; // 기본 알파값
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D * alpha;
    }

    Output.SceneColor = FinalColor;

    float3 EncodedNormal = wsNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
}
