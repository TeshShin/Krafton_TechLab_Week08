#include "TextureVS.hlsl"

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
Texture2D SpecularTexture : register(t2);	// map_Ks
Texture2D NormalTexture : register(t3);		// map_Ns
Texture2D AlphaTexture : register(t4);		// map_d
Texture2D BumpTexture : register(t5);		// map_bump

// [IMPORTANT] Must match C++ LightData.h exactly (field names and order)
struct FPointLightData
{
    float3 LightLocation;       // 12 bytes
    float LightIntensity;       // 4 bytes
    float3 LightColor;          // 12 bytes
    float SourceRadius;         // 4 bytes - Light influence radius
    float LightFalloffExtent;   // 4 bytes - Falloff exponent (2.0 - 16.0)
    float3 Padding;             // 12 bytes - Alignment padding
};

struct FSpotLightData
{
    float3 LightLocation;       // 12 bytes
    float LightIntensity;       // 4 bytes
    float3 LightColor;          // 12 bytes
    float SourceRadius;         // 4 bytes - Light influence radius
    float3 LightDirection;      // 12 bytes
    float LightFalloffExtent;   // 4 bytes
    float InnerConeAngle;       // 4 bytes
    float OuterConeAngle;       // 4 bytes
    float2 Padding;             // 8 bytes - Alignment padding
};

StructuredBuffer<FPointLightData> PointLights : register(t6);
StructuredBuffer<FSpotLightData> SpotLights : register(t7);

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_NORMAL_MAP	 (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

// [FORWARD RENDERING] Calculate PointLight contribution using Blinn-Phong
float3 CalculatePointLight(FPointLightData Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower)
{
    float3 LightVec = Light.LightLocation - WorldPos;
    float Distance = length(LightVec);

    // Early exit if outside light influence radius
    if (Distance > Light.SourceRadius)
        return float3(0, 0, 0);

    float3 LightDir = normalize(LightVec);

    // Diffuse (Lambertian)
    float NdotL = saturate(dot(Normal, LightDir));

    // Specular (Blinn-Phong)
    float3 HalfVec = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(Normal, HalfVec));
    float Specular = pow(NdotH, SpecularPower);

    // Distance Attenuation (radial falloff)
    float NormalizedDist = Distance / Light.SourceRadius;
    float Attenuation = saturate(1.0f - pow(NormalizedDist, Light.LightFalloffExtent));
    Attenuation *= Light.LightIntensity;

    // Combine diffuse + specular
    return Light.LightColor * (NdotL + Specular * 0.3f) * Attenuation;
}

// [FORWARD RENDERING] Calculate SpotLight contribution using Blinn-Phong
float3 CalculateSpotLight(FSpotLightData Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower)
{
    float3 LightVec = Light.LightLocation - WorldPos;
    float Distance = length(LightVec);

    // Early exit if outside light influence radius
    if (Distance > Light.SourceRadius)
        return float3(0, 0, 0);

    float3 LightDir = normalize(LightVec);

    // Spot cone attenuation
    float Theta = dot(LightDir, -Light.LightDirection);
    float InnerCos = cos(Light.InnerConeAngle);
    float OuterCos = cos(Light.OuterConeAngle);

    if (Theta < OuterCos)
        return float3(0, 0, 0); // Outside cone

    // Smooth cone falloff
    float SpotIntensity = saturate((Theta - OuterCos) / (InnerCos - OuterCos));

    // Diffuse (Lambertian)
    float NdotL = saturate(dot(Normal, LightDir));

    // Specular (Blinn-Phong)
    float3 HalfVec = normalize(LightDir + ViewDir);
    float NdotH = saturate(dot(Normal, HalfVec));
    float Specular = pow(NdotH, SpecularPower);

    // Distance Attenuation
    float NormalizedDist = Distance / Light.SourceRadius;
    float Attenuation = saturate(1.0f - pow(NormalizedDist, Light.LightFalloffExtent));

    // Combine all factors
    return Light.LightColor * (NdotL + Specular * 0.3f) * Attenuation * SpotIntensity * Light.LightIntensity;
}

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;

    float4 FinalColor = float4(0.f, 0.f, 0.f, 1.f);
    float2 UV = Input.Tex;

    // Base diffuse color
    float4 DiffuseColor = Kd;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        DiffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
        FinalColor.a = DiffuseColor.a;
    }

    // Ambient contribution
    float4 AmbientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        AmbientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }

    // Start with ambient lighting
    float3 Lighting = AmbientColor.rgb;

    // [FORWARD RENDERING] Add dynamic lights
    float3 Normal = normalize(Input.WorldNormal);
    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero

    // Get light counts from StructuredBuffer
    uint NumPointLights, NumSpotLights, Stride;
    PointLights.GetDimensions(NumPointLights, Stride);
    SpotLights.GetDimensions(NumSpotLights, Stride);

    // Accumulate PointLight contributions
    for (uint i = 0; i < NumPointLights; i++)
    {
        Lighting += CalculatePointLight(PointLights[i], Input.WorldPosition, Normal, ViewDir, SpecularPower);
    }

    // Accumulate SpotLight contributions
    for (uint j = 0; j < NumSpotLights; j++)
    {
        Lighting += CalculateSpotLight(SpotLights[j], Input.WorldPosition, Normal, ViewDir, SpecularPower);
    }

    // Apply lighting to diffuse color
    FinalColor.rgb = DiffuseColor.rgb * Lighting;

    // Alpha handling
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D;
        FinalColor.a *= alpha;
    }

    Output.SceneColor = FinalColor;

    // Encode normal for G-Buffer (still needed for other passes)
    float3 EncodedNormal = Normal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
}
