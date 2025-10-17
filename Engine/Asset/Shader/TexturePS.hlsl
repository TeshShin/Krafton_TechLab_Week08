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

PS_OUTPUT mainPS(PS_INPUT Input) : SV_Target
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

    // Shininess for material
    float Shininess = Ns;
        
    // 조명 계산 호출
    float3 FinalLitColor = CalculateLighting(AmbientColor, DiffuseColor, SpecularColor, Shininess,
        Input.WorldPosition, Input.WorldNormal, ViewWorldLocation);

    float4 FinalColor;
    FinalColor.rgb = FinalLitColor;

    // 3. 알파 값 처리 (기존 코드와 동일)
    FinalColor.a = D; // 기본 알파값
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D * alpha;
    }

    // 4. 최종 출력값 설정
    // -----------------------
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