#include "TextureVS.hlsl"

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
Texture2D ShiniessTexture : register(t3); // map_Ns
Texture2D AlphaTexture : register(t4); // map_d
Texture2D BumpTexture : register(t5); // map_bump(normal map)

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_SHINIESS_MAP (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

cbuffer AmbientLightConstants : register(b3)
{
    float4 AmbientLightColor;
};

cbuffer CameraInverse : register(b1)
{
    row_major float4x4 ViewInverse; // 카메라 월드 위치 구하기 위함.
    row_major float4x4 ProjectionInverse;
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

// 이거 계산 후 마지막에 AmbientLightColor 계산해 더해줘야 함
float4 CaculateBlinnPhong(float3 ToLightDir, float3 ToEyeDir, float4 lightColor, float3 Normal, float4 DiffuseColor, float4 SpecularColor, float Shiniess)
{
    float3 halfVector = normalize(ToLightDir + ToEyeDir);
    
    float3 DiffuseTerm = DiffuseColor * saturate(dot(normalize(Normal), normalize(ToLightDir)));
    float3 SpecularTerm = SpecularColor * pow(saturate(dot(normalize(Normal), halfVector)), Shiniess);
    
    float4 FinalColor = float4((DiffuseTerm + SpecularTerm) * lightColor.rgb, 1.0f);
    
    return FinalColor;
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

    // Specular contribution
    float4 SpecularColor = Ks;
    if(MaterialFlags & HAS_SPECULAR_MAP)
    {
        SpecularColor *= SpecularTexture.Sample(SamplerWrap, UV);
    }
    
    // Alpha handling
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        FinalColor.a = D;
        FinalColor.a *= alpha;
    }
    
    // Specular exponent from texture
    float Shininess = Ns;
    if (MaterialFlags & HAS_SHINIESS_MAP)
    {
        float ShiniessTemp = ShiniessTexture.Sample(SamplerWrap, UV).r;
        Shininess = Shininess * ShiniessTemp * 128.0f; // Assuming texture stores exponent in [0,1] range
    }
    
    float Normal = Input.WorldNormal;
    if(MaterialFlags & HAS_BUMP_MAP)
    {
        float3 Normal = BumpTexture.Sample(SamplerWrap, UV).xyz * 2.0f - 1.0f;
        Normal = normalize(Normal);
    }

    float3 CameraWorldLocation = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), ViewInverse).xyz;
    float3 ToEyeDir = normalize(CameraWorldLocation - Input.WorldPosition);
    uint TempLightCount = 1;
    for (uint i = 0; i < TempLightCount; ++i)
    {
        // TODO: 이곳에 각자의 Light 계산식을 넣어서 테스트 해보세요 (ToLightDir, LightColor 구하기)
        
        float3 ToLightDir = float3(0.0f, 0.0f, 1.0f); // 임시값
        float4 LightColor = float4(1.0f, 1.0f, 1.0f, 1.0f); // 임시값
        FinalColor += CaculateBlinnPhong(ToLightDir, ToEyeDir, LightColor, Normal, DiffuseColor, SpecularColor, Shininess);
    }
    FinalColor.rgb += AmbientLightColor.rgb * AmbientColor.rgb; // 환경광 더해주기
    
    
    Output.SceneColor = FinalColor;
    float3 EncodedNormal = Normal * 0.5f + 0.5f;
    Output.NormalData = float4(EncodedNormal, 1.0f);
	
    return Output;
}
