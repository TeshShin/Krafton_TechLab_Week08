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

StructuredBuffer<FUnifiedDynamicLight> DynamicLights : register(t6);
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
	float3 TotalAmbient : COLOR0;
	float3 TotalDiffuse : COLOR1;
	float3 TotalSpecular : COLOR2;
};


PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
	Output.WorldNormal = normalize(mul(Input.Normal, (float3x3)WorldInverseTranspose));
	Output.Tex = Input.Tex;

	//#define LIGHTING_MODEL_GOURAUD // for coding
#if defined(LIGHTING_MODEL_GOURAUD)
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

	Output.TotalAmbient = GlobalAmbient.Color * GlobalAmbient.Intensity;
	Output.TotalDiffuse = TotalDiffuse;
	Output.TotalSpecular = TotalSpecular;
#else
	Output.TotalDiffuse = Input.Color;
#endif
	
	return Output;
}
