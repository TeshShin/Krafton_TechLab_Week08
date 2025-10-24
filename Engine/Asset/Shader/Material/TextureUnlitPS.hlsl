#include "Asset/Shader/Lighting/LightingFunctions.hlsli"

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

Texture2D DiffuseTexture : register(t0); // map_Kd
Texture2D AlphaTexture : register(t4); // map_d

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_ALPHA_MAP	 (1 << 4)

struct PS_OUTPUT
{
	float4 SceneColor : SV_Target0;
	float4 NormalData : SV_Target1;
};

PS_OUTPUT mainPS(PS_INPUT Input) : SV_TARGET
{
	PS_OUTPUT Output;

	float2 UV = Input.Tex;

    // 1. Sample material properties
    // -----------------------
    // Base diffuse color
	float4 DiffuseColor = Kd;
	if (MaterialFlags & HAS_DIFFUSE_MAP)
	{
		DiffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
	}

    // 2. Combine lighting with material properties
    // -----------------------
	float4 FinalColor;
	FinalColor.rgb = float3(0, 0, 0);
	FinalColor.rgb += DiffuseColor.rgb;

    // 3. Alpha value processing
    // -----------------------
	FinalColor.a = D; // Base alpha value
	if (MaterialFlags & HAS_ALPHA_MAP)
	{
		float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
		FinalColor.a = D * alpha;
	}

    // 4. Output to render targets
    // -----------------------
	Output.SceneColor = FinalColor;
	Output.NormalData = float4(Input.WorldNormal, 1.0f);

	return Output;
}
