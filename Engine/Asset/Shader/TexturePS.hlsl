#include "TextureVS.hlsl"
#include "AmbientDirectionalLighting.hlsl"

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
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT  1
#define LIGHT_TYPE_RECT  2

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

// [UNIFIED FORWARD RENDERING] Calculate dynamic light contribution using Blinn-Phong
float3 CalculateDynamicLight(FUnifiedDynamicLight Light, float3 WorldPos, float3 Normal, float3 ViewDir, float SpecularPower)
{
    float3 LightVec = Light.Position - WorldPos;
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
    float Attenuation = saturate(1.0f - pow(NormalizedDist, Light.FalloffExponent));
    Attenuation *= Light.Intensity;

    // Spot-specific attenuation (if applicable)
    float SpotAttenuation = 1.0f;
    if (Light.LightType == LIGHT_TYPE_SPOT)
    {
        float Theta = dot(LightDir, -Light.Direction);
        float InnerCos = cos(Light.Param0);  // InnerConeAngle
        float OuterCos = cos(Light.Param1);  // OuterConeAngle

        if (Theta < OuterCos)
            return float3(0, 0, 0); // Outside cone

        // Smooth cone falloff
        SpotAttenuation = saturate((Theta - OuterCos) / (InnerCos - OuterCos));
    }

    // Combine diffuse + specular with all attenuation factors
    return Light.Color * (NdotL + Specular * 0.3f) * Attenuation * SpotAttenuation;
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
    }

    // Ambient color for material
    float4 AmbientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        AmbientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }

    // Specular color for material
    float4 SpecularColor = Ks;
    if (MaterialFlags & HAS_SPECULAR_MAP)
    {
        SpecularColor *= SpecularTexture.Sample(SamplerWrap, UV);
    }

    // Start with ambient lighting (Material ambient * Global ambient)
    float3 Lighting = AmbientColor.rgb * GlobalAmbient.Color * GlobalAmbient.Intensity;

    // [UNIFIED FORWARD RENDERING] Add dynamic lights (single loop)
    float3 Normal = normalize(Input.WorldNormal);
    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero

    // Get light count from StructuredBuffer
    uint NumDynamicLights, Stride;
    DynamicLights.GetDimensions(NumDynamicLights, Stride);

    // Accumulate all dynamic light contributions in a single loop
    for (uint i = 0; i < NumDynamicLights; i++)
    {
        Lighting += CalculateDynamicLight(DynamicLights[i], Input.WorldPosition, Normal, ViewDir, SpecularPower);
    }

    // Apply lighting to diffuse color
    FinalColor.rgb = DiffuseColor.rgb * Lighting;

    // 3. 알파 값 처리 (기존 코드와 동일)
    FinalColor.a = D; // 기본 알파값
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D * alpha;
    }

    // Normal mapping
    // -----------------------
    float3 wsNormal;
    if (MaterialFlags & HAS_BUMP_MAP)
    {
        // Sample and unpack tangent-space normal (assumes XYZ in texture)
        float3 nTS = BumpTexture.Sample(SamplerWrap, UV).xyz * 2.0f - 1.0f;
        nTS = normalize(nTS);

        // Derive TBN from screen-space derivatives (no vertex tangents required)
        float3 N = normalize(Input.WorldNormal);
        float3 dpdx = ddx(Input.WorldPosition);
        float3 dpdy = ddy(Input.WorldPosition);
        float2 dUVdx = ddx(UV);
        float2 dUVdy = ddy(UV);

        // Robust tangent reconstruction
        float3 T = dUVdy.y * dpdx - dUVdx.y * dpdy;
        // float3 B = -dUVdy.x * dpdx + dUVdx.x * dpdy;

        // Orthonormalize
        T = normalize(T - N * dot(N, T));
        float3 B_ortho = normalize(cross(N, T));

        float3x3 TBN = float3x3(T, B_ortho, N);
        wsNormal = normalize(mul(nTS, TBN));
    }
    else
    {
        wsNormal = normalize(Input.WorldNormal);
    }

    Output.SceneColor = FinalColor;

    float3 EncodedNormal = wsNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
}
