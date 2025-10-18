#include "LightingFunctions.hlsl"

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

StructuredBuffer<FUnifiedDynamicLight> DynamicLights : register(t6);

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_SHININESS_MAP (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

cbuffer Camera : register(b1)
{
	row_major float4x4 View;
	row_major float4x4 Projection;
	float3 ViewWorldLocation;
	float NearClip;
	float FarClip;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : TEXCOORD0;
	float3 WorldNormal : TEXCOORD1;
	float2 Tex : TEXCOORD2;
	float4 Color : COLOR;
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;

#if defined(LIGHTING_MODEL_GOURAUD)
	Output.SceneColor = Input.Color;
	float3 wsNormal = Input.WorldNormal;
#else
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

	// Normal mapping
    // -----------------------
	float3 wsNormal = Input.WorldNormal;
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
	
    float3 ViewDir = normalize(ViewWorldLocation - Input.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero
	
    // Accumulate separated diffuse and specular contributions
    float3 TotalDiffuse = float3(0, 0, 0);
    float3 TotalSpecular = float3(0, 0, 0);

    for (uint i = 0; i < UnifiedLightCount; i++)
    {
        FLightingResult LightResult = CalculateDynamicLight(
            DynamicLights[i], Input.WorldPosition, wsNormal, ViewDir, SpecularPower);

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

    Output.SceneColor = FinalColor;
#endif

    float3 EncodedNormal = wsNormal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);

    return Output;
}
