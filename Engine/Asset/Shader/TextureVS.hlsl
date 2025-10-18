#include "LightingFunctions.hlsl"


// For Gouraud shading model
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
	FAmbientLight GlobalAmbient; // 16 bytes - Scene-wide ambient illumination
	uint UnifiedLightCount; // 4 bytes  - Number of lights in StructuredBuffer
	float3 Padding; // 12 bytes - Alignment padding
};

//--------------------------------------------------------------------------------------
// Material Constants
//--------------------------------------------------------------------------------------

cbuffer MaterialConstants : register(b2)
{
	float4 Ka; // Ambient color
	float4 Kd; // Diffuse color
	float4 Ks; // Specular color
	float Ns; // Specular exponent
	float Ni; // Index of refraction
	float D; // Dissolve factor
	uint MaterialFlags; // Which textures are available (bitfield)
	float Time;
};

Texture2D DiffuseTexture : register(t0); // map_Kd
Texture2D AmbientTexture : register(t1); // map_Ka
Texture2D SpecularTexture : register(t2); // map_Ks
Texture2D ShininessTexture : register(t3); // map_Ns
Texture2D AlphaTexture : register(t4); // map_d
Texture2D BumpTexture : register(t5); // map_bump

StructuredBuffer<FUnifiedDynamicLight> DynamicLights : register(t6);

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_SHININESS_MAP (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

//--------------------------------------------------------------------------------------

cbuffer Model : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 WorldInverseTranspose;
}

cbuffer Camera : register(b1)
{
	row_major float4x4 View;
	row_major float4x4 Projection;
	float3 ViewWorldLocation;    
	float NearClip;
	float FarClip;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPosition: TEXCOORD0;
	float3 WorldNormal : TEXCOORD1;
	float2 Tex : TEXCOORD2;
	float4 Color : COLOR;
};


PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
	Output.WorldNormal = normalize(mul(Input.Normal, (float3x3)WorldInverseTranspose));
	Output.Tex = Input.Tex;

#if defined(LIGHTING_MODEL_GOURAUD)
    float2 UV = Input.Tex;

    // Base diffuse color
    float4 DiffuseColor = Kd;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        DiffuseColor *= DiffuseTexture.SampleLevel(SamplerWrap, UV, 0);
    }

    // Ambient color for material
    float4 AmbientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        AmbientColor *= AmbientTexture.SampleLevel(SamplerWrap, UV, 0);
    }
	else if (MaterialFlags & HAS_DIFFUSE_MAP)
	{
		AmbientColor *= DiffuseTexture.SampleLevel(SamplerWrap, UV, 0);
	}

    // Specular color for material
    float4 SpecularColor = Ks;
    if (MaterialFlags & HAS_SPECULAR_MAP)
    {
        SpecularColor *= SpecularTexture.SampleLevel(SamplerWrap, UV, 0);
    }

	float3 wsNormal = Output.WorldNormal;
	
	float3 ViewDir = normalize(ViewWorldLocation - Output.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero
	
    // Accumulate separated diffuse and specular contributions
    float3 TotalDiffuse = float3(0, 0, 0);
    float3 TotalSpecular = float3(0, 0, 0);

    for (uint i = 0; i < UnifiedLightCount; i++)
    {
        FLightingResult LightResult = CalculateDynamicLight(
            DynamicLights[i], Output.WorldPosition, wsNormal, ViewDir, SpecularPower);

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
        float alpha = AlphaTexture.SampleLevel(SamplerWrap, UV, 0).r;
        FinalColor.a = D * alpha;
    }

    Output.Color = FinalColor;
#else
	Output.Color = Input.Color;
#endif
	
	return Output;
}
