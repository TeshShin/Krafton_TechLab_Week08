#include "../Common/CommonConstants.hlsli"
#include "../Lighting/LightingFunctions.hlsli"

// Light Constants (ConstantBuffer b10)
cbuffer LightConstants : register(b0)
{
	uint UnifiedLightCount; // 4 bytes  - Number of lights in StructuredBuffer
	float3 Padding; // 12 bytes - Alignment padding
}; 

//--------------------------------------------------------------------------------------
// Material Constants
//--------------------------------------------------------------------------------------
 
cbuffer MaterialConstants : register(b1)
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

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
    float4 Tangent : TANGENT; // xyz: tangent, w: handedness
};

PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.WorldPosition = mul(float4(Input.Position, 1.0f), ModelWorld).xyz;
	Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), ModelWorld), View), Projection);
	Output.WorldNormal = normalize(mul(Input.Normal, (float3x3)ModelWorldInverseTranspose));
	Output.Tex = Input.Tex;
	  
//#define LIGHTING_MODEL_GOURAUD // for coding
#if defined(LIGHTING_MODEL_GOURAUD)
	float3 wsNormal = Output.WorldNormal;

	float3 ViewDir = normalize(ViewWorldLocation - Output.WorldPosition);
    float SpecularPower = max(Ns, 1.0f); // Prevent division by zero

    // Accumulate separated diffuse and specular contributions
    float3 TotalDiffuse = float3(0, 0, 0);
    float3 TotalSpecular = float3(0, 0, 0);
    float3 TotalAmbient = float3(0, 0, 0);

    for (uint i = 0; i < UnifiedLightCount; i++)
    {
        FLightingResult LightResult = CalculateDynamicLight(
            DynamicLights[i], Output.WorldPosition, wsNormal, ViewDir, SpecularPower);

        TotalDiffuse += LightResult.Diffuse;
        TotalSpecular += LightResult.Specular;
		TotalAmbient += LightResult.Ambient;
	}

	Output.TotalAmbient = TotalAmbient;
	Output.TotalDiffuse = TotalDiffuse;
	Output.TotalSpecular = TotalSpecular;
#else
	Output.TotalDiffuse = Input.Color.rgb;

	// pixel shader에서 normal map을 사용할 경우를 대비하여 World Tangent 계산
	float3 worldT = mul(Input.Tangent.xyz, (float3x3) ModelWorld);
	worldT = normalize(worldT - Output.WorldNormal * dot(Output.WorldNormal, worldT));
	Output.WorldTangent = worldT;
	Output.TangentSign = Input.Tangent.w;
#endif

	return Output;
}
